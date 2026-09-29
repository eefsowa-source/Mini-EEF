# 필터 드라이브 + LPF24 (음질 업그레이드 2차 P1.1)

보이스 필터 앞에 전용 드라이브를 추가하고 24 dB/oct 저역통과 모드를 신설했다.
두 변경 모두 기본값에서 기존 출력과 비트 동일하며, 기존 파라미터 ID/기본값은
그대로다. 계획 문서는 docs/superpowers/specs/2026-09-29-sound-quality-upgrade.md.

## DSP 구조

- `filterDrive`(0..1, 기본 0)는 보이스 버퍼의 필터 입력에 적용하는 tanh
  컬러링이다. drive=0이면 분기를 타지 않아 레거시 입력 경로와 동일하다.
- 드라이브가 켜지면 선택적 필터 서브스텝 경로가 함께 활성화되고, 비선형을
  선형 보간한 중간점 입력과 현재 입력에서 각각 평가한다. 결과적으로 비선형이
  호스트 레이트의 2배로 샘플링되어 폴드백 에너지가 크게 줄어든다.
- 필터 모드에 `LPF24`(인덱스 3)를 추가했다. 공진을 소유한 기존 TPT 스테이지
  뒤에 damping 2.0의 평탄 TPT 스테이지를 직렬로 두어 24 dB/oct를 만든다.
  두 스테이지 모두 무조건 안정한 TPT 적분기라 새 안정성 조건이 필요 없다.
- 드라이브가 켜진 동안에는 서브스텝 필터가 항상 2회 갱신되므로 상한 근처
  계수 워핑도 함께 줄어든다. 새 상태(svfIc3/svfIc4, filterInputPrev)는
  노트 시작, 상태 로드, 프리셋 복원에서 기존 상태와 같이 초기화된다.

## 측정 (ThdProbe, sine 1760 Hz @ 48 kHz, 32768 샘플 창)

| filterDrive | filter | THD % | H2 dB | H3 dB | H5 dB | DC dB | alias deep |
|---|---|---|---|---|---|---|---|
| 0 | LPF | 0.0000281 | -131.2 | -149.1 | -156.3 | -58.0 | 6.1e-18 |
| 0 | LPF24 | 0.0000274 | -131.4 | -149.5 | -157.7 | -58.1 | 3.1e-18 |
| 0.5 | LPF | 2.706 | -130.7 | -31.4 | -61.4 | -57.9 | 1.6e-17 |
| 0.5 | LPF24 | 2.574 | -130.9 | -31.8 | -62.7 | -58.2 | 3.7e-19 |
| 1 | LPF | 6.859 | -129.9 | -23.3 | -45.1 | -57.8 | 1.8e-18 |
| 1 | LPF24 | 6.523 | -130.1 | -23.7 | -46.4 | -58.1 | 9.7e-19 |
| rolloff 4k | LPF | 1.229 | -134.0 | -38.2 | -74.3 | -60.9 | 5.0e-19 |
| rolloff 4k | LPF24 | 0.531 | -137.4 | -45.5 | -88.4 | -89.6 | 1.6e-18 |

해석:

1. filterDrive 0.5→1.0에서 THD가 2.71%→6.86%로 증가해 배음 추가가 확인된다.
   H2는 -130 dB로 대칭 커브 특성이 유지된다.
2. fold line(18.08 kHz) 에너지는 모든 조건에서 윈도 플로어(1e-17 이하)로,
   호스트 레이트 단독 평가 시 우려했던 드라이브 에일리어싱이 측정 한계
   아래에 머문다.
3. cutoff 4 kHz 비교에서 LPF24의 H5가 -88.4 dB로 LPF(-74.3 dB) 대비 14.1 dB
   더 감쇠하고, H3는 7.3 dB 더 감쇠한다. 24 dB/oct 기울기가 실제로 동작한다.
4. DC는 필터 드라이브 전 구간에서 -57.8 dB 이하로 유지된다.

## 게이트 결과

- `EonMiniEEF_PresetSmoke`(CTest `preset_state_and_audio_smoke`): PASS
- `realtime_audio_contract`: PASS
- `python3 scripts/run_dsp_sanity.py`: PASS (44.1/48/96/192 kHz)
- CTest: 3/3 PASS
- `EonMiniEEF_ThdProbe`: THD 베이스라인 6개 + 신규 3개
  (filterDriveAddsHarmonics / filterDriveBounded / lp24Steeper) 전부 PASS
- pluginval 1.0.4 strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED
- UI 스냅샷: 필터 클러스터 6노브(CUTOFF/RESO/FLT DRV/OUTPUT/DRIVE/SAT)와
  LPF24 콤보 항목이 렌더되며, 기존에 화면에 배치되지 않던 AM DEPTH 노브도
  함께 표시된다.

Release 바이너리 SHA256:

- VST3 Mach-O: `5ce7c8ac369bee7ba312c1bd69bb26d2c2d51eee88391aba88d0083c2b74cc5b`
- AU Mach-O: `e82abbc292ade9d7cb0c916eeed51596e05e1d915808520f89c0ae7dd9f2ad8f`
- 측정 조건: THD/drive/rolloff 48 kHz / 256 samples, dsp-sanity 44.1–192 kHz

## 함께 수정한 빌드 결함

1. `melatonin_inspector` FetchContent가 기본 `<name>-src` 디렉터리로 받아져
   `juce_add_module`이 모듈 헤더를 찾지 못하고 모든 재구성이 실패했다.
   `SOURCE_DIR`을 `melatonin_inspector`로 고정해 해결했다(Debug 링크 계약은 유지).
2. `Build-Git2/CMakeCache.txt`의 `JUCE_SOURCE_DIR`이 `$PWD/...` 리터럴로
   기록되어 재구성이 실패했다. 실제 `Build-Git2/_deps/juce-src` 절대경로로 교정했다.

## 증명하지 않는 것

이 측정은 새 드라이브/LPF24 음색의 청감 우위를 증명하지 않는다. CPU 비용,
호스트 로드, 레벨 매칭 청취는 별도 게이트다. 설치된 VST3/AU 번들은 아직
이 빌드로 갱신하지 않았으므로 호스트 결과는 새 해시로 다시 받아야 한다.

