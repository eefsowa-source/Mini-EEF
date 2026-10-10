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

Tooling: RenderWav now deletes the output file before writing —
juce::File::createOutputStream does not truncate, so repeated renders were
concatenating RIFFs.

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

## Not done / risk

- No A/B listening yet — Ableton gate still blocked (Mac locked).  The
  settle/whistle magnitudes are educated defaults, not tuned values; expect
  a knob-tuning pass after first listen.
- drift>0 renders stay seeded and reproducible across runs, but differ from
  the previous build by design.  Edge case: the first note a voice plays
  after a state load starts all layers at phase 0 (init state), so its
  attack is a single in-phase transient before detune separates them.
