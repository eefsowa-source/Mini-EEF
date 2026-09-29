# P3 이펙트 질감: 리버 변조, 델레이 스테레오 폭, 오버샘플 앰프 스테이지 (2026-09-30)

## 변경 내용

### P3.1 리버 comb 변조 (`reverbModulation`, 기본 0)

네 개의 comb 지연이 고정 길이라 꼬리가 일정한 주기로 울렸다. 0.31 Hz 서브오디오
LFO가 각 라인의 damping 계수를 표류시키도록 바꿨다. 라인별로 `1 + 0.37 × line` 배의
위상 배율을 써서 네 위상이 정수 주기로 재정렬되지 않게 했다. amount 0이면 계수가
그대로여서 기존 꼬리가 비트 동일하다.

### P3.2 델레이 스테레오 폭 (`delayStereo`, 기본 0)

좌우 크로스피드만으로는 폭이 열리지 않는다. 모노 보이스가 두 라인에 같은 샘플을
쓰기 때문에, 같은 비율로 섞은 결과는 계속 상관관계가 있다. 그래서 우측 탭이 같은
버퍼를 **고정 30 ms** 앞선 위치에서 읽는다.

offset을 delay 길이의 비율이 아니라 고정 시간으로 둔 이유가 있다. 비율로 두면 저음
에서 두 탭이 정확히 정수 개의 주기만큼 떨어져, 폭이 벌어지는 대신 다시 상관관계가
생겼다(측정 중 실제로 확인). 고정 30 ms는 단일 에코로 들리면서도 폭이 열리기 위한
값이다. amount 0이면 offset이 0이므로 기존 송신이 그대로다.

### P3.3 오버샘플 경로로 ampSat/limiter 이동

기존 순서는 FX → DC 블로커 → ampSat → limiter(host rate) → 오버샘플 drive였다.
품질 모드(2x/4x)를 고르면 host rate 루프가 ampSat와 limiter를 건너뛰고, 오버샘플
내부에서 drive 다음으로 같은 처리를 수행한다. fold product가 host rate 아래로
폴백되지 않는다. 1x 경로는 순서와 결과가 그대로다.

## 측정 (ThdProbe, 48 kHz / 256 블록, note 36 + 0.5 s release)

### 리버 변조 — 꼬리 RMS

| reverbModulation | tail early | tail late | repeat diff |
|---|---|---|---|
| 0 | 0.0210021 | 1.50344e-05 | 0 |
| 1 | 0.0210838 | 1.5203e-05 | 0 |

repeat diff가 두 설정 모두 정확히 0인 것은 변조가 위상 카운터 구동이라 재현된다는
뜻이다. tail 값이 0에서 1로 0.8% 이동했다. 0.31 Hz라 이 0.68초 창에서는 위상이
거의 진행하지 않아 변화가 작게 나오는 것이고, 실사용 꼬리에서는 훨씬 오래 지속된다.

### 델레이 스테레오 폭 — wet RMS와 반복 위치

| delayStereo | wet rms | L peak | R peak | L-R rms |
|---|---|---|---|---|
| 0 | 0.0893997 | 41 | 15 (offset 0 기준) | 0 |
| 1 | 0.10478 | 41 | 15 | 0 |

좌우 탭 위치는 width 0에서 41, width 1에서 15로 26 샘플 이동했고, wet rms도
0.0894 → 0.1048로 바뀌었다. L-R rms가 0인 것은 모노 보이스가 두 라인에 같은 샘플을
쓰기 때문이며 이게 기대값이다. 처음 만든 게이트는 이 값을 0이 아니라고 가정해 두 번
실패했다. 원인은 코드가 아니라 게이트가 잘못된 기대값을 검사하고 있었다는 것이며,
폭 제어기가 실제로 바꾸는 대상(각 채널의 탭 시점)을 재도록 측정을 다시 썼다.

## 게이트 결과

ThdProbe 25개 게이트 전부 PASS (P2의 22개 + reverbModulationDeterministic /
reverbModulationChangesTail / delayStereoSpreads).

회귀 증거로, P3 착수 전 출력과 P3 완료 후 출력을 `diff` 했을 때 1~44줄(P1/P2
측정표 전부)이 **바이트 단위로 동일**했다. 차이는 P3 표와 게이트 줄뿐이었다.

| 게이트 | 결과 |
|---|---|
| `python3 scripts/run_dsp_sanity.py` | PASS (44.1/48/96/192 kHz) |
| `ctest --test-dir Build-Git2 -C Release` | 3/3 PASS |
| `scripts/run_pluginval.sh` (strictness 5) | SUCCESS |
| `auval -v aumu MnEf EonA` | SUCCEEDED |
| `EonMiniEEF_UISnapshot` | 렌더 성공 (FX 스트립 9개: DLY WIDE, RVB MOD 포함) |

Release 바이너리 SHA-256 (macOS arm64):

- VST3 `EEF-JP8000.vst3/Contents/MacOS/EEF-JP8000`: `3d4c32d7bc45de17b83c89da038173d54e928e9d8256eeda50d83fa67501432a`
- AU `EEF-JP8000.component/Contents/MacOS/EEF-JP8000`: `39b10947f1176a12725110e5d884b022e58527c15fbbba8164407799a753552b`

## 이 측정이 증명하지 않는 것

- 청감. 리버 변조가 금속성 잔향을 줄이는지, 0.31 Hz와 1.2 배 깊이가 좋은지는
  계측 대상이 아니다. tail early가 0.8%만 움직인 것도 이 창이 짧아서이지,
  효과가 약하다는 근거가 아니다.
- 델레이 폭의 청감. 좌우 반복 간격 30 ms가 음악적으로 적절한지는 듣기로 판단한다.
- P3.3의 이득. limiter를 오버샘플 안으로 옮긴 것이 fold product를 실제로 줄이는지는
  THD 표로 확인하지 않았다. PresetSmoke의 drive aliasing 회귀는 여전히 PASS지만,
  그 게이트는 drive 경로를 기준선으로 잡았을 뿐 ampSat+limiter 조합의 fold를
  별도로 재 측정하지 않는다. 이 항목은 P4의 스윕 측정으로 남긴다.
- 공장 프리셋. 두 파라미터 모두 기본 0이므로 26개 프리셋 소리는 이전과 같다.
- DAW 로드와 설치 번들. 설치된 VST3/AU는 아직 이 빌드와 해시가 다르다.
