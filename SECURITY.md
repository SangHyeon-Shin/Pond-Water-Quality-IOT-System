# 보안: 인증정보 관리

## 무엇이 노출되었나

2024-05-26 ~ 2026-10의 수신기 코드(`Arduino/Communication_Regenerator-receiver.ino`)에
다음 값이 평문으로 들어 있었고, 저장소는 **public** 상태였습니다.

| 항목 | 위험 |
|---|---|
| Blynk Auth Token | 누구나 Blynk HTTP API로 가짜 값을 기록하거나 데이터를 조회할 수 있음 |
| Wi-Fi SSID·비밀번호 | 해당 공유기에 무단 접속 가능 |

현재 브랜치에서는 파일이 삭제되었지만 **git 이력·포크·캐시에는 계속 남아 있습니다.**
따라서 파일 삭제만으로는 해결되지 않습니다.

## 반드시 할 일 (소유자만 가능)

1. **Blynk 토큰 폐기**: Blynk 콘솔 → Devices → 해당 기기에서 Auth Token을 재발급합니다.
   재발급 메뉴가 없으면 기기를 삭제하고 같은 템플릿으로 새 기기를 만듭니다.
2. **Wi-Fi 비밀번호 변경**: 공유기 관리 페이지에서 변경합니다(해당 공유기를 아직 쓰는 경우).
3. 새 값은 `firmware/gateway_*/secrets.h`에만 넣습니다(아래 참조).

## 지금의 방식

- 인증정보는 각 게이트웨이 폴더의 `secrets.h`에만 둡니다.
  `secrets.example.h`를 복사해 만들며, `secrets.h`는 `.gitignore`로 커밋이 막혀 있습니다.
- `secrets.h`가 없으면 컴파일이 안내 메시지와 함께 중단됩니다.
- CI(`.github/workflows/firmware.yml`)의 `secrets-check` 작업이 `secrets.h` 커밋이나
  32자 Blynk 토큰 하드코딩을 발견하면 실패합니다.

## 선택: git 이력에서 제거

토큰을 폐기했다면 이력 정리는 필수가 아닙니다. 그래도 지우려면 다음 절차를 따릅니다.
**이력을 다시 쓰고 강제 푸시하므로**, 모든 협업자가 저장소를 새로 클론해야 하고
열린 PR이 깨질 수 있습니다.

```bash
pip install git-filter-repo
git clone --mirror https://github.com/SangHyeon-Shin/Pond-Water-Quality-IOT-System.git
cd Pond-Water-Quality-IOT-System.git
# replacements.txt 한 줄에 하나씩:  <노출된 값>==>***REMOVED***
git filter-repo --replace-text ../replacements.txt
git push --force --mirror
```

GitHub 서버의 캐시된 뷰와 이미 만들어진 포크에는 남을 수 있으므로, 이력 정리는 토큰 폐기를 대신할 수 없습니다.

## 취약점 제보

이 저장소의 보안 문제는 GitHub 저장소 소유자에게 비공개로 알려 주세요.
