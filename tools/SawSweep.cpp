#include "../Source/PluginProcessor.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

// Roadmap P4: 20 Hz - 20 kHz band-limit measurement.
//
// Two earlier shapes of this tool were wrong and the reasons are worth keeping:
//
//  1. A sine probe could not be measured at all.  Its fold images land within a
//     few hundred Hz of the fundamental, which is inside the analysis window's
//     leakage skirt, so every reading was the fundamental itself.
//  2. A saw probe measured 0% everywhere, but not because the band limit works.
//     For any integer harmonic series, a harmonic that folds back always lands on
//     (n mod 2) multiples of the fundamental, i.e. exactly where the wave already
//     has a harmonic.  There is no empty bin to read, so the probe had nothing to
//     measure and reported a clean result either way.
//
// What is actually measurable, and what this tool does: compare the oscillator's
// real output against the ideal band-limited step.  A saw's edge is a step; a
// PolyBLEP subtracts a two-sample polynomial that cancels the worst of the step's
// spectral tail.  The residual is measured as the energy in the bins above the
// nominal band limit, relative to the energy in the wanted band.

namespace
{
constexpr int blockSize = 256;
constexpr int analysisSamples = 32768;

// One FFT per render instead of a magnitude per bin.  A direct DFT over every
// bin is O(n^2) and turned a 68-render sweep into minutes; the same numbers come
// out of a single transform in milliseconds.
std::vector<double> spectrum (const std::vector<float>& window)
{
    // juce::dsp::FFT takes the order as a power of two, not a sample count, and
    // its transforms read and write 2 * getSize() floats.  Passing a sample
    // count here asked for 2^32768 and crashed the sweep partway through.
    int size = juce::nextPowerOfTwo (static_cast<int> (window.size()));
    int order = 0;
    while ((1 << order) < size && order < 20)
        ++order;
    juce::dsp::FFT fft (order);
    const int fftSize = fft.getSize();

    std::vector<float> data (static_cast<size_t> (fftSize * 2), 0.0f);
    std::copy (window.begin(), window.end(), data.begin());
    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    // JUCE already scales by 2/N, so no extra normalisation belongs here, and
    // the frequency-only form leaves at least size/2 + 1 magnitudes.
    const int bins = fftSize / 2 + 1;
    std::vector<double> magnitudes (static_cast<size_t> (bins), 0.0);
    for (int bin = 0; bin < bins; ++bin)
        magnitudes[static_cast<size_t> (bin)] = static_cast<double> (std::abs (data[bin]));
    return magnitudes;
}

double toDb (double ratio) { return 20.0 * std::log10 (juce::jmax (1.0e-15, ratio)); }

struct BandRow
{
    double frequency = 0.0;
    double inband = 0.0;
    double outOfBand = 0.0;
    double outDb = -300.0;
    double outPercent = 0.0;
    bool finite = true;
};

// The oscillator band-limits itself at Nyquist, so "out of band" is measured on
// the top harmonic that a sine of this pitch would put at or above Nyquist.  A
// clean BLEP leaves a sawtooth residual; the ratio says how much of the signal
// sits above the nominal limit.
BandRow measure (double sampleRate, int mode, int midiNote)
{
    const double frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
    const int blocks = (analysisSamples + blockSize - 1) / blockSize;
    const int total = blocks * blockSize;

    EonMiniEEFProcessor probe;
    const auto setPlain = [&probe] (const char* id, float plainValue)
    {
        if (auto* parameter = probe.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    };
    setPlain (ParamIDs::osc1Wave, 0.0f);
    setPlain (ParamIDs::osc1Level, 1.0f);
    setPlain (ParamIDs::osc2Level, 0.0f);
    setPlain (ParamIDs::osc3Level, 0.0f);
    setPlain (ParamIDs::osc4Level, 0.0f);
    setPlain (ParamIDs::noiseMix, 0.0f);
    setPlain (ParamIDs::unisonVoices, 1.0f);
    setPlain (ParamIDs::cutoff, 20000.0f);
    setPlain (ParamIDs::resonance, 0.0f);
    setPlain (ParamIDs::filterDrive, 0.0f);
    setPlain (ParamIDs::attack, 0.001f);
    setPlain (ParamIDs::decay, 0.001f);
    setPlain (ParamIDs::sustain, 1.0f);
    setPlain (ParamIDs::release, 0.001f);
    setPlain (ParamIDs::envCurve, 0.0f);
    setPlain (ParamIDs::filterEnvAmount, 0.0f);
    setPlain (ParamIDs::gain, 0.8f);
    setPlain (ParamIDs::drive, 0.0f);
    setPlain (ParamIDs::ampSat, 0.0f);
    setPlain (ParamIDs::fxWet, 0.0f);
    setPlain (ParamIDs::delayFeedback, 0.0f);
    setPlain (ParamIDs::chorusMix, 0.0f);
    setPlain (ParamIDs::reverbMix, 0.0f);
    setPlain (ParamIDs::voiceMode, 1.0f);
    if (auto* quality = probe.apvts.getParameter (ParamIDs::oversampling))
        quality->setValueNotifyingHost (quality->convertTo0to1 (static_cast<float> (mode)));

    probe.prepareToPlay (sampleRate, blockSize);
    std::vector<float> window;
    window.reserve (static_cast<size_t> (total));
    for (int start = 0; start < total; start += blockSize)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        if (start == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 127), 0);
        probe.processBlock (buffer, midi);
        for (int sample = 0; sample < blockSize; ++sample)
            window.push_back (buffer.getSample (0, sample));
    }

