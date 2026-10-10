# 2026-10-09 HEAD revalidation — commit 5340814

## Binaries

- VST3 (Build-Git2 Release): `facacde8153d3d428f9c8dfba5319334360383d6f3937ae3224dfc2b98799845`
  - `~/Library/Audio/Plug-Ins/VST3/EEF-JP8000.vst3` reinstalled, same hash
- AU (Build-Git2 Release): `bd251f2672891620c4d79a55e656d416448bbadd1c8caeeea7ac2b193aec5912`
  - `~/Library/Audio/Plug-Ins/Components/EEF-JP8000.component` reinstalled, same hash

## Gates passed

- `ctest --test-dir Build-Git2 -C Release`: 4/4 (preset_state_and_audio_smoke,
  realtime_audio_contract, dsp_numeric_sanity, wave_harmonic_baseline)
- `scripts/run_dsp_sanity.py`: PASS — 44.1/48/96/192 kHz, finite/bounded/DC
- `scripts/run_pluginval.sh` strictness 5: SUCCESS (VST3)
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED (render, MIDI, parameter,
  ramped scheduling tests all PASS)

## Open gate

- Ableton Live 12 Suite host load: Mac locked, GUI unavailable. Retry needed;
  Live version/buffer/sample rate not yet recorded. REAPER unused per project rule.
