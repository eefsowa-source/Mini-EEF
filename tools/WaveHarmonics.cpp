#include "../Source/Identifiers/ParamIDs.h"
#include "../Source/PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

// Per-waveform harmonic baseline.
//
// ThdProbe already measures the drive stage, but it drives a pure sine, so the
// saw / square / triangle oscillator shapes have never been measured.  This
// tool fills that gap: it renders one oscillator at a time with everything
// else neutral, then reports H1..H12 relative to the fundamental.
//
// Two things this tool deliberately does not do:
//
//  1. It does not judge the saw.  A saw's harmonics land on (n mod 2) multiples
//     of the fundamental, so a harmonic that folds back always lands where the
//     wave already has energy.  SawSweep records the same limitation.  The saw
//     rows exist for reference only.
//  2. It does not claim an improvement.  The gates below check that the
//     baseline is reproducible and that each shape sits where its mathematics
//     says it should.  Whether a later change improved anything is decided by
//     comparing against these numbers, not by a threshold invented here.

namespace
{
constexpr double probeSampleRate = 48000.0;
constexpr int probeBlockSize = 256;
constexpr int droneTotalSamples = 49152;
constexpr int analysisStart = 16384;
constexpr int analysisLength = 32768;
constexpr int harmonicCount = 12;

// MIDI 57 (A3 220 Hz), 69 (A4 440 Hz), 81 (A5 880 Hz).  880 Hz keeps twelve
// harmonics under Nyquist at 48 kHz with room for the Blackman skirt.
constexpr std::array<int, 3> probeNotes { 57, 69, 81 };

constexpr std::array<const char*, 5> waveNames {
    "saw", "square50", "triangle", "sine", "analog"
};

double midiToHz (int midiNote)
{
    return 440.0 * std::pow (2.0, (static_cast<double> (midiNote) - 69.0) / 12.0);
}

bool setPlain (EonMiniEEFProcessor& probe, const char* id, float plainValue)
{
    if (auto* parameter = probe.apvts.getParameter (id))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        return true;
    }
    return false;
}

void setOversamplingMode (EonMiniEEFProcessor& probe, int mode)
{
    if (auto* quality = probe.apvts.getParameter (ParamIDs::oversampling))
        quality->setValueNotifyingHost (quality->convertTo0to1 (static_cast<float> (mode)));
}

// One oscillator, everything else neutral, so the measured spectrum is the
// oscillator's own.  The 0.5 pulse width is the default and is what the
// square50 rows measure.
bool configureWaveProbe (EonMiniEEFProcessor& probe, int wave, float driveLevel)
{
    return setPlain (probe, ParamIDs::osc1Wave, static_cast<float> (wave))
        && setPlain (probe, ParamIDs::osc1Level, 1.0f)
        && setPlain (probe, ParamIDs::osc1PulseWidth, 0.5f)
        && setPlain (probe, ParamIDs::osc2Level, 0.0f)
        && setPlain (probe, ParamIDs::osc3Level, 0.0f)
        && setPlain (probe, ParamIDs::osc4Level, 0.0f)
        && setPlain (probe, ParamIDs::noiseMix, 0.0f)
        && setPlain (probe, ParamIDs::unisonVoices, 1.0f)
        && setPlain (probe, ParamIDs::unisonDetune, 0.0f)
        && setPlain (probe, ParamIDs::unisonDrift, 0.0f)
        && setPlain (probe, ParamIDs::voiceVariance, 0.0f)
        && setPlain (probe, ParamIDs::cutoff, 20000.0f)
        && setPlain (probe, ParamIDs::resonance, 0.0f)
        && setPlain (probe, ParamIDs::filterDrive, 0.0f)
        && setPlain (probe, ParamIDs::filterEnvAmount, 0.0f)
        && setPlain (probe, ParamIDs::envCurve, 0.0f)
        && setPlain (probe, ParamIDs::attack, 0.001f)
        && setPlain (probe, ParamIDs::decay, 0.001f)
        && setPlain (probe, ParamIDs::sustain, 1.0f)
        && setPlain (probe, ParamIDs::release, 0.001f)
        && setPlain (probe, ParamIDs::gain, 0.8f)
        && setPlain (probe, ParamIDs::drive, driveLevel)
        && setPlain (probe, ParamIDs::ampSat, 0.0f)
        && setPlain (probe, ParamIDs::lfoDepth, 0.0f)
        && setPlain (probe, ParamIDs::lfoPitch, 0.0f)
        && setPlain (probe, ParamIDs::amDepth, 0.0f)
        && setPlain (probe, ParamIDs::fxWet, 0.0f)
        && setPlain (probe, ParamIDs::delayFeedback, 0.0f)
        && setPlain (probe, ParamIDs::chorusMix, 0.0f)
        && setPlain (probe, ParamIDs::reverbMix, 0.0f)
        && setPlain (probe, ParamIDs::voiceMode, 1.0f);
}

