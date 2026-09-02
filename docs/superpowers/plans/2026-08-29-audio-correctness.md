# EEF-JP8000 Audio Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove confirmed note-priority, chorus, DC-blocker, oversized-block, and quality-gate defects while preserving the current synth contract.

**Architecture:** Keep the monolithic processor architecture intact and make narrowly scoped changes at its established ownership points. Add black-box regressions to `PresetSmoke.cpp`; use fixed-size state and prepared DSP objects so the callback remains allocation-free.

**Tech Stack:** C++20, JUCE 8, CMake/CTest, Bash, pluginval.

---

### Task 1: Mono note priority

**Files:**
- Modify: `tools/PresetSmoke.cpp`
- Modify: `Source/PluginProcessor.cpp`

- [ ] Add a black-box test that selects Mono, renders C-G-E note transitions at known sample offsets, verifies only the newest held pitch dominates, verifies non-current note-off does not change the current note, and verifies E-off falls back to G then G-off to C.
- [ ] Build and run `EonMiniEEF_PresetSmoke`; record the expected failure caused by the current polyphonic output.
- [ ] Give `NoteOnlySynthesiser` access to the APVTS mode, add fixed `[16][128]` held-note velocity/sequence state, and branch only Note On/Off handling. Mono starts one retriggered voice; current-note release selects the highest sequence still held; no held note releases the voice.
- [ ] Rebuild and run the focused smoke executable; require the new test and existing sample-offset/non-note-MIDI tests to pass.

### Task 2: Chorus, DC blocker, and oversized oversampling

**Files:**
- Modify: `tools/PresetSmoke.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] Add a maximum-depth chorus test that renders beyond one LFO quarter-cycle and rejects one-channel tap disappearance.
- [ ] Add a 55 Hz level-consistency test at 44.1 and 192 kHz, with a tolerance that fails the current fixed `0.995` pole.
- [ ] Add an equivalence test comparing a 257-sample 4x-oversampled render prepared with hints 64 and 257; it must fail while the 64-hint path bypasses oversampling.
- [ ] Run the smoke executable and record all intended failures before production edits.
- [ ] Add a no-allocation fractional delay reader which clamps delay samples to `[1, lineLength-2]` and linearly interpolates wrapped adjacent samples.
- [ ] Compute `dcPole = exp(-2*pi*8/sampleRate)` in `prepareToPlay` and use it in the one-pole DC blocker.
- [ ] Split `processOversampledOutput` into sub-blocks no larger than `oversamplingBlockSize`, applying the selected oversampler and nonlinearity to every chunk while preserving latency compensation across the full host block.
- [ ] Rebuild and require all new regressions plus the prior smoke suite to pass.

### Task 3: Honest quality gates

**Files:**
- Create: `tools/QualityGateContract.cmake`
- Modify: `CMakeLists.txt`
- Modify: `scripts/run_quality_checks.sh`
- Modify: `.github/workflows/pluginval.yml`

- [ ] Add a CTest contract that invokes `run_quality_checks.sh` with a missing build directory and fails unless the script exits non-zero and omits `quality: PASS`; also assert the workflow contains a CTest command.
- [ ] Run that contract and record failure against the existing script/workflow.
- [ ] Change the script to exit non-zero when `CTestTestfile.cmake` is absent. Add `ctest --test-dir Build -C Release --output-on-failure` to GitHub Actions after build and before pluginval.
- [ ] Reconfigure and run CTest; require the contract and all existing tests to pass.

### Task 4: Release verification and review

**Files:**
- Verify all changed files above; no source additions.

- [ ] Configure a fresh `Build-Quality-Audit` Release tree using `/Users/sungha/JUCE`.
- [ ] Build all targets and run `ctest --test-dir Build-Quality-Audit -C Release --output-on-failure`.
- [ ] Run pluginval strictness 5 against the fresh VST3 artifact.
- [ ] Run independent spec-compliance and code-quality review; resolve every Critical or Important item and rerun affected checks.
- [ ] Record that Git commit is unavailable because this directory has no `.git` repository; do not initialize one implicitly.
