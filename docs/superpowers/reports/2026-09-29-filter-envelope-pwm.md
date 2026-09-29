# 필터 전용 엔벨로프 + PWM 목적지 (음질 업그레이드 2차 P1.3)

필터 스윕이 LFO에만 의존하던 구조를 끝내고 전용 필터 엔벨로프를 추가했다.
모듈레이션 매트릭스에는 PWM 목적지를 더했다. 두 변경 모두 기본값에서 기존
경로와 동일하다.

## DSP 구조

- 필터 엔벨로프: 보이스마다 두 번째 `juce::ADSR`(`filterEnv`)을 두고
  `filterAttack/Decay/Sustain/Release`로 구동한다. cutoff에는
  `2^(filterEnvAmount * env * 5)` 배율로 적용해 최대 ±5옥타브를 스윕한다.
  `filterEnvAmount` 기본 0은 배율을 정확히 1.0으로 돌려 cutoff 경로가
  기존과 비트 동일하다.
- 엔벨로프 곡선(`envCurve`)은 앰프와 필터 엔벨로프가 같은 셰이퍼를 공유한다.
  이를 위해 P1.2의 곡선 상태를 `EnvelopeCurveState` 구조체로 정리했고,
  앰프 엔벨로프 동작은 변하지 않았다(기존 게이트로 확인).
- PWM 목적지: 매트릭스 목적지 목록에 `PWM`(인덱스 5)을 추가했다. 소스 값을
  ±0.45로 제한해 누적한 뒤 각 오실레이터의 펄스 폭에 더하고 0.05..0.95로
  다시 클램프한다. 라우팅이 없으면 누적값이 0이라 기존 펄스 폭과 동일하다.
- 새 엔벨로프는 노트 시작에서 noteOn, 노트 오프에서 noteOff, 상태 로드에서
  reset되며, prepareToPlay에서 샘플레이트를 받는다. 실시간 할당은 없다.

## 측정 (ThdProbe, 48 kHz)

필터 엔벨로프 — saw, cutoff 300 Hz, amount별 초기/정착 RMS:

| filterEnvAmount | early rms | late rms | ratio |
|---|---|---|---|
| 0 | 0.00233982 | 0.00233972 | 1.00004 |
| 1 | 0.03278 | 0.00233972 | 14.0102 |

amount 1에서 초기 구간 에너지가 14배 크고, 정착 RMS는 amount 0과 완전히
동일하다(0.00233972). 즉 엔벨로프는 초기 스윕만 만들고 정착 상태를 바꾸지
않는다. amount 0의 비율 1.00004는 스윕이 없음을 뜻한다.

PWM 목적지 — 50% 펄스, LFO→PWM 라우팅 유무에 따른 2차 배음:

| destination | H2 dB |
|---|---|
| 0 (라우팅 없음) | -125.7 |
| 5 (PWM) | -3.88 |

50% 듀티에서는 짝수 배음이 억제되어 H2가 측정 바닥이지만, PWM이 듀티를
움직이면 122 dB 상승한다.

## 게이트 결과

- `EonMiniEEF_PresetSmoke`(CTest): PASS, `realtime_audio_contract`: PASS
- `python3 scripts/run_dsp_sanity.py`: PASS (44.1/48/96/192 kHz)
- CTest: 3/3 PASS
- `EonMiniEEF_ThdProbe`: 기존 13 + 신규 3(filterEnvSweepsCutoff /
  filterEnvAmountOffStatic / pwmDestinationWorks) 전부 PASS
- pluginval 1.0.4 strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED
- UI 스냅샷: AMP ENVELOPE 패널이 2행이 되어 윗줄 AMP 5노브,
  아랫줄 FILTER ENVELOPE 5노브(F ATK/F DEC/F SUS/F REL/F AMT)로 렌더된다.

Release 바이너리 SHA256:

- VST3 Mach-O: `7b70598479e31ddd25d1e5460a296c2285263a36736a7cb5bf0e4d408126234f`
- AU Mach-O: `ca4bdb5efe37a1beec7b5d7e55adad236d310ea8c9d066453a6cd65c18593793`
- 측정 조건: 48 kHz / 256 samples

## 제약

모듈레이션 매트릭스는 아직 편집 UI가 없어 소스/목적지/양은 호스트 자동화로만
바꿀 수 있다. PWM 목적지도 같은 경로로만 접근 가능하며, 매트릭스 편집 UI는
1차 로드맵의 잔여 항목이다. 청감, CPU, 호스트 로드, 레벨 매칭 청취는 여전히
별도 게이트다.

