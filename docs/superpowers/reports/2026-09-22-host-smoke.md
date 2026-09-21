# Host smoke gate (2026-09-22)

이번 단계에서는 현재 Release VST3를 실제 DAW에서 로드하고 재생하는 마지막
호스트 게이트를 시도했다. 자동화 스크립트는 [reaper_eef_instrument_smoke.lua](../../../scripts/reaper_eef_instrument_smoke.lua)로 남겼다.

## Candidate identity

- Build: `Build-Thd/EonMiniEEF_artefacts/Release/VST3/EEF-JP8000.vst3`
- Built Mach-O SHA256: `30f909c9369f846eeceb34db0db8f12c8670f3ba12980bc80374adad7572cad4`
- Installed VST3 Mach-O SHA256: `0c9b1ae039012395f5e6c98e1b741b115df0f900ee27995cb1ed6d0d3b6ae623`
- The installed copy is therefore not the same binary as this Release build.

## REAPER

- Version: `7.79.0_06dd787u`
- Isolated profile: `/tmp/eef-reaper-host-CcfhkI`
- Candidate scan entry: `EEF-JP8000 (EON Audio)`, VSTi, UID
  `{ABCDEF019182FAEB456F6E414D6E4566}` in `reaper-vstplugins_arm64.ini`
- Requested render conditions: 48 kHz, stereo, 3 seconds, MIDI notes on the
  instrument track.

The isolated `-newinst` script and `-renderproject` attempts did not reach a
completed render or produce the smoke status/WAV evidence while another REAPER
instance was already running on this host. The scan-cache line proves discovery
of the candidate, but it does not prove successful instantiation, audio callback
processing, or rendered output. The current REAPER project was left untouched.

## Ableton Live

- Version: `12.4.5 (2026-08-19_225ce5e356)`
- Observed set: `EEF-Spatial-Release-Recall`
- Observed sample rate: 48.0 kHz

Live was already running an active user set. The Mini EEF was not loaded into
that set, so no Live load, playback, save/reopen, CPU, or listening claim is made;
the set was left untouched.

## Gate status

Build, offline DSP, pluginval, AU validation, and scan-level discovery are
available from the preceding reports. Exact Release-binary REAPER playback,
Ableton playback, and level-matched listening remain open gates. The installed
VST3 must also be refreshed before a host result can be attributed to the
`30f909c...` build.
