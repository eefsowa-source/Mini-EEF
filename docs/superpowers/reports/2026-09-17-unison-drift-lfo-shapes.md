# 유니즌 드리프트 + LFO 파형 확장 (로드맵 C 단계 완료)

새 파라미터: unisonDrift(0..1, 기본 0), lfoShape(Sine/Triangle/Sample & Hold,
기본 Sine). 두 파라미터 모두 기본값에서 기존 출력과 동일 경로다.

## 설계

1. 드리프트는 레이어별 결정적 랜덤 워크(xorshift32, note 번호 시드 체인)로
   -1..+1 범위를 유지하고, 외부 레이어일수록 드리프트 가중(0.5 + 0.5|position|)
   을 받아 안쪽 레이어는 중심을 지킨다. 원폴 스무딩 대신 작은 스텝(0.0003)
   랜덤 워크라 재트리거마다 같은 경로를 걸어 오프라인 렌더가 재현된다.
   유니즌 1에서는 기본 12센트 폴백 드리프트로 단음에도 미세 음흔들림을 준다.
2. LFO Triangle은 기존 free-running 위상을 그대로 쓰고, S&H는 LFO 사이클당
   한 번 -0.8..+0.8 표본을 뽑아 cutoff/pitch 목적지에서 계단식 움직임을
   만든다. S&H 시드도 노트 시드 체인이라 렌더 재현성이 유지된다.
3. UI: 스트립에 DRIFT 노브 추가(6->7), MOD 섹션 상단에 SINE/TRI/S&H 콤보.

## 게이트 결과 (tools/ThdProbe C 섹션 신규)

driftDeterministic PASS(동일 시드 두 렌더 maxDiff 0.0),
driftBounded PASS, lfoShapesBounded PASS. 기존 THD 6개 게이트 동시 재확인.
PresetSmoke 전 항목(peak 0.408), dsp-sanity(4 샘플레이트), CTest 3/3,
pluginval strictness 5 SUCCESS. 움직임의 청감 품질은 별도 게이트로 미실시.
