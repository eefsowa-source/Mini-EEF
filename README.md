# EEF-JP8000

독자적인 VA 기반 VST3 instrument입니다. 네 개의 오실레이터(Saw/Square는 PolyBLEP 대역 제한, Triangle/Sine), Noise, ADSR, 최대 8-voice unison, 안정화 LPF/HPF/BPF, 모노/폴리 MIDI 음성, 4-slot modulation matrix(FM/AM 포함), LFO tempo-sync·key tracking, 드라이브/DC blocker, 1x/2x/4x oversampling, delay/chorus/reverb, APVTS 상태 저장을 포함합니다. 오실레이터·필터·이펙트 파라미터는 호스트 자동화와 프리셋 저장을 지원합니다.

UI는 독자적인 다크 메탈 패널과 따뜻한 포인터/눈금 노브를 사용하는 아날로그 하드웨어 작업 흐름으로 재구성했습니다. VCO A/B, VOICE/UNISON, AMP ENVELOPE, FILTER/OUTPUT, MODULATION, GLOBAL FX를 한 화면에 배치해 사운드 설계 중 페이지 전환을 최소화합니다.

내장 패치는 Init, Supersaw Pad, Trance Pluck, Arena Lead, Sub Mono Bass, Acid Bass의 여섯 가지입니다. Init은 등록된 모든 자동화 파라미터를 각 기본값으로 되돌립니다.

## 빌드

JUCE 8.0.14 소스를 네트워크로 가져오도록 기본 설정되어 있습니다.

```sh
cmake -B Build
cmake --build Build --config Release
```

오프라인 환경에서는 로컬 JUCE 경로를 지정할 수 있습니다.

```sh
cmake -B Build -DJUCE_SOURCE_DIR=/path/to/JUCE
```

실시간 `processBlock`에는 할당, 파일 I/O, mutex, UI 호출을 넣지 않았습니다. MIDI 입력은 Note On/Off와 velocity만 처리합니다. 출시 전에는 44.1–192 kHz, 가변 block size, MIDI sample-offset 정확성, aliasing/NaN/DC 검사와 `pluginval --strictness-level 5`를 CI에서 수행해야 합니다.

## 검증

```sh
./scripts/run_pluginval.sh
```

`pluginval` 실행 파일이 PATH에 없으면 `PLUGINVAL_BIN=/path/to/pluginval`로 지정합니다. CI는 macOS에서 Release VST3를 빌드하고 strictness level 5 검사를 실행합니다.

DSP 수치 스모크 테스트(파형 유한값/DC, TPT 필터 안정성, 대표 unison 루프)는 외부 패키지 없이 실행할 수 있습니다.

```sh
python3 scripts/run_dsp_sanity.py
```
