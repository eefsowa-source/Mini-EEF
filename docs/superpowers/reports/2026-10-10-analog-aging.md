# 2026-10-10 Analog aging behaviors (drift-gated Tier 1)

## What changed

Five analog behaviors in EonVoice, all gated on unisonDrift > 0 so every
drift=0 patch stays bit-identical:

- Per-voice slow pitch wander (xorshift random walk, +-8 cents at drift=1,
  linear cents-to-ratio).  Applies on top of pitchVarianceRatio.
- Note-on settle: a seeded +-6-cent one-pole transient decaying with a
  ~30 ms time constant (VCO warm-up smear).
- Free-running oscillator phase: with drift>0, startNote no longer resets
  ph1..ph4 to the deterministic offsets; phases continue from the previous
  note so retrigger attacks vary.  drift=0 keeps the old deterministic seed.
- Board hiss: independent L/R draws scaled to -68 dBFS at drift=1, added to
  the summed unison output before the filter.
- Resonance whistle: damping floor lowered from 0.08 to 0.02 with an extra
  term that only engages above resonance 0.9 (res<=0.9 bit-identical).
- Resonance bass loss (Tier 2): above resonance 0.9 and on the low-pass
  modes only, the filter input is high-passed at ~40 Hz with a
  res-proportional mix (up to 0.06 at res=1), approximating the passband
  loss of a resonating ladder.  res<=0.9 leaves filterX == x bit-for-bit.

Tooling: RenderWav now deletes the output file before writing —
juce::File::createOutputStream does not truncate, so repeated renders were
concatenating RIFFs.  RenderWav also accepts paramId=plainValue overrides
as extra argv entries (e.g. resonance=0.97 cutoff=3000).

## Verification of the new branches (2026-10-11)

No factory preset reaches resonance > 0.9 or exercises the whistle path, so
the new branches were rendered with overrides on preset 0:

- res=0.95 / res=0.97, cutoff=3000: output stays finite and bounded.  A
  Goertzel measurement at the cutoff in the release tail shows the
  resonant component rising from 0.00017 (res 0.89) to 0.00047 (res 0.97),
  ~2.7x for the damping change 0.08 -> ~0.05.  Whistle is present but
  restrained; a true self-oscillation whistle needs the drive to keep
  feeding the filter.
- drift=0.5 on preset 0: render differs massively as designed; hiss is
  below the signal floor (in-band, ~-74 dBFS at drift 0.5) and only exists
  while a voice sounds, so it cannot be isolated in a silent window.

## Evidence

- Build: cmake --build Build-Git2 --config Release.
- Render parity (24-bit PCM): drift=0 presets 0/5/20/25 produce 0 changed
  samples vs the pre-change build.  drift>0 presets 14/30 change massively
  (>90% of samples) by design — the phase free-run alone reshapes every
  attack.
- CTest Release 4/4 PASS (preset_state_and_audio_smoke still passes since it
  checks structure, not a bit baseline).
- python3 scripts/run_dsp_sanity.py PASS.
- CpuProbe 400: worst case 48.30% vs 48.23% before this change (+0.07pp,
  the wander/hiss cost at drift=0 is two xorshift draws per sample).

## Deployed

- VST3 -> ~/Library/Audio/Plug-Ins/VST3/EEF-JP8000.vst3, binary SHA256
  4b6c27ba16d29ffe038cdc3b866152927bfe36fbf2faf94752a1dabbfe45837a
- AU   -> ~/Library/Audio/Plug-Ins/Components/EEF-JP8000.component, binary
  SHA256 a056d16aeaeb655b5805693dc9c42f2613b9a43374cf1e6c25eeb811d47806df
- pluginval strictness 5 SUCCESS on this binary; auval -v aumu MnEf EonA
  AU VALIDATION SUCCEEDED.

## Not done / risk

- No A/B listening yet — Ableton gate still blocked (Mac locked).  The
  settle/whistle magnitudes are educated defaults, not tuned values; expect
  a knob-tuning pass after first listen.
- drift>0 renders stay seeded and reproducible across runs, but differ from
  the previous build by design.  Edge case: the first note a voice plays
  after a state load starts all layers at phase 0 (init state), so its
  attack is a single in-phase transient before detune separates them.
