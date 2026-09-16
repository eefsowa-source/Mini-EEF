#include "../Source/PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

// Roadmap step A: quantitative THD baseline for the global drive stage.
// Renders a pure sine drone through the processor at several drive levels
// in both the 1x (host-rate) and 4x (oversampled) saturation paths, then
// measures the harmonic spectrum, DC component, and aliasing fold lines.
// The printed table is the baseline that future drive-curve work (asymmetric
// tanh, tube-style soft clip) must be compared against.

namespace
{
constexpr double probeSampleRate = 48000.0;
constexpr int probeBlockSize = 256;
constexpr int droneTotalSamples = 49152;
constexpr int analysisStart = 16384;
constexpr int analysisLength = 32768;
constexpr double fundamentalHz = 1760.0; // MIDI 93 (A6)

// Fold-back lines of the 15th/17th harmonics at 48 kHz: 26.4 kHz -> 21.6 kHz,
// 29.92 kHz -> 18.08 kHz. Genuine low-order harmonics never land here.
constexpr std::array<double, 2> aliasFoldLines { 21600.0, 18080.0 };

bool configureSineProbe (EonMiniEEFProcessor& probe, float driveLevel)
{
    const auto setPlain = [&probe] (const char* id, float plainValue)
    {
        if (auto* parameter = probe.apvts.getParameter (id))
        {
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
            return true;
        }
        return false;
    };

    // A single sine oscillator isolates the drive stage: symmetric tanh()
    // then produces odd harmonics only, so the even-harmonic columns double
    // as asymmetry detectors once new drive curves are introduced.
    return setPlain (ParamIDs::osc1Wave, 3.0f)          // sine
        && setPlain (ParamIDs::osc1Level, 1.0f)
        && setPlain (ParamIDs::osc2Level, 0.0f)
        && setPlain (ParamIDs::osc3Level, 0.0f)
        && setPlain (ParamIDs::osc4Level, 0.0f)
        && setPlain (ParamIDs::noiseMix, 0.0f)
        && setPlain (ParamIDs::unisonVoices, 1.0f)
        && setPlain (ParamIDs::cutoff, 20000.0f)
        && setPlain (ParamIDs::resonance, 0.0f)
        && setPlain (ParamIDs::attack, 0.001f)
        && setPlain (ParamIDs::decay, 0.001f)
        && setPlain (ParamIDs::sustain, 1.0f)
        && setPlain (ParamIDs::release, 0.001f)
        && setPlain (ParamIDs::gain, 0.8f)
        && setPlain (ParamIDs::drive, driveLevel)
        && setPlain (ParamIDs::ampSat, 0.0f)
        && setPlain (ParamIDs::fxWet, 0.0f)
        && setPlain (ParamIDs::delayFeedback, 0.0f)
        && setPlain (ParamIDs::chorusMix, 0.0f)
        && setPlain (ParamIDs::reverbMix, 0.0f)
        && setPlain (ParamIDs::voiceMode, 1.0f);
}

void setOversamplingMode (EonMiniEEFProcessor& probe, int mode)
{
    if (auto* quality = probe.apvts.getParameter (ParamIDs::oversampling))
        quality->setValueNotifyingHost (quality->convertTo0to1 (static_cast<float> (mode)));
}

void setDriveCurve (EonMiniEEFProcessor& probe, int curve)
{
    if (auto* parameter = probe.apvts.getParameter (ParamIDs::driveCurve))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (curve)));
}

void renderDroneWindow (EonMiniEEFProcessor& probe, std::array<float, analysisLength>& window)
{
    probe.prepareToPlay (probeSampleRate, probeBlockSize);
    for (int blockStart = 0; blockStart < droneTotalSamples; blockStart += probeBlockSize)
    {
        juce::AudioBuffer<float> buffer (2, probeBlockSize);
        juce::MidiBuffer midi;
        if (blockStart == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 93, (juce::uint8) 100), 0);
        probe.processBlock (buffer, midi);
        for (int sample = 0; sample < probeBlockSize; ++sample)
        {
            const int absoluteSample = blockStart + sample;
            if (absoluteSample >= analysisStart)
                window[(size_t) (absoluteSample - analysisStart)] = buffer.getSample (0, sample);
        }
    }
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
    return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2))
         / bigN;
}

