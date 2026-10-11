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
- Sub oscillator (Tier 3): new subOscLevel param (default 0).  A 50%
  PolyBLEP square one octave below oscillator 1 is summed into the
  pre-filter unison mix at centre pan, with its own per-layer phase that
  follows the same seeded/free-run rules.  Verified: subOscLevel=0 renders
  0 changed samples on all six probed presets; 0.6 adds the expected low
  octave (RMS +135% on the Init patch).  UI: SUB knob added to the unison
  strip (now 9 controls).

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

Deployed (2026-10-11, sub-osc build):

- VST3 binary SHA256 3db56238b5bf725e3bcb61f1792edafaeeb81e539dbc183d796e0ce245cbe2dc
- AU binary SHA256 6cd047f7c62f9492e051a90f03da4d9fc4523c42be8f2395560216ab8742e42c
- pluginval strictness 5 SUCCESS on this binary.

## 2026-10-11 addendum — ladder-style stage saturation (filter drive)

### What changed

- processTptSvf gained an opt-in saturate flag: the band state v1 passes
  through a rational soft-clip v1*(27+v1^2)/(27+9*v1^2) before the
  integrator update.  This is the Moog-ladder trick — the resonance itself
  compresses, so a driven filter growls instead of just getting louder.
- Wired only into the stage-one calls, gated on filterDriveActive
  (filterDrive > 0).  The LPF24 cascade stage stays clean.  Drive-free
  patches execute the identical code path.
- Aliasing: filterDriveActive already forces the 2x half-step path
  (filterOversample), so the non-linearity is sampled at twice the host
  rate, same as the existing input tanh.

### Evidence

- Drive-free presets p0/p5/p14/p20/p25/p30 renders bit-identical to the
  pre-change snapshots (/tmp/eef-t2 vs /tmp/eef-sat, 582120 bytes each).
- p26 (minimoog lead, the only factory preset with filterDrive=0.22):
  peak 0.2232 -> 0.1942, RMS 0.0477 -> 0.0471, finite, max diff 0.048 —
  the soft-clip eats the resonance-ring overshoot, which is the point.
- ctest 4/4 PASS, dsp-sanity PASS, pluginval strictness 5 SUCCESS,
  auval -v aumu MnEf EonA PASS.
- Deployed: VST3 SHA256 91ea8bc0ce4ae815ebeda2ee896fdfafb634bddea7b08eec2284aeeceb47a37d
  AU SHA256 88e1be20f5d14eedb4e0cd500b22b5cc9234b072f1aae602a8b3c89c3effc76e.

## Not done / risk

- No A/B listening yet — Ableton gate still blocked (Mac locked).  The
  settle/whistle magnitudes are educated defaults, not tuned values; expect
  a knob-tuning pass after first listen.
- drift>0 renders stay seeded and reproducible across runs, but differ from
  the previous build by design.  Edge case: the first note a voice plays
  after a state load starts all layers at phase 0 (init state), so its
  attack is a single in-phase transient before detune separates them.
