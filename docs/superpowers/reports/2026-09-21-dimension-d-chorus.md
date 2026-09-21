# Dimension-D 스타일 쿼드탭 코러스 (로드맵 D 단계)

Studio D / Roland Dimension D 계열의 공간감과 저변조 코러스 동작을 참고해
기존 단일 지연 탭을 독립 스테레오 4탭 구조로 교체했다. 회로를 복각한다고
주장하지 않고, 참고한 동작 원리와 이 플러그인의 설계값을 분리한다.

## DSP 구조

1. 100 ms 링 버퍼(48 kHz 기준)를 delay/fx 라인과 분리했다. 각 샘플에서
   25 ms 중심 지연을 읽고 `depth * sin(phase)`로 작은 변조를 건다.
2. 네 탭의 LFO 위상은 0/90/180/270도다. 탭별 지연은 선형 보간으로 읽어
   블록 경계와 fractional delay에서 클릭을 피한다.
3. 좌우 출력 가중치는 각각 `[1.0, 0.8, 0.6, 0.4]`와
   `[0.4, 0.6, 0.8, 1.0]`이며 합을 2.8로 정규화한다. 이 값은 하드웨어
   회로 상수가 아니라 회전하는 출력 네트워크를 만들기 위한 구현값이다.
4. dry는 유니티로 유지하고, 쿼드탭 wet만 `chorusMix`로 더한다. 기본값
   `chorusMix=0`에서는 기존의 클린 경로를 유지한다.
5. `prepareToPlay`와 오디오 스레드 리셋 경로에서 버퍼, write position,
   LFO 위상을 함께 초기화한다. 전용 버퍼를 사용해 delayTime이 짧을 때
   코러스가 미래의 delay 인덱스를 읽던 경로도 제거했다.

## 회귀 게이트

`tools/PresetSmoke.cpp`에 쿼드탭 코러스 프로브를 추가했다. 48 kHz / 128
샘플 블록에서 지속음을 렌더해 다음을 확인한다.

- wet 신호가 실제로 dry와 달라지는지 (`differenceEnergy > 0`)
- 모든 출력이 유한한지
- dry에 wet을 더하는 구조의 정상 peak 증가를 허용하면서도 2.5배 dry 및
  0.5 절대 상한을 넘지 않는지

## 검증 결과

- `EonMiniEEF_PresetSmoke`: PASS, 6 factory presets, peak `0.408675`
- 쿼드탭 코러스: dry peak `0.0955519`, wet L/R peak `0.120095 / 0.127403`,
  difference energy `13.3116`
- `dsp-sanity.py`: PASS (44.1 / 48 / 96 / 192 kHz)
- CTest: 3/3 PASS
- pluginval 1.0.4, strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED (macOS 15.7.9)

Release 바이너리의 현재 해시는 다음과 같다.

- VST3 Mach-O: `f73475d3ce5720800ed6bc8eebe5b660276f84ca70ca2fa5b2d369531ba3f179`
- AU Mach-O: `5fd79baed2398ac37bcf5d8a0c28ec81fa65cefeb2eda52ed57e859521891de9`
- 샘플레이트/버퍼: 위 코러스 프로브 48 kHz / 128 samples; dsp-sanity는 44.1,
  48, 96, 192 kHz를 사용

REAPER/Ableton Live의 실제 로드 및 레벨 매칭 청취는 이 측정에 포함하지 않았다.
