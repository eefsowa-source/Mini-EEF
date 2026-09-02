#pragma once
#include <JuceHeader.h>
#include "Identifiers/ParamIDs.h"
#include <array>
#include <atomic>

class EonMiniEEFProcessor final : public juce::AudioProcessor
{
public:
    EonMiniEEFProcessor();
    ~EonMiniEEFProcessor() override = default;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EEF-JP8000"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Maximum delay time is two seconds and feedback reaches 0.9.  Roughly
    // 66 repeats are required to fall below -60 dB, so 140 seconds is a
    // conservative finite host tail declaration for offline bounce/freeze.
    double getTailLengthSeconds() const override { return 140.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Lock-free handoff from the real-time audio thread to the editor meter.
    float consumePeakLeft() noexcept { return peakLeft.exchange (0.0f); }
    float consumePeakRight() noexcept { return peakRight.exchange (0.0f); }
    float getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }
private:
    void processOversampledOutput (juce::AudioBuffer<float>&, float) noexcept;
    std::unique_ptr<juce::Synthesiser> synth;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> fxDelay;
    int fxWritePosition = 0;
    float chorusPhase = 0.0f;
    std::array<float, 4> reverbL {}, reverbR {};
    // Per-channel state for the final DC blocker.  Kept on the processor so
    // state survives block boundaries without any real-time allocation.
    std::array<float, 2> dcInput {}, dcOutput {};
    // Both pipelines are constructed once with their filter state allocated
    // outside the real-time callback.  The parameter only selects between
    // these prepared paths; processBlock never creates or resizes anything.
    juce::dsp::Oversampling<float> oversampling2x { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::Oversampling<float> oversampling4x { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    int oversamplingBlockSize = 0;
    static constexpr int latencyBufferCapacity = 256;
    std::array<std::array<float, latencyBufferCapacity>, 2> latencyBuffer {};
    int latencyWritePosition = 0;
    int fixedLatencySamples = 0;
    std::atomic<bool> dspResetRequested { false };
    std::atomic<float> hostBpm { 120.0f };
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EonMiniEEFProcessor)
};
