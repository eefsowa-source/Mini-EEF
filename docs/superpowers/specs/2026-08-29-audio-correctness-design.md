# EEF-JP8000 Audio Correctness Design

## Goal

Fix confirmed sound and validation defects without adding parameters, restoring removed MIDI/TIME FX/wavetable features, or changing preset/state IDs.

## Selected scope

- Make `voiceMode=Mono` a real one-voice, last-note-priority mode. A new note retriggers immediately; releasing the current note falls back to the newest still-held note with its stored velocity. Poly behavior remains unchanged.
- Make chorus reads safe for the full published depth range. Clamp the modulated delay to the valid delay-line interval and use linear fractional interpolation instead of integer taps.
- Make the DC blocker cutoff invariant across sample rates by deriving its pole from a fixed subsonic cutoff during `prepareToPlay`.
- Process oversized host blocks through the selected preallocated oversampler in chunks rather than silently bypassing oversampling.
- Make local/CI quality gates fail when CTest was not configured, and make GitHub Actions run CTest before pluginval.

## Compatibility and realtime constraints

- Keep all APVTS parameter IDs, defaults, six factory presets, VST3 identity, Note On/Off plus velocity-only MIDI filtering, and sample-offset event timing.
- No allocation, lock, file I/O, host notification, or UI call in `processBlock`.
- Held-note tracking uses fixed-size storage. Delay interpolation reads the existing preallocated buffer. Oversampling chunks reuse the already prepared JUCE oversamplers.
- No reverb redesign or new UI controls in this patch. Reverb topology and parameter smoothing are a follow-up after these correctness gates pass.

## Acceptance

- Regression tests are observed failing on the old implementation for mono behavior, unsafe chorus modulation, sample-rate-dependent bass loss, oversized-block oversampling bypass, and false-positive quality script behavior.
- The same tests then pass, followed by a fresh Release build, full CTest, and pluginval strictness 5.
- Automated checks are reported separately from actual DAW audition; no DAW claim is made without a host run.
