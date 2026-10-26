# 2026-10-27 AU 신원 마이그레이션 검증 (EEF LABs)

## 배경

커밋 `4a180cb` (brand: EEF LABs manufacturer and bundle identity)에서 제조사
코드가 `EonA` → `EEFL`, 번들 id가 `com.eonaudio.eefjp8000` →
`com.eeflabs.eefjp8000`으로 바뀌었다. AU 컴포넌트 신원은 `aumu MnEf EonA`에서
`aumu MnEf EEFL`로 이동한다. 이 리포트는 그 전환을 실측으로 남긴다.

## 설치·검증 절차

```sh
# 구버전 백업(파일명 관례 유지) 후 새 빌드 설치
mv ~/Library/Audio/Plug-Ins/Components/EEF-JP8000.component \
   ~/Library/Audio/Plug-Ins/Components/.EEF-JP8000.component.bak-20261027-064013
cp -R Build/EonMiniEEF_artefacts/Release/AU/EEF-JP8000.component \
      ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar   # AU 레지스트리 리스캔
```

설치된 컴포넌트 확인: `CFBundleIdentifier = com.eeflabs.eefjp8000`.

## 결과

| 검사 | 결과 |
| --- | --- |
| `auval -v aumu MnEf EEFL` | **AU VALIDATION SUCCEEDED** (44.1 kHz, render·파라미터·레이프 일정·MIDI 전부 PASS) |
| `auval -v aumu MnEf EonA` | FAIL — "FATAL ERROR: didn't find the component" (구신원 소멸 확인) |

## 롤백 방법

```sh
rm -rf ~/Library/Audio/Plug-Ins/Components/EEF-JP8000.component
mv ~/Library/Audio/Plug-Ins/Components/.EEF-JP8000.component.bak-20261027-064013 \
   ~/Library/Audio/Plug-Ins/Components/EEF-JP8000.component
killall -9 AudioComponentRegistrar
```

## 남은 것

- DAW 호스트 로드(Live/Logic/Cubase)와 청취는 여전히 수동 단계다. 구 신원으로
  저장된 프로젝트는 missing plugin 처리 후 재삽입해야 한다.
- pluginval은 이번에 실행하지 않았다. CI(`631e0e4` 이후)가 VST3 경로를 담당한다.
