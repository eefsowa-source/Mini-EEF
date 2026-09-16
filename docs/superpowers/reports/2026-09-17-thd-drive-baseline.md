# Drive 단계 THD 기준선 (로드맵 A 단계 완료)

측정: tools/ThdProbe.cpp (sine 1760 Hz, 48 kHz, 32768 샘플 Blackman 창)
빌드: Build-Thd/EonMiniEEF_ThdProbe, 2026-09-17 실행 결과.

| drive | mode | THD % | H3 dB | H5 dB | DC dB |
|---|---|---|---|---|---|
| 0 | 1x | 0.0000288 | -149.1 | -154.6 | -58.0 |
| 0 | 4x | 0.0000288 | -148.9 | -154.8 | -58.0 |
| 0.25 | 1x | 0.979 | -40.2 | -78.9 | -57.9 |
| 0.25 | 4x | 0.401 | -47.9 | -94.3 | -58.0 |
| 0.5 | 1x | 2.12 | -33.5 | -65.4 | -57.9 |
| 0.5 | 4x | 0.889 | -41.0 | -80.4 | -57.9 |
| 1.0 | 1x | 5.31 | -25.5 | -49.3 | -57.8 |
| 1.0 | 4x | 2.36 | -32.6 | -63.4 | -57.9 |

(H2/H4/H6는 모두 -130 dB 이하. 대칭 tanh의 짝수 배음 억제가 정상.)

## 해석

1. 클린 신호(drive=0)는 THD 0.0029%로 측정 한계 수준. B 단계 amp 단
   saturation을 넣을 때 이 숫자를 기준으로 삼는다.
2. drive=1에서 1x THD 5.31% vs 4x THD 2.36%. 차이의 주성분은 에일리어싱이
   아니라 anti-alias 필터를 통과한 고차 배음(H5 이상) 감쇠다. 에일리어싱
   자체는 PresetSmoke의 딥클립 드론 게이트(bf8bbf9)가 관리한다.
3. DC는 모든 조건에서 -58 dB 이하로 억제된다. B 단계 asymmetric 커브
   도입 시 이 값이 오르면 별도 하이패스 정책이 필요하다는 신호로 쓴다.

## 게이트 결과

cleanSine PASS, driveAddsHarmonics PASS, dcControlled PASS.
같은 트리에서 PresetSmoke 전 항목, CTest 3/3, pluginval strictness 5
SUCCESS 확인. DAW 로드/청감은 별도 게이트로 미실시.
