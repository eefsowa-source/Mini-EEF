# EEF-JP8000 설계

## 목표

깨끗하고 두꺼운 VA 기본 음색을 낮은 CPU 비용으로 제공하는 독자적 VST3 instrument. 특정 상용 신디사이저의 코드·프리셋·UI를 재현하지 않으며, 청감·주파수 응답·CPU 측정으로 품질을 판단한다.

## 현재 구현 (Phase 1~4 핵심 프로토타입)

- JUCE 8.0.14 / C++20 / CMake
- VST3 instrument, macOS 우선 빌드
- 16 voice polyphonic, sample-offset 정확한 Note On/Off renderer
- Oscillator 4개: Saw, Square, Triangle, Sine (OSC3/4는 독립 mix로 활성화)
- Saw/Square PolyBLEP 대역 제한
- per-voice phase, ADSR, 최대 8-voice unison, detune/stereo spread/phase
- LFO, velocity 및 4-slot modulation matrix
- Pitch/Cutoff/Amp/Osc2 FM 목적지와 bounded phase-FM
- LFO free-run/host tempo-sync, cutoff key tracking, 독립 Noise와 LFO AM
- TPT state-variable LPF/HPF/BPF + LPF24(공진 스테이지 뒤 평탄 TPT 캐스케이드),
  cutoff/resonance smoothing
- 보이스별 필터 드라이브(tanh, 기본 0=레거시 비트 동일). 드라이브가 켜지면
  선택적 2x 서브스텝 안에서 비선형을 호스트 레이트의 2배로 평가한다
- 내부 drive, chorus/delay/reverb, DC blocker, soft limiter, peak meter
- 1x/2x/4x oversampling 선택 (drive nonlinear stage)
- APVTS 파라미터와 project/preset 상태 XML 저장
- Init과 5개 신스 패치(Supersaw Pad, Trance Pluck, Arena Lead, Sub Mono Bass, Acid Bass)
- processBlock 내 동적 할당·I/O·UI 호출 없음

## 신호 경로

`MIDI Note On/Off → voice allocation → Oscillator pair → mixer → per-voice filter drive → per-voice LPF → ADSR → output`

각 voice는 oscillator phase, envelope, filter state를 독립적으로 보유한다. voice allocator는 idle voice, release voice, oldest voice 순서로 선택한다. Pitch bend, aftertouch, sustain pedal, CC mapping과 MIDI Learn은 처리하지 않는다.

## 다음 구현 순서

1. **음질 계측**: 44.1/48/96/192 kHz 스윕, 20 Hz–20 kHz aliasing 측정, resonance 안정성·NaN/Inf·DC 검사, 고정 seed offline render 비교.
2. **Phase 3 후속**: 더 정교한 AM/ring-mod 모드와 modulation matrix 편집.
3. **출시 품질**: pluginval VST3 validator 경로 연동, macOS 서명/notarization, Windows x64 CI와 설치 패키지, 주요 DAW offline bounce 비교.

## 실시간 안전 규칙

오디오 스레드에서는 메모리 할당, 파일·네트워크 I/O, mutex, UI 접근을 금지한다. APVTS raw parameter는 블록 시작 시 읽고, 음질에 민감한 값은 voice 내부에서 sample-rate 기반으로 smoothing한다. meter 데이터가 필요해지면 lock-free FIFO 또는 VST3 Data Exchange만 사용한다.

## 품질 게이트

각 단계는 컴파일만으로 완료하지 않는다. 오디오 출력에 NaN/Inf가 없는지, DC offset이 제한되는지, resonance runaway가 없는지, block size 변경과 MIDI note-on/off가 안전한지 자동 검사한다. 출시 후보는 Cubase, Ableton Live, REAPER, Logic에서 preset 저장/복원과 offline bounce 일치 여부를 확인한다.

현재 자동 검증 명령:

```sh
python3 scripts/run_dsp_sanity.py
PLUGINVAL_BIN=/path/to/pluginval ./scripts/run_pluginval.sh
```
