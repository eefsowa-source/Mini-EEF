# EEF-JP8000 아날로그 품질 로드맵 (2026-09-17)

## 목표와 근거

이 문서는 코드 리뷰(Source/PluginProcessor.cpp)와 커밋 757aca0, bf8bbf9의
에일리어싱 게이트 이력을 근거로, 기존 구현을 보존하면서 아날로그적인
nonlinear 응답·배음 구조·질감을 단계적으로 끌어올리는 계획이다.
품질 판정은 청감, 스펙트럼/THD 측정, CPU, DAW 호스트 확인으로 분리한다.

## 리뷰 결론

이미 갖춘 기반: PolyBLEP saw/pulse, 4 osc + unison, TPT SVF,
2x/4x oversampled tanh drive, 고정 지연 보상, DC blocker + 안전 리미터.

한계:
1) 비선형이 drive 전역 1단뿐이고 필터·앰프 단에는 saturation이 없다.
2) 배음 구조가 균일하다. 레이어 간 미세한 level·phase 음변동과 per-osc
   saturation이 없어 아날로그 특유의 음색 변화가 단조롭다.
3) LFO가 sine뿐이고 modulation matrix 소스도 제한적이다.
4) 이펙트 체인은 기능 수준이며 질감 목적 설계가 아니다.

## 단계

### A. 측정 기반 정비 (우선)

1. THD+N 게이트 도구: 특정 음높이에서 drive별 배음 스펙트럼(2f,3f,4f),
   aliasing 에너지, DC 성분을 수치로 기록한다. 기존 aliasing 게이트를 확장한다.
2. drive 커브 비교: tanh 대비 asymmetric tanh, diode-style soft clip 등
   후보 커브를 오프라인 렌더로 비교해 배음 비율 데이터를 남긴다.
3. 결과를 보고서로 저장하고, 그 기준선 위에서 아래 단계를 채택/기각한다.

### B. 드라이브/새추레이션 질감 고도화

1. drive 커브 선택 파라미터 추가(tanh / asymmetric / tube-style 등).
2. asymmetric 커브는 DC 오프셋이 생기므로 기존 DC blocker로 억제되는지
   측정으로 확인하고, 필요 시 전용 하이패스 정책을 문서화한다.
3. 필터 후 또는 amp 단에 낮은 양의 soft saturation(knee가 완만한 커브)을
   넣어 클린 상태에서도 질감을 유지한다. bypass 규칙은 현재와 동일하게
   오디오 스레드 할당 없이 구현한다.

### C. 배음·음변동 구조

1. per-oscillator gain staging과 saturation 옵션을 검토한다.
2. unison voice 간 아주 작은 level/phase/pitch 음변동(drift)을 선택적으로
   추가한다. seed 고정 옵션은 오프라인 렌더 재현성을 유지한다.
3. LFO 파형에 triangle/random-walk S&H 후보를 추가해 modulation matrix의
   cutoff 목적지와 결합해 움직임을 넓힌다.

### D. 이펙트 질감 정비

1. chorus는 per-oscillator modulation에 흡수하거나 질감 목적 파라미터로
   재설계한다(지연 보간 품질 포함).
2. reverb는 알고리즘 교체 없이 조절 가능한 damping·pre-delay 정도만
   좁게 확장해 기존 프리셋 호환을 유지한다.

## 검증 게이트

각 단계마다:

1. regression/spectrum 테스트 추가 → 기존 PresetSmoke·dsp-sanity 유지.
2. Release 빌드 + CTest + pluginval(strictness 5) 갱신.
3. 오프라인 렌더 WAV 비교로 스펙트럼 변화를 수치로 기록.
4. REAPER와 Ableton Live 로드 확인은 청감 단계와 분리해 보고.

## 하지 않을 것

1. 프리셋/파라미터 ID 대규모 파괴(레거시 상태 마이그레이션 깨기).
2. 오디오 스레드에서의 동적 할당·락 도입.
3. 자동화되지 않은 청감 주장이나 DAW 동작 일반화.