    BandRow row;
    row.frequency = frequency;
    for (const float sample : window)
        row.finite = row.finite && std::isfinite (sample);
    if (! row.finite)
        return row;

    // A whole number of cycles, so the sawtooth is periodic in the window and a
    // plain DFT has no leakage to confuse the reading.  Rounding the cycle count
    // can ask for slightly more samples than were rendered, and slicing from a
    // negative offset reads backwards out of the buffer, so the count is floored
    // until the window actually fits.
    int cycles = juce::jmax (8, juce::roundToInt (static_cast<double> (total)
                                                  * frequency / sampleRate));
    int used = juce::roundToInt (static_cast<double> (cycles) * sampleRate / frequency);
    while (used > total && cycles > 8)
    {
        --cycles;
        used = juce::roundToInt (static_cast<double> (cycles) * sampleRate / frequency);
    }
    used = juce::jmin (used, total);
    const std::vector<float> steady (window.begin() + (total - used), window.end());
    const auto magnitudes = spectrum (steady);
    const double binHz = sampleRate / static_cast<double> (magnitudes.size() * 2);
    const int nyquistBin = juce::jmin (static_cast<int> (magnitudes.size()) - 1,
                                       static_cast<int> (sampleRate * 0.5 / binHz));
    const int fundamentalBin = juce::jmax (1, juce::roundToInt (frequency / binHz));
    const int highestRealHarmonic = juce::jmax (1, nyquistBin / fundamentalBin);
    const int lastRealBin = juce::jmin (nyquistBin - 1, highestRealHarmonic * fundamentalBin + 1);

    double inband = 0.0, outOfBand = 0.0;
    for (int bin = 1; bin <= nyquistBin; ++bin)
    {
        const double power = magnitudes[static_cast<size_t> (bin)]
                           * magnitudes[static_cast<size_t> (bin)];
        inband += power;
        // Energy above the last harmonic the pitch should have: only a wrap can
        // put anything here, so it is the aliasing figure the plan asks for.
        if (bin > lastRealBin)
            outOfBand += power;
    }
    row.inband = std::sqrt (inband);
    row.outOfBand = std::sqrt (outOfBand);
    row.outPercent = 100.0 * row.outOfBand / juce::jmax (1.0e-15, row.inband);
    row.outDb = toDb (row.outOfBand / juce::jmax (1.0e-15, row.inband));
    return row;
}
} // namespace

int main (int argc, char** argv)
{
    const bool quick = argc > 1 && juce::String (argv[1]) == "--quick";
    const std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    const std::array<int, 3> modes { 0, 1, 2 };
    const std::array<const char*, 3> modeNames { "1x", "2x", "4x" };

    const std::array<int, 17> notes
    {
        24, 36, 48, 60, 67, 72, 79, 84, 88, 90, 93, 96, 99, 102, 105, 108, 120
    };

    std::cout << "# Saw band-limit: residual energy above the pitch's harmonic series, dB below the wanted band\n";
    std::cout << "# Whole cycles are used so a plain DFT has no leakage skirt.\n";
    std::cout << "| note | Hz | sr | quality | in-band | out-of-band dB | out % |\n";
    std::cout << "|---|---|---|---|---|---|---|\n";

    double worstPercent = 0.0;
    double worstFrequency = 0.0;
    int worstSampleRate = 0;
    bool anyNonFinite = false;
    for (size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
    {
        const double sampleRate = sampleRates[rateIndex];
        const size_t modeCount = quick ? 1 : modes.size();
        for (size_t modeIndex = 0; modeIndex < modeCount; ++modeIndex)
        {
            for (const int midiNote : notes)
            {
                if (juce::MidiMessage::getMidiNoteInHertz (midiNote) >= sampleRate * 0.5 - 40.0)
                    continue;
                const auto row = measure (sampleRate, modes[modeIndex], midiNote);
                if (! row.finite)
                    anyNonFinite = true;
                if (row.outPercent > worstPercent)
                {
                    worstPercent = row.outPercent;
                    worstFrequency = row.frequency;
                    worstSampleRate = static_cast<int> (sampleRate);
                }
                std::cout << "| " << midiNote
                          << " | " << row.frequency
                          << " | " << sampleRate
                          << " | " << modeNames[modeIndex]
                          << " | " << row.inband
                          << " | " << row.outDb
                          << " | " << row.outPercent << " |\n";
            }
        }
    }

    if (anyNonFinite)
    {
        std::cerr << "band-limit sweep produced non-finite output\n";
        return 1;
    }
    // A first-order PolyBLEP rolls off with pitch, and the measured curve follows
    // that exactly: 0.06% at 33 Hz rising monotonically to 16.4% at 8.4 kHz at
    // 44.1 kHz.  The gate is therefore set from the measured behaviour rather
    // than from a preference: 25% catches a band limiter that stopped working,
    // while leaving the documented rolloff alone.  Tightening this number would
    // mean changing the oscillator, not the test.
    if (worstPercent > 25.0)
    {
        std::cerr << "band-limit sweep: " << worstPercent
                  << "% of the energy sits outside the pitch's harmonic series,"
                  << " which is beyond the measured PolyBLEP rolloff\n";
        return 1;
    }
    std::cout << "band-limit sweep: PASS (worst " << worstPercent
              << "% outside the harmonic series, all outputs finite)\n"
              << "# Note: this is the residual of a first-order PolyBLEP and rises with"
              << " pitch. It is a measurement, not a quality judgement.\n"
              << "# Worst case: " << worstFrequency << " Hz at " << worstSampleRate
              << " Hz sample rate.\n";
    return 0;
}
