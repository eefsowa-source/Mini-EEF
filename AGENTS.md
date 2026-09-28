# EEF-JP8000

- `DESIGN.md`가 신호 경로·실시간 안전 규칙의 기준이고 `README.md`에 빌드·검증 명령이 있다. 구현은 `Source/`, 검사 스크립트는 `scripts/`에 있다.
- DSP 변경에는 `python3 scripts/run_dsp_sanity.py`와 관련 CTest를 실행한다. 플러그인 형식은 `scripts/run_pluginval.sh`로 따로 검사한다.
- UI 스냅샷, 호스트 로드, 프리셋 복원, MIDI 동작과 청취를 별도 증거로 남긴다. 기존 `output/`·측정 자료를 새 실행 결과로 덮어쓰지 않는다.
