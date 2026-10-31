# Blynk·게이트웨이 설정

## 1. 데이터스트림 (Blynk 템플릿)

V0~V3은 2024 대시보드와 같으므로 기존 위젯을 그대로 쓸 수 있습니다. V4~V9를 추가합니다.

| 핀 | 이름 | 형식 | 단위 | 최소 / 최대 | 소수점 |
|---|---|---|---|---|---|
| V0 | 수온 | Double | °C | −5 / 45 | 2 |
| V1 | pH | Double | pH | 0 / 14 | 2 |
| V2 | 탁도 | Double | NTU | 0 / 1000 | 1 |
| V3 | TDS | Double | ppm | 0 / 2000 | 0 |
| V4 | 용존산소 | Double | mg/L | 0 / 20 | 2 |
| V5 | 폭기 | Integer | — | 0 / 1 | — |
| V6 | 노드 공급전압 | Double | V | 0 / 6 | 2 |
| V7 | 상태 비트 | Integer | — | 0 / 65535 | — |
| V8 | 노드 연결 | Integer | — | 0 / 1 | — |
| V9 | 패킷 손실률 | Double | % | 0 / 100 | 1 |

- 2024 대시보드의 pH 게이지 범위는 0~12였습니다. 범위 밖 값이 잘리거나 버려지지 않도록 **0~14**로 넓힙니다.
- 무효 값(NaN)은 게이트웨이가 아예 보내지 않으므로 그래프에 0이나 −1000 같은 가짜 값이 생기지 않습니다.
- 상태 비트의 의미는 [calibration.md](calibration.md#상태-비트-blynk-v7-시리얼-status)를 참고하세요.
- 앱의 "Online" 표시는 **게이트웨이** 상태입니다. 연못 노드의 상태는 **V8**과 `node_offline` 이벤트로 확인합니다.

## 2. 이벤트 (알림)

템플릿 → Events에서 아래 **코드 그대로** 이벤트를 만들고 푸시 알림을 켭니다.

| 코드 | 발생 조건 |
|---|---|
| `water_quality_alert` | 교정된 센서 값이 기준을 벗어남 (설명에 값이 들어감, 예: `pH 9.50`) |
| `node_offline` | 노드 패킷이 300 s 이상 없음 (`NODE_OFFLINE_TIMEOUT_S`) |
| `node_online` | 노드 패킷 수신 재개 |

경보 기준은 각 게이트웨이의 `gateway_config.h`에 있습니다(예시값: pH 6.0~9.0, 수온 > 30 °C, DO < 4.0 mg/L,
탁도 > 100 NTU, TDS > 1000 ppm, 노드 전압 < 4.5 V). 같은 종류의 경보는 6시간에 한 번까지만 보내고,
**무효이거나 미교정인 센서 값으로는 경보를 보내지 않습니다**. 요금제별 이벤트 한도는 Blynk에서 확인하세요.

## 3. 인증정보 (`secrets.h`)

```bash
cp firmware/gateway_nodemcu/secrets.example.h firmware/gateway_nodemcu/secrets.h
# 또는 firmware/gateway_uno_esp01/ 에서 같은 방법
```

`secrets.h`에 템플릿 ID·이름, **새로 발급한** Auth Token, Wi-Fi 정보를 넣습니다.
이 파일은 `.gitignore`에 들어 있어 커밋되지 않습니다. 2024 토큰은 공개되었으므로 반드시 폐기합니다([SECURITY.md](../SECURITY.md)).

## 4. ESP-01 AT 펌웨어 설정 (Uno 게이트웨이만)

SoftwareSerial은 9600 baud에서만 안정적입니다. ESP-01 기본값은 펌웨어에 따라 115200인 경우가 많으므로,
USB-시리얼 어댑터로 ESP-01에 연결해 한 번 설정합니다.

```
AT+UART_DEF=9600,8,1,0,0
```

오래된 AT 펌웨어는 `AT+CIOBAUD=9600`을 씁니다. 응답 `OK` 후 9600 baud로 `AT`에 `OK`가 오는지 확인합니다.

## 5. 동작 방식

- 게이트웨이는 `loop()`마다 수신 버퍼(FIFO)를 **모두 비우고** 패킷마다 바로 Blynk로 보냅니다.
  2024 코드는 1초에 1개만 읽어 나머지 패킷을 잃었습니다.
- Wi-Fi가 끊겨도 `setup()`에서 멈추지 않습니다(2024 코드의 `Blynk.begin()`은 연결될 때까지 무한 대기).
  무선 수신과 시리얼 기록은 계속하고, 연결은 백그라운드에서 다시 시도합니다.
- 패킷마다 시리얼에 `CSV,...` 한 줄을 출력합니다. 이 기록으로 폭기 효과를 분석할 수 있습니다.

```bash
# Arduino IDE 시리얼 모니터에서 "타임스탬프 표시"를 켜고 복사해 저장한 뒤
python3 tools/aeration_effect.py gateway_log.txt                 # 용존산소
python3 tools/aeration_effect.py gateway_log.txt --column turbidity_ntu
```

DO로 폭기를 제어하면 "폭기 ON" 구간은 원래 DO가 낮은 구간이라 단순 평균 비교는 편향됩니다.
스크립트는 같은 시간대끼리의 차이와 변화율도 함께 보여 주며, 가장 확실한 방법은 일정 기간 폭기를 하루씩 번갈아 켜고 끄는 것입니다.