double toDb (double ratio)
{
    return 20.0 * std::log10 (juce::jmax (1.0e-12, ratio));
}

struct DriveReport
{
    double thd = 0.0;
    std::array<double, 5> harmonicDb {};
    double dcDb = 0.0;
    double aliasRatio = 0.0;
    // Deep-stopband fold ratio (18.08 kHz line only), matching the scope of
    // the established PresetSmoke aliasing gate. The 21.6 kHz line sits on
    // the oversampler's stopband edge at 48 kHz, where residual H5 energy is
    // anti-alias-filter rolloff rather than host-rate aliasing.
    double aliasRatioDeep = 0.0;
};

DriveReport analyseWindow (const std::array<float, analysisLength>& window)
{
    DriveReport report;
    const double fundamental = goertzelMagnitude (window, fundamentalHz);
    double harmonicPower = 0.0;
    for (int harmonic = 2; harmonic <= 6; ++harmonic)
    {
        const double magnitude = goertzelMagnitude (window, fundamentalHz * harmonic);
        report.harmonicDb[(size_t) (harmonic - 2)] = toDb (magnitude / fundamental);
        harmonicPower += magnitude * magnitude;
    }
    report.thd = std::sqrt (harmonicPower) / juce::jmax (1.0e-12, fundamental);

    double mean = 0.0;
    for (const float sample : window)
        mean += sample;
    mean /= static_cast<double> (window.size());
    report.dcDb = toDb (std::abs (mean) / fundamental);

    double aliasPower = 0.0;
    for (const double line : aliasFoldLines)
    {
        const double magnitude = goertzelMagnitude (window, line);
        aliasPower += magnitude * magnitude;
    }
    report.aliasRatio = aliasPower / juce::jmax (1.0e-12, fundamental * fundamental);
    const double deepAlias = goertzelMagnitude (window, aliasFoldLines[1]);
    report.aliasRatioDeep = deepAlias * deepAlias
        / juce::jmax (1.0e-12, fundamental * fundamental);
    return report;
}
} // namespace

