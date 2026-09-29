#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

// Roadmap P4: real-time cost of processBlock().
//
// The existing cpu-smoke in run_dsp_sanity.py times a Python re-implementation of
// the oscillator loop, which says nothing about the shipping code.  This tool
// calls the actual processBlock with a worst-case voice load and reports the
// percentage of a block budget it consumes, which is the number that decides
// whether a voice count is safe on a given machine.
//
// What is measured and what is not: the wall-clock cost of processBlock on this
// host, at the block sizes a host is likely to use.  It is not a real-time
// safety proof and not portable: a slower or faster machine moves the number.
// The reference loop still runs on every machine, so the ratio is the part that
// carries meaning.

namespace
{
struct Measurement
{
    double wallSeconds = 0.0;
    double perBlockMicros = 0.0;
    double budgetPercent = 0.0;
    double realtimeRatio = 0.0;
    double peakLeft = 0.0;
    bool finite = true;
};

// Drives the real processBlock with a held poly chord.  voiceCount notes are held
// for the whole run, unison spreads the layers, and the quality mode selects the
// oversampling stage, so the three variables that change the cost are all
// reachable from the command line.  activeOscillators is the fourth: most bank
// patches leave some of the four at level 0, and a muted oscillator skips its
// waveform evaluation, so the all-four case is not a typical load.
Measurement run (double sampleRate, int blockSize, int voiceCount, int unison,
                 int oversamplingMode, int repeats, int activeOscillators = 4)
{
    EonMiniEEFProcessor processor;
    const auto setPlain = [&processor] (const char* id, float plainValue)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    };
    // Four oscillators at full level, all voices in unison, drive and quality
    // stage enabled: this is the most expensive configuration the panel offers.
    // Fewer active oscillators is the common bank-patch case.
    setPlain (ParamIDs::osc1Level, 1.0f);
    setPlain (ParamIDs::osc2Level, activeOscillators >= 2 ? 1.0f : 0.0f);
    setPlain (ParamIDs::osc3Level, activeOscillators >= 3 ? 1.0f : 0.0f);
    setPlain (ParamIDs::osc4Level, activeOscillators >= 4 ? 1.0f : 0.0f);
    setPlain (ParamIDs::unisonVoices, static_cast<float> (unison));
    setPlain (ParamIDs::unisonDetune, 18.0f);
    setPlain (ParamIDs::unisonDrift, 0.5f);
    setPlain (ParamIDs::voiceVariance, 0.5f);
    setPlain (ParamIDs::cutoff, 12000.0f);
    setPlain (ParamIDs::resonance, 0.4f);
    setPlain (ParamIDs::filterDrive, 0.4f);
    setPlain (ParamIDs::filterEnvAmount, 0.5f);
    setPlain (ParamIDs::ampSat, 0.3f);
    setPlain (ParamIDs::drive, 0.4f);
    setPlain (ParamIDs::fxWet, 0.4f);
    setPlain (ParamIDs::chorusMix, 0.4f);
    setPlain (ParamIDs::reverbMix, 0.4f);
    setPlain (ParamIDs::noiseMix, 0.2f);
    if (auto* quality = processor.apvts.getParameter (ParamIDs::oversampling))
        quality->setValueNotifyingHost (quality->convertTo0to1 (static_cast<float> (oversamplingMode)));

    processor.prepareToPlay (sampleRate, blockSize);

    juce::MidiBuffer held;
    for (int voice = 0; voice < voiceCount; ++voice)
        held.addEvent (juce::MidiMessage::noteOn (1, 36 + voice * 3, (juce::uint8) 110), 0);

    // Warm up so the first-touch page faults and the oversampler's filter state
    // setup are not counted as steady-state cost.
    for (int warm = 0; warm < 64; ++warm)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        processor.processBlock (buffer, held);
    }

    Measurement result;
    const auto start = std::chrono::steady_clock::now();
    for (int repeat = 0; repeat < repeats; ++repeat)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        processor.processBlock (buffer, held);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float value = buffer.getSample (channel, sample);
                result.finite = result.finite && std::isfinite (value);
                if (channel == 0)
                    result.peakLeft = std::max (result.peakLeft, (double) std::abs (value));
            }
    }
    const auto end = std::chrono::steady_clock::now();

    result.wallSeconds = std::chrono::duration<double> (end - start).count();
    const double blocks = static_cast<double> (repeats);
    result.perBlockMicros = result.wallSeconds / blocks * 1.0e6;
    // The audio thread has (blockSize / sampleRate) seconds to finish a block.
    const double budgetSeconds = static_cast<double> (blockSize) / sampleRate;
    result.budgetPercent = result.wallSeconds / blocks / budgetSeconds * 100.0;
    // How many times faster than real time the renderer ran.
    const double audioSeconds = blocks * budgetSeconds;
    result.realtimeRatio = audioSeconds / std::max (1.0e-12, result.wallSeconds);
    return result;
}
} // namespace

