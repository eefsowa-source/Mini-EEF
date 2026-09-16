# 드라이브 커브 선택 + 앰프 새추레이션 (로드맵 B 단계 완료)

측정: tools/ThdProbe.cpp 커브 비교 섹션 (drive=1, 4x, sine 1760 Hz @ 48 kHz).
새 파라미터: driveCurve(Symmetric/Asymmetric/Tube, 기본 Symmetric),
ampSat(0..1, 기본 0).

| curve | THD % | H2 dB | H3 dB | H5 dB | DC dB |
|---|---|---|---|---|---|
| SYM (기준) | 2.36 | -131.6 | -32.6 | -63.4 | -57.9 |
| ASYM | 9.65 | -20.4 | -38.4 | -89.3 | -57.8 |
| TUBE | 5.27 | -132.1 | -25.6 | -48.4 | -57.8 |

## 설계와 발견

1. ASYM은 78%/122% 비대칭 전도로 H2를 -20.4 dB까지 올린다(짝수 배음 =
   튜브 앰프 특성). 첫 측정에서 DC -6.5 dB로 게이트가 잡아냈고, 커브 내부
   원폴 DC 킬러(~5 Hz, 상태 asymHpX/Y)를 추가해 -57.8 dB로 회복했다.
   상태는 prepareToPlay와 dspResetRequested 블록에서 resetDriveCurveState()로
   초기화한다.
2. TUBE는 tanh 2단 캐스케이드로 3차 배음을 1.3 dB 강조한다. H2는
   -132 dB로 대칭 특성 유지.
3. ampSat는 |x|>0.7에서만 무른 무릎(knee)을 적용한다. amount=0이면
   비트 동일이고, 팩토리 프리셋 피크 0.408이라 클린 연주는 그대로다.
4. 기본값(Symmetric, ampSat=0)은 기존 경로와 비트 동일을 유지한다.

## 게이트 결과

THD 프로브 6개 게이트 전부 PASS(curvesDcOk 임계 -20 dB). PresetSmoke 전
항목(6 프리셋, peak 0.408), dsp-sanity(44.1/48/96/192 kHz), CTest 3/3,
pluginval strictness 5 SUCCESS. DAW 로드/청감은 별도 게이트로 미실시.