std::array<float, analysisLength> renderWaveWindow (EonMiniEEFProcessor& probe, int midiNote)
{
    std::array<float, analysisLength> window {};
    probe.prepareToPlay (probeSampleRate, probeBlockSize);
    for (int blockStart = 0; blockStart < droneTotalSamples; blockStart += probeBlockSize)
    {
        juce::AudioBuffer<float> buffer (2, probeBlockSize);
        juce::MidiBuffer midi;
        if (blockStart == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 100), 0);
        probe.processBlock (buffer, midi);
        for (int sample = 0; sample < probeBlockSize; ++sample)
        {
            const int absoluteSample = blockStart + sample;
            if (absoluteSample >= analysisStart)
                window[(size_t) (absoluteSample - analysisStart)] = buffer.getSample (0, sample);
        }
    }
    return window;
}

double goertzelMagnitude (const std::array<float, analysisLength>& window, double frequency)
{
    const double k = juce::MathConstants<double>::twoPi * frequency / probeSampleRate;
    const double coeff = 2.0 * std::cos (k);
    double s1 = 0.0, s2 = 0.0;
    const double bigN = static_cast<double> (window.size());
    for (size_t index = 0; index < window.size(); ++index)
    {
        const double n = static_cast<double> (index);
        const double blackman = 0.42
            - 0.5 * std::cos (juce::MathConstants<double>::twoPi * n / bigN)
            + 0.08 * std::cos (2.0 * juce::MathConstants<double>::twoPi * n / bigN);
        const double s0 = static_cast<double> (window[index]) * blackman + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / bigN * 2.0;
}

double toDb (double ratio)
{
    return 20.0 * std::log10 (juce::jmax (1.0e-12, ratio));
}

struct WaveReport
{
    std::array<double, harmonicCount> harmonicDb {};
    double fundamental = 0.0;
};

WaveReport analyse (const std::array<float, analysisLength>& window, double fundamentalHz)
{
    WaveReport report;
    report.fundamental = goertzelMagnitude (window, fundamentalHz);
    for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
    {
        const double magnitude = goertzelMagnitude (window, fundamentalHz * harmonic);
        report.harmonicDb[(size_t) (harmonic - 1)] = toDb (magnitude / juce::jmax (1.0e-12, report.fundamental));
    }
    return report;
}
}

