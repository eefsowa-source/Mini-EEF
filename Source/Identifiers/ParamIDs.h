#pragma once
namespace ParamIDs {
inline constexpr auto osc1Wave = "osc1Wave";
inline constexpr auto osc2Wave = "osc2Wave";
inline constexpr auto osc3Wave = "osc3Wave";
inline constexpr auto osc4Wave = "osc4Wave";
inline constexpr auto osc1Level = "osc1Level";
inline constexpr auto osc1Coarse = "osc1Coarse";
inline constexpr auto osc1Fine = "osc1Fine";
inline constexpr auto osc1Phase = "osc1Phase";
inline constexpr auto osc1Pan = "osc1Pan";
inline constexpr auto osc1PulseWidth = "osc1PulseWidth";
inline constexpr auto osc2Level = "osc2Level";
inline constexpr auto osc2Coarse = "osc2Coarse";
inline constexpr auto osc2Fine = "osc2Fine";
inline constexpr auto osc2Phase = "osc2Phase";
inline constexpr auto osc2Pan = "osc2Pan";
inline constexpr auto osc2PulseWidth = "osc2PulseWidth";
inline constexpr auto osc3Level = "osc3Level";
inline constexpr auto osc3Coarse = "osc3Coarse";
inline constexpr auto osc3Fine = "osc3Fine";
inline constexpr auto osc3Phase = "osc3Phase";
inline constexpr auto osc3Pan = "osc3Pan";
inline constexpr auto osc3PulseWidth = "osc3PulseWidth";
inline constexpr auto osc4Level = "osc4Level";
inline constexpr auto osc4Coarse = "osc4Coarse";
inline constexpr auto osc4Fine = "osc4Fine";
inline constexpr auto osc4Phase = "osc4Phase";
inline constexpr auto osc4Pan = "osc4Pan";
inline constexpr auto osc4PulseWidth = "osc4PulseWidth";
// Optional white-noise layer.  Zero keeps the legacy oscillator mix unchanged.
inline constexpr auto noiseMix = "noiseMix";
// Neutral defaults (one layer, zero cents) preserve legacy presets.
inline constexpr auto unisonVoices = "unisonVoices";
inline constexpr auto unisonDetune = "unisonDetune";
inline constexpr auto unisonSpread = "unisonSpread";
inline constexpr auto unisonPhase = "unisonPhase";
inline constexpr auto unisonDrift = "unisonDrift";
// Per-voice analog tolerance: seeded cutoff / envelope-time / level offsets
// per poly voice, modelling the fixed spread between voice cards.  The 0
// default leaves every multiplier at exactly 1.0, so existing presets keep
// their measured output bit-for-bit.
inline constexpr auto voiceVariance = "voiceVariance";
inline constexpr auto voiceMode = "voiceMode";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto resonance = "resonance";
// Per-voice filter input drive: bounded tanh colouration evaluated inside the
// selective 2x filter path.  The 0 default keeps the legacy filter input
// bit-identical so existing presets retain their measured baselines.
inline constexpr auto filterDrive = "filterDrive";
// Filter topology: LPF is the legacy/default mode for preset compatibility.
inline constexpr auto filterMode = "filterMode";
inline constexpr auto attack = "attack";
inline constexpr auto decay = "decay";
inline constexpr auto sustain = "sustain";
inline constexpr auto release = "release";
// Exponential (RC-style) shaping of the amp envelope.  0 keeps the legacy
// linear juce::ADSR response bit-identical; higher values bend attack, decay
// and release toward the analog-style curves without changing their timings.
inline constexpr auto envCurve = "envCurve";
// Dedicated filter envelope.  filterEnvAmount is bipolar and defaults to 0,
// which leaves the cutoff path bit-identical; the four stage times shape the
// sweep when it is turned up.
inline constexpr auto filterAttack = "filterAttack";
inline constexpr auto filterDecay = "filterDecay";
inline constexpr auto filterSustain = "filterSustain";
inline constexpr auto filterRelease = "filterRelease";
inline constexpr auto filterEnvAmount = "filterEnvAmount";
inline constexpr auto gain = "gain";
inline constexpr auto drive = "drive";
// Roadmap step B: selectable drive curve and gentle amp-stage saturation.
// Default "Symmetric" is bit-identical to the legacy tanh path so every
// existing preset keeps its measured baseline.
inline constexpr auto driveCurve = "driveCurve";
inline constexpr auto ampSat = "ampSaturation";
inline constexpr auto lfoRate = "lfoRate";
inline constexpr auto lfoDepth = "lfoDepth";
inline constexpr auto lfoPitch = "lfoPitch";
// LFO-driven amplitude modulation (tremolo/ring-like depth), disabled by default.
inline constexpr auto amDepth = "amDepth";
inline constexpr auto velocityAmount = "velocityAmount";
inline constexpr auto lfoSync = "lfoSync";
inline constexpr auto lfoDivision = "lfoDivision";
inline constexpr auto lfoShape = "lfoShape";
inline constexpr auto keyTracking = "keyTracking";
inline constexpr auto compThreshold = "compThreshold";
inline constexpr auto compRatio = "compRatio";
inline constexpr auto fxWet = "fxWet";
inline constexpr auto delayTime = "delayTime";
inline constexpr auto delayFeedback = "delayFeedback";
// Stereo width of the delay.  0 keeps the established single-path send (the
// left channel feeds both taps), while higher values ping-pong the taps and
// spread the repeats across the stereo field.
inline constexpr auto delayStereo = "delayStereo";
inline constexpr auto chorusDepth = "chorusDepth";
inline constexpr auto chorusRate = "chorusRate";
inline constexpr auto chorusMix = "chorusMix";
inline constexpr auto reverbMix = "reverbMix";
// Slow modulation of the reverb comb feedback.  Zero keeps the established
// static comb texture, so existing presets keep their measured tail.
inline constexpr auto reverbModulation = "reverbModulation";
// Global non-linear output quality.  1x is the compatibility/default mode;
// higher modes oversample the final saturation stage before downsampling.
inline constexpr auto oversampling = "oversampling";
// Four fixed routing slots keep the modulation matrix deterministic and
// allocation-free in the real-time renderer.  Sources and destinations use
// stable choice parameters so old state remains valid when new slots are
// added.
inline constexpr auto modSourcePrefix = "modSource";
inline constexpr auto modDestinationPrefix = "modDestination";
inline constexpr auto modAmountPrefix = "modAmount";
inline constexpr const char* modSource (int index) {
    return index == 0 ? "modSource0" : index == 1 ? "modSource1" : index == 2 ? "modSource2" : "modSource3";
}
inline constexpr const char* modDestination (int index) {
    return index == 0 ? "modDestination0" : index == 1 ? "modDestination1" : index == 2 ? "modDestination2" : "modDestination3";
}
inline constexpr const char* modAmount (int index) {
    return index == 0 ? "modAmount0" : index == 1 ? "modAmount1" : index == 2 ? "modAmount2" : "modAmount3";
}
// Matrix source/destination indices are fixed in the current state format.
// Source 0 is off, 1-3 are LFO/envelope/velocity, 4 is oscillator 1
// (audio-rate FM source) and 5 is the dedicated filter envelope.  Legacy
// source 4/5/6 states are migrated on load: 6 became oscillator 1, and
// anything else at 4 or above became off, so index 5 was already
// normalised to off before this source existed.  No stored state can
// therefore acquire a surprise filter-envelope routing on load.
// Destination 0 is off, 1-5 are pitch/cutoff/amp/oscillator-2 FM/PWM.
}