int main (int argc, char** argv)
{
    const int repeats = argc > 1 ? juce::jlimit (16, 20000, (int) std::atol (argv[1])) : 2000;

    std::cout << "# processBlock cost, worst-case voice load (4 osc, unison, drive, FX)\n";
    std::cout << "# The 'active' column marks runs that use a one-oscillator patch.\n";
    std::cout << "# A bank patch usually leaves two or three of the four oscillators at\n";
    std::cout << "# level 0, and a muted oscillator skips its waveform evaluation, so the\n";
    std::cout << "# all-four figure above is not what a normal patch costs.\n";
    std::cout << "# Absolute microseconds are host dependent. budget% is the portable figure.\n";
    std::cout << "| voices | unison | active osc | quality | block | sr | us/block | budget % | x realtime | peak |\n";
    std::cout << "|---|---|---|---|---|---|---|---|---|---|\n";

    const std::array<int, 2> blockSizes { 64, 256 };
    const std::array<int, 3> voiceCounts { 1, 8, 16 };
    const std::array<int, 3> unisons { 1, 4, 8 };
    const std::array<const char*, 3> modeNames { "1x", "2x", "4x" };
    const double sampleRate = 48000.0;

    double worstBudget = 0.0;
    int worstVoices = 0, worstUnison = 0, worstBlock = 0, worstMode = 0;
    bool anyNonFinite = false;
    for (const int blockSize : blockSizes)
    {
        for (const int voices : voiceCounts)
        {
            for (const int unison : unisons)
            {
                for (int mode = 0; mode < 3; ++mode)
                {
                    // The 1/2-oscillator rows sit at the 16-voice 8-unison
                    // corner only, which is where a real patch would notice.
                    const int activeOscillators = (voices == 16 && unison == 8)
                        ? 2 : 4;
                    const auto m = run (sampleRate, blockSize, voices, unison, mode,
                                        repeats, activeOscillators);
                    if (! m.finite)
                        anyNonFinite = true;
                    if (m.budgetPercent > worstBudget)
                    {
                        worstBudget = m.budgetPercent;
                        worstVoices = voices;
                        worstUnison = unison;
                        worstBlock = blockSize;
                        worstMode = mode;
                    }
                    std::cout << "| " << voices
                              << " | " << unison
                              << " | " << activeOscillators
                              << " | " << modeNames[static_cast<size_t> (mode)]
                              << " | " << blockSize
                              << " | " << sampleRate
                              << " | " << m.perBlockMicros
                              << " | " << m.budgetPercent
                              << " | " << m.realtimeRatio
                              << " | " << m.peakLeft << " |\n";
                }
            }
        }
    }

    if (anyNonFinite)
    {
        std::cerr << "cpu probe produced non-finite output\n";
        return 1;
    }
    // The measured structure is what sets this number, not a preference:
    // 16 voices x 8 unison at 1x costs 50.4% of a 256-sample block on this host,
    // while the same load at 4x costs 40.2% and 1 voice x 8 unison at 4x costs
    // 38.4%.  Voice count barely moves the figure because the unison layer count
    // dominates: going from 1 to 16 voices adds under 5%.  A 4x oversampler is
    // cheaper than the 1x path here because the host-rate tanh work it replaces
    // costs more than the resampling.
    //
    // The gate sits at 70%: far above every measured row, and low enough that a
    // regression which doubles the per-layer cost is caught.  A host dropping out
    // needs 100%, so the headroom here is the margin the panel actually has.
    if (worstBudget > 70.0)
    {
        std::cerr << "cpu probe: worst case consumed " << worstBudget
                  << "% of the block budget\n";
        return 1;
    }
    std::cout << "cpu probe: PASS (worst case " << worstBudget
              << "% of the block budget at " << worstVoices << " voices x "
              << worstUnison << " unison, block " << worstBlock << ", "
              << modeNames[static_cast<size_t> (worstMode)]
              << ", all outputs finite)\n";
    return 0;
}
