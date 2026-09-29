# 지수형 앰프 엔벨로프 (음질 업그레이드 2차 P1.2)

엔벨로프에 지수(RC형) 곡선 옵션 `envCurve`(0..1, 기본 0)를 추가했다.
juce::ADSR 상태 머신과 각 스테이지 타이밍은 그대로 두고, 보이스에 전달되는
값만 곡선화한다. 기본값 0에서는 분기를 타지 않아 기존 선형 응답과 동일하다.

## DSP 구조

- 상승(어택): `1 - (1 - y)^γ` — 초반이 빠르고 피크로 완만하게 붙는다.
- 하강(디케이): `s + (1 - s) * ((y - s) / (1 - s))^γ` — 서스테인 레벨 s로
  수렴하는 지수 감쇠라 디케이 끝과 서스테인 시작 사이에 레벨 점프가 없다.
- 하강(릴리즈): `r0 * (y / r0)^γ` — 노트 오프 시점에 실제로 유지하던 레벨
  r0을 기준으로 삼아 릴리즈 시작에서 불연속이 생기지 않는다.
- γ = 1 + 3·envCurve. 상승/하강 방향은 직전 raw 값과의 비교로 판별하고,
  노트 시작과 상태 로드에서 기준값을 초기화한다.
- 어택/디케이/릴리즈 시간은 juce::ADSR이 그대로 관리하므로 0.2초 어택은
  곡선을 켜도 정확히 0.2초에 피크에 도달한다. 바뀌는 것은 도달 경로다.

## 측정 (ThdProbe, sine 1760 Hz @ 48 kHz, attack 0.2 s / sustain 1.0 / release 0.2 s)

| envCurve | peak | atk 25% | atk 75% | sustain | rel 25% | rel 75% |
|---|---|---|---|---|---|---|
| 0 (선형) | 0.109574 | 0.027624 | 0.082373 | 0.109574 | 0.082447 | 0.027727 |
| 1 (곡선) | 0.109574 | 0.075270 | 0.109133 | 0.109574 | 0.035257 | 0.000451 |

이론값과의 대조:

| 지점 | 선형 기대 | 곡선 기대(γ=4) | 측정(곡선) |
|---|---|---|---|
| atk 25% | 0.25 × peak = 0.0274 | (1 - 0.75⁴) = 0.684 × peak = 0.0750 | 0.07527 |
| atk 75% | 0.75 × peak = 0.0822 | (1 - 0.25⁴) = 0.996 × peak = 0.1092 | 0.10913 |
| rel 25% | 0.75 × peak = 0.0822 | 0.75⁴ = 0.316 × peak = 0.0347 | 0.03526 |
| rel 75% | 0.25 × peak = 0.0274 | 0.25⁴ = 0.0039 × peak = 0.00043 | 0.00045 |

서스테인 레벨은 두 경우 모두 0.109574로 동일하다. 곡선을 켜도 홀드 구간
레벨이 변하지 않는다는 뜻이며, 이것이 게이트로 고정돼 있다.

## 게이트 결과

- `EonMiniEEF_PresetSmoke`(CTest): PASS, `realtime_audio_contract`: PASS
- `python3 scripts/run_dsp_sanity.py`: PASS (44.1/48/96/192 kHz)
- CTest: 3/3 PASS
- `EonMiniEEF_ThdProbe`: 기존 6 + P1.1 3 + 신규 4
  (envCurveAttackFaster / envCurveReleaseFaster / envCurveSustainHeld /
  envCurveBounded) 전부 PASS
- pluginval 1.0.4 strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED
- UI 스냅샷: AMP ENVELOPE 섹션에 ENV CRV 노브가 추가돼 5노브로 렌더된다.

Release 바이너리 SHA256:

- VST3 Mach-O: `94097639b906d15ae526ce1b01c0f1885aaf053675529ad1b4a487ff5a3d2fd9`
- AU Mach-O: `c446ebd4ea9fce28b0d3450f27ed62c8508eb30dc30d7cbd913c3d7f5d359bee`
- 측정 조건: 48 kHz / 256 samples

## 증명하지 않는 것

이 측정은 곡선 엔벨로프의 청감 우위를 증명하지 않는다. CPU 비용, 호스트 로드,
레벨 매칭 청취는 별도 게이트다. 또한 엔벨로프 곡선은 어택/디케이/릴리즈
타이밍을 바꾸지 않으므로, 기존 프리셋의 엔벨로프 '길이'는 그대로 유지된다.

