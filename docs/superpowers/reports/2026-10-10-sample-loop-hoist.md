# 2026-10-10 Voice sample-loop hoists and reciprocal divides

## What changed

Second CPU round on EonVoice::renderNextBlock (first round: commits
aecd80f, be653b6, 5340814).  All work is in Source/PluginProcessor.cpp.

Batch 1, bit-identical:

- Hoisted block-constant values out of the sample loop: waveform indexes,
  coarse semitones, LFO pitch/depth amounts, velocity gain, unison scale,
  per-layer unison position/positionNorm, and srF/nyquist.
- Mod matrix decoded once per block into source/destination/amount arrays.
  Oscillator-1-as-FM-source accumulation moved to block scope (it is a
  parameter-only sum).  Slots that can only contribute +-0 (source 0,
  destination 0, amount 0, null parameters) are skipped.
- modulatedFrequency skips each exp2f when its exponent is 0
  (exp2(0) == 1 exactly, multiplication order preserved).
- Value-keyed caches: per-oscillator exp2f(semitones/12) and per-layer
  exp2f(cents/1200) re-evaluate only when the smoothed input moved.
- lfoPhase and the pulse-oscillator second BLEP phase use compare-wrap
  instead of std::fmod (each can wrap at most once per step).
- Removed the dead per-sample keyTrack load (keyRatioForNote is already
  computed in startNote).

Batch 2, ~1 ulp changes (div -> multiply-by-reciprocal):

- increment, frequencyRatio (Nyquist fade), cents and positionNorm
  spans, fine-tune /100, the TPT SVF v1 solve (call sites now pass
  precomputed reciprocals), and the filter-drive tanh normalisation.

## Evidence

- Build: cmake --build Build-Git2 --config Release, VST3 binary
  SHA256 d074380c2bf604890b0a16b4537238d16a66339b2ba6210ea6454e7f3b1313d3.
- Render parity: EonMiniEEF_RenderWav presets 0/5/14/20/25/30 vs
  /tmp/eef-base baseline rendered at 5340814.  Renders are 24-bit PCM.
  After batch 1 the PCM is bit-identical on all six presets.  After batch 2:
  drift=0 presets (0/5/20/25) differ by at most 1 LSB on <=2.7% of samples
  (RMS < 0.0001%); drift>0 presets (14/30) differ by RMS 0.45-0.58% because
  the 1-ulp phase increments decorrelate the drift wanderers over 2.2 s.
  (Corrected 2026-10-10: an earlier version of this report called the
  renders 16-bit and claimed 0-LSB parity for batch 2.)
- CpuProbe 400 repeats, Release, 48 kHz — worst case at 8 voices x 8 unison,
  block 64, 1x, all outputs finite:

  | stage | 8v x 8u 1x | 16v x 8u (2 osc) 1x @64 | 16v x 8u (2 osc) 1x @256 |
  |---|---|---|---|
  | baseline (5340814) | 52.0% | 48.0% | 40.4% |
  | after batch 1 | 49.5% | 45.7% | 37.6% |
  | after batch 2 | 48.2% | 44.1% | 36.5% |

- CTest Release 4/4 (preset_state_and_audio_smoke, realtime_audio_contract,
  dsp_numeric_sanity, wave_harmonic_baseline).
- python3 scripts/run_dsp_sanity.py PASS (44.1/48/96/192 kHz).
- scripts/run_pluginval.sh strictness 5 on the Build-Git2 VST3: SUCCESS.

## Not done / risk

- Host gates: Ableton Live 12 load + listening still pending (Mac was locked
  during the 2026-10-09 session; same blocker).  The installed plug-ins still
  hold the previous binary — this build is not yet deployed to
  ~/Library/Audio/Plug-Ins.
- Batch 2 introduces ~1 ulp differences in the float path.  On drift=0
  presets they stay at 1 LSB of 24-bit PCM; under drift>0 the phase
  decorrelation grows to about 0.5% RMS, which is inaudible but visible.
