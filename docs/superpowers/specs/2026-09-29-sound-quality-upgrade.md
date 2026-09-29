# EEF-JP8000 음질 업그레이드 계획 2차 (2026-09-29)

## 목표와 근거

1차 아날로그 품질 로드맵(docs/superpowers/specs/2026-09-17-analog-quality-roadmap.md)의
A~F 단계가 모두 완료됐다. 이 문서는 현재 코드(Source/PluginProcessor.cpp) 리뷰와
기존 측정 리포트를 근거로 다음 라운드의 음질 향상 항목을 우선순위로 정리한다.
청감 우수성 주장 없이 단계마다 수치 게이트와 호스트 게이트를 분리한다.

## 현재 상태 요약 (코드 확인 사실)

- 오실레이터: 1차 PolyBLEP saw/pulse + triangle + sine, 4기, 노이즈(모노).
  blep 보정은 1차이며 상한 주파수에서 자연 감쇠한다.
- 유니즌: 최대 8레이어, detune/spread/결정적 random-walk drift(detune cents만).
- 필터: 보이스당 스테레오 TPT SVF, 12 dB/oct LP/HP/BP, 선형.
  safeCutoff > 0.28*sr 또는 resonance > 0.72일 때 내부 2x 서브스텝.
- 엔벨로프: juce::ADSR(선형 구간) 1개, amp 전용. 매트릭스 소스로 재사용 가능.
- 모듈레이션: LFO 1개(sine/tri/S&H), 매트릭스 소스 {LFO, Amp Env, Velocity, Osc1},
  목적지 {Pitch, Cutoff, Amp, Osc2 FM}. PWM 목적지 없음.
- 글로벌: 1x/2x/4x 오버샘플 drive(SYM/ASYM/TUBE), damped feedback delay,
  quad-tap chorus, 4-comb+2-allpass reverb, DC blocker, ampSat, soft limiter.

## 측정 기준선 (실측)

- ThdProbe 48 kHz: clean THD 0.0029%, drive 스윕/커브별 H2/H3/H5/DC 기록됨,
  DC -58 dB 이하 유지.
- 에일리어싱 게이트: PresetSmoke 딥클립 드론 + fold-line(21.6k/18.08k) 검사.
- dsp-sanity: 44.1/48/96/192 kHz PASS, CTest 3/3, pluginval s5, auval PASS.
- 미완료 게이트: 설치 VST3가 최신 빌드와 해시 불일치, REAPER 렌더 미완,
  Ableton 로드·청취 미실시, soundcraft 매니페스트 미작성.

## 단계

### P1. 보이스 코어 — 인지 효과가 가장 큰 항목

1. **필터 입력 드라이브 + 24 dB 모드 (최우선)**
   - 변경: 보이스 필터 앞에 bounded pre-drive(tanh 계열, 소량) 추가.
     filterMode에 24 dB LP 추가 — 두 개의 TPT SVF 직렬(선형, 안정적)로
     구현해 4극 저역통과를 얻는다. supersaw/lead 음색 변화가 가장 크다.
   - 위험: 필터 내부 비선형은 에일리어싱을 만든다. pre-drive는 기존
     선택적 2x 서브스텝과 같은 프레임 안에서 두 번 평가해 억제한다.
     비선형 4극 래더는 별도 실험으로 분리한다(기본 채택 아님).
   - 게이트: ThdProbe에 필터 스윕 케이스 추가(cutoff 스윕 중 배음/에일리어싱),
     resonance 극한 안정성, 44.1~192 kHz dsp-sanity, PresetSmoke.

2. **지수(RC형) 엔벨로프 옵션**
   - 변경: 선형 juce::ADSR을 지수 커브 ADSR로 교체하거나 curve 파라미터 추가.
     attack/decay/release를 exponential 형태로. pluck·bass의 타격감 변화.
   - 위험: 기존 프리셋의 엔벨로프 타이밍이 바뀐다. 기본값에서 구형과
     최대한 가깝게 정합하거나 "Env Curve" 파라미터 기본 0=legacy로 둔다.
   - 게이트: 고정 노트 렌더의 엔벨로프 피크/시정수 오프라인 비교, PresetSmoke.

3. **필터 전용 엔벨로프(ADSR2→cutoff) 또는 매트릭스 목적지 확장**
   - 변경: 작은 두 번째 ADSR + env2Amount→cutoff 라우팅, 또는 비용 최소
     대안으로 매트릭스 목적지에 PWM/FilterEnv 추가. 현재 단일 amp env만
     있어 필터 스윕이 LFO 의존이다.
   - 게이트: 매트릭스 라우팅 단위 검사, 상태 저장/복원 호환.

