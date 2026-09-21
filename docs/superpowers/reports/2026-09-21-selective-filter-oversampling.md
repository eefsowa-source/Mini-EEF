# 선택적 고역 필터 오버샘플링 (로드맵 E 단계)

보이스별 TPT state-variable filter에 내부 2x 스텝 경로를 추가했다. 전체
보이스 버퍼를 업샘플링하지 않고, 필터가 가장 불리한 영역에서만 두 번의
반샘플레이트 업데이트를 수행해 레이턴시와 오디오 스레드 할당을 늘리지 않는다.

## 설계

- `safeCutoff > 0.28 * sampleRate` 또는 resonance `> 0.72`일 때 선택 경로를
  활성화한다.
- 선택 경로는 `g = tan(pi * cutoff / (2 * sampleRate))`를 사용하고 같은 입력
  샘플을 두 번 처리한다. 두 번째 TPT 상태가 실제 출력이 된다.
- 저 cutoff/저 resonance에서는 기존 host-rate TPT 식을 그대로 사용한다.
- 필터 상태는 기존 보이스별 `svfIc1/svfIc2`를 재사용하며, note 시작과 state
  load reset 규칙도 바꾸지 않았다.
- 기본 cutoff/resonance에서는 선택 경로가 비활성이라 기존 CPU 경로를 보존한다.

## 검증

- `EonMiniEEF_PresetSmoke`: PASS, peak `0.408675`
- `dsp-sanity.py`: PASS (44.1 / 48 / 96 / 192 kHz)
- CTest: 3/3 PASS
- THD/drive/analog motion probe: PASS
- pluginval 1.0.4 strictness 5: SUCCESS
- `auval -v aumu MnEf EonA`: AU VALIDATION SUCCEEDED (macOS 15.7.9)

48 kHz THD probe에서 고 cutoff 설정은 선택 필터 경로를 통과했으며, 전체
THD 및 DC 게이트는 유지됐다. 이 측정은 고역 필터의 청감 우수성을 증명하지
않으며, REAPER/Ableton Live 로드와 레벨 매칭 청취는 별도 게이트다.

Release 바이너리 SHA256:

- VST3 Mach-O: `0885343004d0d6b8d6e9669020364641acd48acd2b83070ce25ae61d0e8604c4`
- AU Mach-O: `2952acb61f691d1c6cf913d5197be9f54bb725457f9827c107cd9292794a1d86`
- 측정 샘플레이트/버퍼: THD 48 kHz / 256 samples; CTest DSP sanity는
  44.1, 48, 96, 192 kHz를 사용
