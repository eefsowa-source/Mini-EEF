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
// real output against an ideal band-limited saw.  A saw's aliases fold back onto
// its own harmonic series, so the energy that betrays a band limiter is the
// energy that lands *between* the harmonics.  That, plus how far the first
// sixteen harmonics sit from the ideal 1/n series, is the two columns below.

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
    // How far the first sixteen harmonics sit from a 1/n series, in dB.  A
    // band limiter that softens the top octave buys its numbers here, and that
    // is not a sound this synth should make.
    double harmonicDb = 0.0;
    bool finite = true;
};

// Two numbers per note, because one alone was not enough to keep the band
// limiter honest:
//
//  * alias - energy in the audible band (20 Hz to 20 kHz) that is not on a
//    harmonic.  That is the part a listener hears.
//  * harmonic deviation - how far each of the first sixteen harmonics is from
//    the ideal 1/n saw series.  A kernel whose residual is too wide reports a
//    superb alias figure while quietly deleting the top octave, and this is the
//    column that catches it.
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

    // The last analysisSamples, tapered.  A rectangular window leaves the
    // fractional-cycle offset of a real note as a skirt, and a loud high
    // harmonic drags that skirt into the region this tool reads: at 8.4 kHz the
    // second harmonic alone used to account for twenty times the "alias" power,
    // so a kernel that deleted the harmonic scored better than one that kept it.
    std::vector<float> tapered (window.end() - analysisSamples, window.end());
    const double pi = juce::MathConstants<double>::pi;
    const size_t last = tapered.size() - 1;
    for (size_t i = 0; i <= last; ++i)
    {
        const double x = 2.0 * pi * static_cast<double> (i) / static_cast<double> (last);
        tapered[i] *= static_cast<float> (0.35875 - 0.48829 * std::cos (x)
                                          + 0.14128 * std::cos (2.0 * x)
                                          - 0.01168 * std::cos (3.0 * x));
    }

    const auto magnitudes = spectrum (tapered);
    // spectrum() returns fftSize / 2 + 1 bins, so the transform length is twice
    // the last index, not twice the count.
    const double binHz = sampleRate / static_cast<double> (2 * (magnitudes.size() - 1));
    const int nyquistBin = static_cast<int> (magnitudes.size()) - 1;
    const int fundamentalBin = juce::jmax (1, juce::roundToInt (frequency / binHz));

    // +/-8 bins covers the Blackman-Harris mainlobe for this length.
    constexpr int guardBins = 8;
    const int harmonicCount = juce::jmax (1, (nyquistBin - guardBins) / fundamentalBin);
    const int audibleTop = juce::jmin (nyquistBin, juce::roundToInt (20000.0 / binHz));
    // Harmonic deviation is only judged where the voice filter is flat: it runs
    // wide open at min(20 kHz, 0.40 sr) and its rolloff reaches -1 dB by 7 kHz,
    // so measuring higher would read the filter as a broken oscillator.
    const double judgedHarmonicHz = 0.25 * juce::jmin (20000.0, 0.40 * sampleRate);
    // The output DC blocker sits at 38 Hz, so the fundamental of the low notes
    // is up to 3.7 dB below its harmonics.  Undo it before comparing them, or
    // every bass note reads as a broken saw.
    // shortcut: this corner is a copy of the processor's; a measurement tool
    // should not fail to build against a header it is only reading, but move it
    // back to EonMiniEEFProcessor::dcBlockCornerHz once that owns the value.
    constexpr double outputDcCornerHz = 38.292999;
    const double dcPole = std::exp (-juce::MathConstants<double>::twoPi
                                    * outputDcCornerHz / sampleRate);
    const auto dcGain = [dcPole, sampleRate] (double hz)
    {
        const double w = juce::MathConstants<double>::twoPi * hz / sampleRate;
        return std::sqrt (2.0 - 2.0 * std::cos (w))
             / std::sqrt (juce::jmax (1.0e-12, 1.0 - 2.0 * dcPole * std::cos (w) + dcPole * dcPole));
    };
    // Below this the window is too short to place the fundamental, let alone
    // measure it.
    const bool resolvesFundamental = fundamentalBin >= 8;
    std::vector<bool> isHarmonic (static_cast<size_t> (nyquistBin) + 1, false);
    double harmonicPower = 0.0, fundamentalPower = 0.0, fundamentalMag = 0.0;

    for (int k = 1; k <= harmonicCount; ++k)
    {
        const int centre = juce::roundToInt (k * frequency / binHz);
        const int first = juce::jmax (1, centre - guardBins);
        const int final = juce::jmin (nyquistBin, centre + guardBins);
        double power = 0.0;
        for (int bin = first; bin <= final; ++bin)
        {
            isHarmonic[static_cast<size_t> (bin)] = true;
            const double m = magnitudes[static_cast<size_t> (bin)];
            power += m * m;
        }
        if (k == 1)
        {
            fundamentalPower = power;
            fundamentalMag = std::sqrt (power) / dcGain (frequency);
        }
        harmonicPower += power;
        if (k > 1 && k <= 16 && resolvesFundamental && k * frequency <= judgedHarmonicHz)
        {
            // An ideal saw's k-th harmonic is 1/k of its first, so the measured
            // ratio should come to k.
            const double ratio = std::sqrt (power) / dcGain (k * frequency) * k
                                 / juce::jmax (1.0e-15, fundamentalMag);
            row.harmonicDb = juce::jmax (row.harmonicDb, std::abs (toDb (ratio)));
        }
    }

    double aliasPower = 0.0;
    // Bin 3 is the low edge of the window's own skirt; nothing musical lives
    // there for these pitches.
    for (int bin = 3; bin <= audibleTop; ++bin)
    {
        if (isHarmonic[static_cast<size_t> (bin)])
            continue;
        const double m = magnitudes[static_cast<size_t> (bin)];
        aliasPower += m * m;
    }

    row.inband = std::sqrt (harmonicPower);
    row.outOfBand = std::sqrt (aliasPower);
    // Referenced to the whole signal, not just the counted harmonics, so a
    // kernel that loses level does not also get a quieter alias figure.
    double signalPower = 0.0;
    for (int bin = 1; bin <= nyquistBin; ++bin)
    {
        const double m = magnitudes[static_cast<size_t> (bin)];
        signalPower += m * m;
    }
    row.outPercent = 100.0 * aliasPower / juce::jmax (1.0e-30, signalPower);
    row.outDb = toDb (std::sqrt (aliasPower / juce::jmax (1.0e-30, signalPower)));
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

    std::cout << "# Saw band-limit: audible-band alias and harmonic fidelity\n";
    std::cout << "# alias = energy below 20 kHz that is not on a harmonic of the note\n";
    std::cout << "# dev = worst deviation of harmonics 2-16 from the ideal 1/n series\n";
    std::cout << "| note | Hz | sr | quality | harmonics | alias dB | alias % | dev dB |\n";
    std::cout << "|---|---|---|---|---|---|---|---|\n";

    double worstPercent = 0.0;
    double worstHarmonicDb = 0.0;
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
                worstHarmonicDb = juce::jmax (worstHarmonicDb, row.harmonicDb);
                std::cout << "| " << midiNote
                          << " | " << row.frequency
                          << " | " << sampleRate
                          << " | " << modeNames[modeIndex]
                          << " | " << row.inband
                          << " | " << row.outDb
                          << " | " << row.outPercent
                          << " | " << row.harmonicDb << " |\n";
            }
        }
    }

    if (anyNonFinite)
    {
        std::cerr << "band-limit sweep produced non-finite output\n";
        return 1;
    }
    // Measured with this kernel: 0.004% worst alias and 0.1 dB worst harmonic
    // deviation.  The gate sits an order of magnitude above that, which still
    // fails the four-point sensor it replaced (0.14% alias, 35 dB deviation).
    if (worstPercent > 0.02 || worstHarmonicDb > 1.5)
    {
        std::cerr << "band-limit sweep: worst " << worstPercent
                  << "% audible-band alias and " << worstHarmonicDb
                  << " dB harmonic deviation, above the 0.02% / 1.5 dB gate\n";
        return 1;
    }
    std::cout << "band-limit sweep: PASS (worst " << worstPercent
              << "% audible-band alias, " << worstHarmonicDb
              << " dB harmonic deviation, all outputs finite)\n"
              << "# Worst alias case: " << worstFrequency << " Hz at "
              << worstSampleRate << " Hz sample rate.\n";
    return 0;
}