### P2. 아날로그 편차와 시그니처 음색

1. **보이스별 편차(voice variance)**
   - 변경: 폴리 보이스별 고정 seed 기반 cutoff/env-time/level 미세 오프셋
     (unison drift와 별개, 보이스 카드 편차 모델). amount 0=비트 동일.
   - 게이트: seed 고정 오프라인 재현성, 오프셋 상한 문서화.

2. **노이즈 스테레오 비상관 + unison 위상/레벨 drift 확장**
   - 변경: 노이즈 좌우 독립 seed(현재 동일 샘플), unison 레이어에 cents
     외 level/phase drift 소량 추가.

3. **PWM 목적지 + 오실레이터 싱크/링모듈 후보**
   - 변경: 매트릭스 목적지에 Pulse Width 추가(LFO→PWM은 JP 계열 대표
     음색). osc hard-sync 또는 OSC1×OSC2 오디오 레이트 ring-mod는
     DESIGN.md Phase 3 잔여분으로 채택 여부를 실험 렌더로 판단한다.

### P3. 이펙트 질감

1. reverb 확산 개선: allpass/comb에 느린 변조 주입(금속성 잔향 감소),
   size/decay 파라미터 범위 재검토. 알고리즘 전면 교체는 하지 않는다.
2. delay 스테레오 폭: ping-pong 또는 좌우 시간 오프셋 옵션.
3. ampSat/limiter를 오버샘플 경로 내부로 이동 검토: 현재 host-rate에서
   동작하며 큰 transient에서 1x tanh가 미세하게 접힌다. 오버샘플 모드일
   때만 적용 위치를 옮기고 1x 경로는 유지한다.

### P4. 측정·호스트 증거 정비

1. soundcraft 매니페스트 작성(--render-mode synth, midi_note 시나리오)로
   설치 VST3 회귀 기준 고정. 리포트에 binary/host SHA-256, run_id,
   샘플레이트/블록/채널 기록.
2. 설치 번들을 최신 빌드로 갱신한 뒤 REAPER 격리 프로필 렌더
   (scripts/reaper_eef_instrument_smoke.lua) 재시도, Ableton 로드,
   레벨 매칭 청취 팩을 별도 증거로 남긴다.
3. DESIGN.md "음질 계측" 잔여: 20 Hz–20 kHz saw 스윕 에일리어싱 측정을
   도구로 추가해 PolyBLEP 상한 응답을 수치로 확정한다.

## 검증 게이트 (모든 단계 공통)

1. tools/PresetSmoke + scripts/run_dsp_sanity.py + CTest 통과.
2. ThdProbe/스펙트럼 신규 케이스 PASS(기준선 대비 비교 표 기록).
3. Release 빌드 + pluginval strictness 5 + auval.
4. 보고서에 바이너리 SHA-256, 샘플레이트, 블록 크기 명시.
5. 호스트 로드·청취는 마지막에 별도 게이트로 보고한다.

## 하지 않을 것

- 기존 파라미터 ID/기본값 파괴(모든 신규 파라미터는 중립 기본값으로
  레거시 상태와 비트 동일 유지).
- 오디오 스레드 동적 할당·락.
- 자동화 없는 청감 주장, 특정 상용 신디의 회로/프리셋 복제.

## 진행 상태

- P1.1 필터 드라이브 + 24 dB LPF24: 완료 (reports/2026-09-29-filter-drive-lpf24.md)
- P1.2 지수형 앰프 엔벨로프: 완료 (reports/2026-09-29-envelope-curve.md)
- P1.3 필터 전용 엔벨로프 + PWM 목적지: 완료 (reports/2026-09-29-filter-envelope-pwm.md)
- P2.1 보이스별 편차 (컷오프/엔벨로프 시간/레벨): 완료
- P2.2 노이즈 스테레오 비상관 + 유니즌 레벨 drift: 완료
- P2.3 hard sync / ring-mod: 제외 (보고서 근거 참조)
- P2: 완료 (reports/2026-09-30-voice-variance-stereo-noise.md)
- P3.1 리버 comb 변조: 완료
- P3.2 델레이 스테레오 폭: 완료
- P3.3 ampSat/limiter를 오버샘플 경로로 이동: 완료
- P3: 완료 (reports/2026-09-30-reverb-delay-oversampled-amp.md)
- P4 이하: 미착수
