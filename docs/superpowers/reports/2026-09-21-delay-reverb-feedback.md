# Delay feedback + damped reverb (로드맵 F 단계)

기존 feedback delay의 무감쇠 누적과 4샘플 reverb 상태를 정리했다. delay는
반복될수록 고역이 줄고 아주 약하게 포화되며, reverb는 고정 크기 버퍼 안에서
4개 damped comb와 2개 allpass를 거쳐 확산된다.

## DSP 구조

- delay feedback에는 one-pole damping(`0.45`)을 적용한 뒤
  `tanh(x * (1 + 0.35 * feedback))`를 정규화해 feedback copy만 부드럽게
  포화한다. dry write 값과 delay time/latency 계약은 유지한다.
- reverb comb 길이는 48 kHz 기준 `149/211/263/293` samples, allpass는
  `31/47` samples다. `prepareToPlay`에서 샘플레이트에 맞춰 길이를 스케일하고
  최대 2048 samples 고정 배열을 사용한다.
- comb feedback은 `0.78`, comb damping은 `0.25`, allpass gain은 `0.5`다.
  좌우 입력은 0.78/0.22 비율로 교차 주입해 stereo decorrelation을 만든다.
- reverb 상태는 prepare/reset 양쪽에서 모든 comb/allpass 버퍼, 위치,
  damping 상태를 초기화한다. processBlock 안에서는 할당이나 resize를 하지 않는다.

## 검증 결과

- `EonMiniEEF_PresetSmoke`: PASS, 6 factory presets, peak `0.402709`
- ambience 장시간 경로: 44.1/48/96/192 kHz와 delay feedback `0.9`, chorus/reverb
  mix `1.0` 조건에서 finite 및 retrigger 출력 PASS
- `dsp-sanity.py`: PASS (44.1 / 48 / 96 / 192 kHz)
- CTest: 3/3 PASS
- THD/drive/analog motion probe: PASS
- pluginval 1.0.4 strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED (macOS 15.7.9)

이 측정은 decay texture가 실제 하드웨어와 같다는 증거가 아니며, REAPER/Ableton
Live 로드와 레벨 매칭 청취는 별도 게이트다.

Release 바이너리 SHA256:

- VST3 Mach-O: `30f909c9369f846eeceb34db0db8f12c8670f3ba12980bc80374adad7572cad4`
- AU Mach-O: `cc8f866ddbf165df4adb09d55aa3fb902e04d70850dd72a71575bf3119e5ec2e`
- 측정 샘플레이트/버퍼: ambience 44.1/48/96/192 kHz; THD 48 kHz / 256 samples
