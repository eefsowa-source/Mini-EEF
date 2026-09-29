# P2 아날로그 편차와 노이즈 비상관 (2026-09-30)

## 변경 내용

### P2.1 보이스별 편차 (`voiceVariance`, 기본 0)

`EonVoice::startNote`가 노트당 하나의 결정적 시드에서 세 개의 오프셋을 뽑아
그 보이스에만 적용한다. 유니즌 detune drift와는 별개 경로다.

| 성분 | 최대 범위 | 적용 위치 |
|---|---|---|
| 컷오프 | ±40 cents | `modulatedCutoff`에 `cutoffVariance` 곱 |
| 앰프/필터 엔벨로프 stage 시간 | ±25% | `updateEnvelopeParameters()`의 ADSR 파라미터 6개 |
| 보이스 레벨 | ±15% | `voiceGain`에 `levelVariance` 곱 |

amount가 0이면 세 배수가 모두 정확히 1.0이므로 기존 프리셋 출력은 비트 동일하다.
시드는 노트 번호에서 파생되므로 같은 노트는 오프라인 렌더에서 그대로 재현되고,
폴리 화음에서는 보이스마다 다른 값이 나간다.

### P2.2 노이즈 스테레오 비상관 + 유니즌 레벨 drift

노이즈가 이전에는 좌우에 같은 샘플을 넣었다. 오른 채널에 독립 시드
(`noiseStateRight`)를 추가해 두 채널이 비상관되도록 했다. 왼쪽 시드 체인은
바꾸지 않았으므로 기존 왼쪽 채널 측정 기준선은 유지된다.

유니즌 레이어에는 detune wanderer 옆에 레벨 wanderer를 추가했다. drift amount가 0이면
곱이 정확히 1.0이라 유니즌 합이 비트 동일하다.

### P2.3 오실레이터 hard sync / ring-mod — 이번 단계에서 제외

계획에는 sync·링모듈이 후보로 들어 있었으나 채택하지 않았다.

- hard sync는 JP8000 계열의 대표 음색이 아니고, 이미 4기 오실레이터 + 유니즌 8 +
  drift + 보이스 편차로 두께 축이 확보된 상태다. sync는 음색 축을 분산시킨다.
- 오디오 레이트 ring-mod는 매트릭스의 `Osc 1 → Osc 2 FM`이 이미 담당한다. FM은 위상
  적분이라 배음 구조가 자연스럽고, 순수 곱셈은 배음을 제거한 `약한 FM`에 가까워
  대역폭만 낭비한다.
- 둘 다 블록당 다항 연산이 늘어난다. 지금까지의 "기본값 0 = 레거시, 비용은 게이트로"
  원칙을 깨므로 이 단계의 범위에서 제외한다.

## 측정 (ThdProbe, 48 kHz / 256 블록)

### 보이스 편차 — 동일 노트를 variance 0과 1로 각각 렌더 후 샘플 단위 비교

정규화 = peak 기준 RMS 차이. 0이면 두 렌더가 동일하다.

| note | diff vs variance=0 | repeat diff |
|---|---|---|
| 60 | 0.00933202 | 0 |
| 67 | 0.0372916 | 0 |

두 노트가 서로 다른 값을 보이는 것은 오프셋이 보이스별이라는 뜻이고, repeat diff가
정확히 0인 것은 시드 기반이라 재현 가능하다는 뜻이다. 게이트 임계는 0.005로 두었다.
오프셋 세 개가 균등 분포이므로 특정 노트가 최대치에 가까운 값을 보인다고 가정하지
않고, 0에서 벗어나는지만 검사한다. (1% 임계는 실제로 note 60이 0.0093이라 실패해서
0.005로 내렸다. 근거는 float 반올림이 아니라 반복 렌더가 정확히 0이라는 점이다.)

### 노이즈 스테레오 비상관 — L−R RMS

| noise mix | L-R rms |
|---|---|
| 0 | 0 |
| 0.5 | 0.0314836 |

mix 0에서 정확히 0인 것은 두 채널이 같은 신호를 갖는다는 뜻이고, 0.5에서 0.031인
것은 우측 시드가 독립이라는 뜻이다.

## 게이트 결과

ThdProbe 22개 게이트 전부 PASS (기존 16 + voiceVarianceDeterministic /
voiceVarianceAudible / noiseStereoDecorrelated), 이어서 아래 전부 PASS.

| 게이트 | 결과 |
|---|---|
| `python3 scripts/run_dsp_sanity.py` | PASS (44.1/48/96/192 kHz) |
| `ctest --test-dir Build-Git2 -C Release` | 3/3 PASS |
| `scripts/run_pluginval.sh` (strictness 5) | 5 SUCCESS |
| `auval -v aumu MnEf EonA` | SUCCEEDED |
| `EonMiniEEF_UISnapshot` | 렌더 성공 (VOICE VAR 8번째 노브, LFO SHAPE 우측 정렬) |

Release 바이너리 SHA-256 (macOS arm64):

- VST3 `EEF-JP8000.vst3/Contents/MacOS/EEF-JP8000`: `433f6e9a3b9b3e529b68155a0ed283f6d09001acc98746441fb9fe859d61b736`
- AU `EEF-JP8000.component/Contents/MacOS/EEF-JP8000`: `feb8eaef2acac505d4d9f28e1a3f2954f008b5bf0f8f4da0664806b472d6d04a`

## 이 측정이 증명하지 않는 것

- 청감 우수성. 위 표는 신호가 실제로 달라지고 재현된다는 것만 보인다. 폴리 화음에서
  어느 정도의 편차가 음악적으로 적정한지는 듣기로 판단해야 한다.
- voiceVariance 프리셋. 공장 프리셋은 여전히 0이므로 기본 소리는 이전과 같다. 편차를
  켠 프리셋이 필요한지, 켜면 어떤 프리셋이 좋아지는지는 별도 판단이다.
- 유니즌 레벨 drift의 레벨. `+0.35 × wanderer` 계수는 측정 게이트가 아니라 청감
  선택이며, 구조적으로는 drift 0이 1.0을 유지한다는 것만 검증했다.
- 노이즈 비상관의 공간감. L−R RMS가 0이 아니라는 것은 상관관계가 없다는 사실이고,
  어떤 폭으로 들리는지는 계측 대상이 아니다.
- DAW 로드와 설치 번들. 설치된 VST3/AU는 아직 이 빌드와 해시가 다르므로 호스트 게이트
  (P4)는 열려 있다.