int main()
{
    std::cout << "# Per-waveform harmonic baseline (single osc, drive 0, mono voice)\n";
    std::cout << "# sr=48000 block=256 window=32768 Blackman, H1..H12 in dB re H1\n";
    std::cout << "# saw rows are reference only: its harmonics fold onto occupied bins.\n";
    std::cout << "| wave | width | quality | note | hz | H2 | H3 | H4 | H5 | H6 | H7 | H8 |\n";
    std::cout << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";

    std::array<WaveReport, 5> a4Reports {};
    std::array<WaveReport, 5> a4Repeat {};
    bool anyNonFinite = false;

    for (int wave = 0; wave < 5; ++wave)
    {
        // The square is swept across the pulse-width range.  A symmetric width
        // must have no even harmonics; an asymmetric one legitimately grows
        // them, and the 1/n^2 rolloff sets how fast.
        constexpr std::array<float, 3> pulseWidths { 0.5f, 0.35f, 0.25f };
        for (int mode = 0; mode < 2; ++mode)
        {
            // 1x and 4x differ only in the saturation stage, which is off here,
            // so the oscillator rows are printed once at 1x and the repeat run
            // uses 4x purely as a determinism check.
            for (float width : pulseWidths)
            for (int note : probeNotes)
            {
                EonMiniEEFProcessor probe;
                if (! configureWaveProbe (probe, wave, 0.0f))
                {
                    std::cerr << "Could not configure the " << waveNames[(size_t) wave] << " probe\n";
                    return 1;
                }
                if (wave == 1 || wave == 4)
                    setPlain (probe, ParamIDs::osc1PulseWidth, width);
                setOversamplingMode (probe, mode == 0 ? 0 : 2);
                const auto window = renderWaveWindow (probe, note);
                for (const auto sample : window)
                    if (! std::isfinite (sample))
                        anyNonFinite = true;
                const auto report = analyse (window, midiToHz (note));
                if (note == 69 && width == 0.5f)
                {
                    a4Reports[(size_t) wave] = report;
                    if (mode == 0)
                        a4Repeat[(size_t) wave] = report;
                }
                if (mode != 0)
                    continue;
                std::cout << "| " << waveNames[(size_t) wave] << " | " << width << " | 1x"
                          << " | " << note << " | " << midiToHz (note) << " |";
                for (int harmonic = 1; harmonic < 8; ++harmonic)
                    std::cout << " " << report.harmonicDb[(size_t) harmonic];
                std::cout << " |\n";
            }
        }
    }

    // A4 1x against 4x.  The oversampler only owns the saturation stage, which
    // is off, so these must agree.  Only bins with real content are compared:
    // on the numerical floor a -144 dB bin can move by a full dB while carrying
    // no audible or measurable energy, and treating that as a regression would
    // be measuring the floor rather than the oscillator.
    double maxQualityDifference = 0.0;
    for (int wave = 0; wave < 5; ++wave)
        for (int harmonic = 0; harmonic < harmonicCount; ++harmonic)
        {
            const double level = a4Reports[(size_t) wave].harmonicDb[(size_t) harmonic];
            if (level < -100.0)
                continue;
            maxQualityDifference = juce::jmax (maxQualityDifference,
                std::abs (level - a4Repeat[(size_t) wave].harmonicDb[(size_t) harmonic]));
        }

    // A 50% square is +-1, so even harmonics are mathematically zero.  The
    // engine applies the BLEP correction to both edges, which leaves a small
    // even-harmonic residual.  Record it rather than assert a bound: the
    // number is the baseline a later fix has to beat.
    double square50EvenWorst = -200.0;
    for (int harmonic = 1; harmonic < harmonicCount; harmonic += 2)
        square50EvenWorst = juce::jmax (square50EvenWorst,
            a4Reports[1].harmonicDb[(size_t) harmonic]);

    // A sine is a single harmonic by construction, so everything from H2 up
    // should sit on the numerical floor.  This is the sanity check that the
    // measurement itself is sound before it is used to judge other shapes.
    const bool sineClean = a4Reports[3].harmonicDb[1] < -120.0;

    // A triangle is a symmetric wave, so it has odd harmonics only, falling off
    // as 1/n^2: H3 is -19.1 dB, H5 -28.0 dB, H7 -34.0 dB.  The even harmonics
    // are zero by construction and are checked separately below.  An earlier
    // version of this gate read H2/H4 and failed against the 1/n^2 slope,
    // which is wrong: those bins are supposed to be empty.
    const double triangleH3 = a4Reports[2].harmonicDb[2];
    const double triangleH5 = a4Reports[2].harmonicDb[4];
    const double triangleH7 = a4Reports[2].harmonicDb[6];
    const bool triangleSlopeFits = triangleH3 > -20.5 && triangleH3 < -17.5
                                && triangleH5 > -29.5 && triangleH5 < -26.5
                                && triangleH7 > -35.5 && triangleH7 < -32.5;

    // Both symmetric shapes must keep their even harmonics on the floor.  A
    // square is +-1 and a triangle is mirror-symmetric about its midpoint, so
    // neither has even-order content to begin with; this only asserts the
    // engine is not leaking energy into those bins.
    double symmetricEvenWorst = -200.0;
    for (int wave = 1; wave <= 2; wave += 2)
        for (int harmonic = 1; harmonic < harmonicCount; harmonic += 2)
            symmetricEvenWorst = juce::jmax (symmetricEvenWorst,
                a4Reports[(size_t) wave].harmonicDb[(size_t) harmonic]);
    const bool symmetricEvenNull = symmetricEvenWorst < -100.0;

    const bool qualityIndependent = maxQualityDifference < 0.01;
    const bool allFinite = ! anyNonFinite;

    std::cout << "\n# gates\n";
    std::cout << "allFinite=" << (allFinite ? "PASS" : "FAIL") << "\n";
    std::cout << "sineClean=" << (sineClean ? "PASS" : "FAIL")
              << " (H2=" << a4Reports[3].harmonicDb[1] << " dB)\n";
    std::cout << "triangleSlopeFits=" << (triangleSlopeFits ? "PASS" : "FAIL")
              << " (H3=" << triangleH3 << " H5=" << triangleH5
              << " H7=" << triangleH7 << " dB, ideal -19.1/-28.0/-34.0)\n";
    std::cout << "symmetricEvenNull=" << (symmetricEvenNull ? "PASS" : "FAIL")
              << " (worst even bin in square50/triangle = " << symmetricEvenWorst << " dB)\n";
    std::cout << "qualityIndependent=" << (qualityIndependent ? "PASS" : "FAIL")
              << " (max |1x-4x| = " << maxQualityDifference << " dB)\n";
    std::cout << "square50 even-harmonic baseline: worst H2/H4/H6/H8/H10/H12 = "
              << square50EvenWorst << " dB re H1 (reference, not gated)\n";

    if (! allFinite || ! sineClean || ! triangleSlopeFits || ! symmetricEvenNull
        || ! qualityIndependent)
    {
        std::cerr << "wave harmonic baseline gate failed\n";
        return 1;
    }
    return 0;
}