int main()
{
    constexpr std::array<float, 4> driveLevels { 0.0f, 0.25f, 0.5f, 1.0f };
    constexpr std::array<int, 2> oversamplingModes { 0, 2 }; // 1x, 4x
    // The B-step curve comparison runs at 4x with drive 1.0.
    constexpr std::array<const char*, 3> curveNames { "SYM", "ASYM", "TUBE" };

    std::cout << "# THD drive baseline (sine 1760 Hz @ 48 kHz, analysis 32768 samples)\n";
    std::cout << "| drive | mode | curve | THD % | H2 dB | H3 dB | H4 dB | H5 dB | H6 dB | DC dB | alias ratio | alias deep |\n";
    std::cout << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";

    std::array<std::array<DriveReport, 2>, driveLevels.size()> reports {};
    for (size_t driveIndex = 0; driveIndex < driveLevels.size(); ++driveIndex)
    {
        for (size_t modeIndex = 0; modeIndex < oversamplingModes.size(); ++modeIndex)
        {
            EonMiniEEFProcessor probe;
            if (! configureSineProbe (probe, driveLevels[driveIndex]))
            {
                std::cerr << "THD probe could not configure the sine voice\n";
                return 1;
            }
            setOversamplingMode (probe, oversamplingModes[modeIndex]);
            setDriveCurve (probe, 0); // baseline table always uses the legacy curve

            std::array<float, analysisLength> window {};
            renderDroneWindow (probe, window);
            for (const float sample : window)
                if (! std::isfinite (sample))
                {
                    std::cerr << "THD probe produced non-finite output (drive="
                              << driveLevels[driveIndex] << ")\n";
                    return 1;
                }

            reports[driveIndex][modeIndex] = analyseWindow (window);
            const auto& r = reports[driveIndex][modeIndex];
            std::cout << "| " << driveLevels[driveIndex]
                      << " | " << (oversamplingModes[modeIndex] == 0 ? "1x" : "4x")
                      << " | " << curveNames[0]
                      << " | " << (100.0 * r.thd)
                      << " | " << r.harmonicDb[0]
                      << " | " << r.harmonicDb[1]
                      << " | " << r.harmonicDb[2]
                      << " | " << r.harmonicDb[3]
                      << " | " << r.harmonicDb[4]
                      << " | " << r.dcDb
                      << " | " << r.aliasRatio
                      << " | " << r.aliasRatioDeep << " |\n";
        }
    }

    // B-step curve comparison at drive=1, 4x.
    constexpr float comparisonDrive = 1.0f;
    std::array<DriveReport, 3> curveReports {};
    for (int curve = 0; curve < 3; ++curve)
    {
        EonMiniEEFProcessor probe;
        if (! configureSineProbe (probe, comparisonDrive))
        {
            std::cerr << "THD probe could not configure the sine voice\n";
            return 1;
        }
        setOversamplingMode (probe, 2);
        setDriveCurve (probe, curve);
        std::array<float, analysisLength> window {};
        renderDroneWindow (probe, window);
        curveReports[(size_t) curve] = analyseWindow (window);
        const auto& r = curveReports[(size_t) curve];
        std::cout << "| " << comparisonDrive << " | 4x | " << curveNames[(size_t) curve]
                  << " | " << (100.0 * r.thd)
                  << " | " << r.harmonicDb[0] << " | " << r.harmonicDb[1]
                  << " | " << r.harmonicDb[2] << " | " << r.harmonicDb[3]
                  << " | " << r.harmonicDb[4]
                  << " | " << r.dcDb
                  << " | " << r.aliasRatio
                  << " | " << r.aliasRatioDeep << " |\n";
    }

    // Gates. The THD numbers document the tanh() baseline; the structural
    // assertions catch regressions, not tuning preferences.
    const bool cleanSine = reports[0][0].thd < 0.01;
    const bool driveAddsHarmonics = reports[3][0].thd > reports[1][0].thd
        && reports[2][0].thd > reports[1][0].thd;
    // New in step B: the alternative curves must be genuinely different in
    // harmonic content from the symmetric baseline while staying bounded and
    // DC-controlled (the DC blocker plus final limiter hold that line).
    const bool curvesDistinct = std::abs (curveReports[1].thd - curveReports[0].thd) > 0.002
                             || std::abs (curveReports[2].thd - curveReports[0].thd) > 0.002;
    const bool curvesBounded = std::isfinite (curveReports[1].thd)
        && std::isfinite (curveReports[2].thd)
        && curveReports[1].thd < 1.0 && curveReports[2].thd < 1.0;
    const bool curvesDcOk = curveReports[1].dcDb < -20.0 && curveReports[2].dcDb < -20.0;
    bool dcControlled = true;
    for (const auto& perDrive : reports)
        for (const auto& report : perDrive)
            dcControlled = dcControlled && report.dcDb < -30.0;

    // Aliasing is not gated here: a single sine into tanh() at these levels
    // keeps the 15th+ fold lines below the window-leakage floor, so a
    // fold-line ratio would compare floor noise. The deep-clip pulse drone
    // regression in PresetSmoke (runOversampledDriveAliasingRegression) owns
    // alias gating; this probe owns the harmonic/THD baseline.
    std::cout << "cleanSine=" << (cleanSine ? "PASS" : "FAIL")
              << " driveAddsHarmonics=" << (driveAddsHarmonics ? "PASS" : "FAIL")
              << " dcControlled=" << (dcControlled ? "PASS" : "FAIL")
              << " curvesDistinct=" << (curvesDistinct ? "PASS" : "FAIL")
              << " curvesBounded=" << (curvesBounded ? "PASS" : "FAIL")
              << " curvesDcOk=" << (curvesDcOk ? "PASS" : "FAIL")
              << " (alias gating owned by PresetSmoke regression)\n";

    if (! cleanSine || ! driveAddsHarmonics || ! dcControlled
        || ! curvesDistinct || ! curvesBounded || ! curvesDcOk)
    {
        std::cerr << "THD drive baseline gate failed\n";
        return 1;
    }
    std::cout << "THD drive baseline: PASS\n";

    // Roadmap step C: unison drift must stay deterministic (same seed chain
    // -> identical offline renders) and bounded; the new LFO shapes must be
    // finite and bounded as well. Motion quality itself stays a listening
    // decision; these assertions catch state or range regressions.
    const auto configureMotionProbe = [] (EonMiniEEFProcessor& probe, int shape,
                                          float driftAmount)
    {
        const auto setPlain = [&probe] (const char* id, float plainValue)
        {
            if (auto* parameter = probe.apvts.getParameter (id))
            {
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
                return true;
            }
            return false;
        };
        return configureSineProbe (probe, 0.0f)
            && setPlain (ParamIDs::unisonVoices, 4.0f)
            && setPlain (ParamIDs::unisonDetune, 18.0f)
            && setPlain (ParamIDs::unisonSpread, 0.7f)
            && setPlain (ParamIDs::unisonDrift, driftAmount)
            && setPlain (ParamIDs::lfoShape, static_cast<float> (shape))
            && setPlain (ParamIDs::lfoRate, 3.0f);
    };
    const auto renderAndPeak = [] (EonMiniEEFProcessor& probe)
    {
        std::array<float, analysisLength> window {};
        renderDroneWindow (probe, window);
        float peak = 0.0f;
        bool finite = true;
        for (const float sample : window)
        {
            finite = finite && std::isfinite (sample);
            peak = juce::jmax (peak, std::abs (sample));
        }
        return std::pair<float, bool> { peak, finite };
    };

    EonMiniEEFProcessor driftProbeA;
    EonMiniEEFProcessor driftProbeB;
    const bool driftConfigured = configureMotionProbe (driftProbeA, 0, 1.0f)
        && configureMotionProbe (driftProbeB, 0, 1.0f);
    std::array<float, analysisLength> windowA {};
    std::array<float, analysisLength> windowB {};
    renderDroneWindow (driftProbeA, windowA);
    renderDroneWindow (driftProbeB, windowB);
    float driftDifference = 0.0f;
    for (size_t index = 0; index < windowA.size(); ++index)
        driftDifference = juce::jmax (driftDifference,
            std::abs (windowA[index] - windowB[index]));
    const auto driftRun = renderAndPeak (driftProbeA);
    const bool driftDeterministic = driftConfigured && driftDifference == 0.0f;
    const bool driftBounded = driftRun.second && driftRun.first < 4.0f;

    bool lfoShapesBounded = true;
    for (int shape = 0; shape < 3; ++shape)
    {
        EonMiniEEFProcessor shapeProbe;
        if (! configureMotionProbe (shapeProbe, shape, 0.0f))
        {
            lfoShapesBounded = false;
            break;
        }
        const auto shapeRun = renderAndPeak (shapeProbe);
        lfoShapesBounded = lfoShapesBounded && shapeRun.second && shapeRun.first < 4.0f;
    }

    std::cout << "driftDeterministic=" << (driftDeterministic ? "PASS" : "FAIL")
              << " driftBounded=" << (driftBounded ? "PASS" : "FAIL")
              << " lfoShapesBounded=" << (lfoShapesBounded ? "PASS" : "FAIL") << "\n";
    if (! driftDeterministic || ! driftBounded || ! lfoShapesBounded)
    {
        std::cerr << "analog motion gate failed\n";
        return 1;
    }
    std::cout << "analog motion (drift/LFO): PASS\n";
    return 0;
}
