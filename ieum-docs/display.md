# E-ink 디스플레이

## 패널

- 제조사: Good Display
- 부품: GDEY0266T90H
- 크기: 2.66인치
- 해상도: 184×360
- 해상도 밀도: 약 151.99 PPI
- 색상: 흰색, 검정색
- 컨트롤러: SSD1685
- 인터페이스: SPI
- 동작 전압: 3.3 V
- 제조사 표시 갱신 시간: 전체 약 2초, 빠른 갱신 약 1.5초, 부분 갱신 약 0.3초
- 갱신 시간 시험 조건: 주변 온도 25 °C
- 동작 온도: 0-50 °C
- 펌웨어 UI: Meshtastic InkHUD 사용 확정

이 패널은 bistable 특성 때문에 전원이 제거되어도 마지막 화면을 유지한다. 따라서 화면을 갱신할 때만 전원과 SPI를 사용하고, 정상 종료 후 로드 스위치로 전원을 차단할 수 있다.

Ieum 펌웨어는 Meshtastic의 InkHUD를 사용한다. `GDEY0266T90H` 전용 드라이버는 기존 `SSD16XX` 계층 위에서 제조사 초기화와 갱신 시퀀스를 구현하고, Ieum `nicheGraphics.h`가 전용 `SPI1`과 TPS22919 전원 수명주기를 연결한다.

## 구현 상태

2026-07-16 기준 구현 범위는 다음과 같다.

- 184×360, 행당 23 byte, 총 8,280 byte인 1-bit InkHUD 프레임버퍼
- InkHUD 회전 0/1/2/3 지원과 Ieum 기본 세로 방향 `IEUM_INKHUD_ROTATION=0`
- Ieum 패널 장착 방향에 맞춘 프레임버퍼 행 역순 전송
- 제조사 예제와 일치하는 SSD1685 초기화 및 전체 갱신 명령
- 설정 단계 5초, 실제 화면 갱신 단계 9초의 bounded BUSY timeout
- 전체 갱신 완료 후 `0x10, 0x01` deep sleep과 제조사 예제의 100 ms 대기
- TPS22919 활성화부터 SPI 종료, 제어 핀 high-Z, TPS22919 비활성화까지의 갱신별 수명주기
- InkHUD `FAST`를 제조사 1.5초 빠른 갱신에 연결
- 부분 갱신을 선택할 때만 이전 프레임을 MCU RAM에 보존하는 전체 화면 부분 갱신 구현
- 두 버튼의 배선과 debounce/long-press 설정 위치를 한 함수에 모으되, 역할 확정 전에는 handler와 IRQ를 시작하지 않음

`pio run -e ieum` 빌드는 확인했다. 실기기에서 행 역순 전송 후 UI 상하 방향이 정상임을 확인했으며, 영상 품질, BUSY 시간, 잔상, off-state 역급전 전류는 아직 검증하지 않았다.

## 하드웨어 구성

RAK4630의 SPI와 제어 신호가 패널에 연결된다. E-ink 전원은 TPS22919QDCKRQ1을 통과하며, 패널 구동에 필요한 다이오드, 인덕터 및 커패시터가 외부 보조 전원 회로를 구성한다.

패널은 write-only SPI를 사용하지만 nRF52 Arduino `SPIClass`는 MISO에도 유효한 variant 핀 인덱스를 요구한다. Ieum은 PCB에 연결되지 않은 P0.31을 `SPI1_MISO`의 dummy 핀으로 사용한다.

정확한 SPI 핀, CS, DC, RESET, BUSY 및 로드 스위치 EN은 핀맵 문서에서 다룬다. 과거 ESP32 테스트 핀 번호를 최종 PCB 핀으로 사용하면 안 된다.

## 구현된 갱신 순서

1. 유휴 상태에는 SPI와 제어 GPIO를 pull 없는 입력으로 두고 TPS22919를 끈다.
2. TPS22919를 활성화한다.
3. CS, DC, RESET, BUSY와 전용 `SPI1`을 구성한다.
4. RESET을 10 ms LOW, 10 ms HIGH로 구동하고 software reset을 수행한다.
5. SSD1685의 gate 수, RAM 방향과 전체 184×360 window를 설정한다.
6. 흑백 1비트 화면 데이터와 갱신 방식에 필요한 두 번째 RAM plane을 전송한다.
7. 화면 갱신을 시작하고 BUSY가 LOW가 될 때까지 비동기로 확인한다.
8. 완료되면 컨트롤러에 deep sleep 명령을 보내고 100 ms 기다린다.
9. `SPI1`을 종료하고 SCLK, MOSI, CS, DC, RESET, BUSY를 high-Z로 바꾼다.
10. TPS22919를 비활성화한다.

초기화 또는 갱신 중 timeout이 발생하면 BUSY 상태에서 추가 명령을 보내지 않고 즉시 SPI를 종료한 뒤 핀을 high-Z로 만들고 TPS22919를 끈다. 별도의 전원 안정화 지연값은 실측 근거가 없으므로 추가하지 않았으며, 공식 RESET pulse가 rail 활성화 직후 수행된다. 최종 PCB에서 필요한 안정화 시간이 확인되면 그 측정값을 수명주기 시작부에 추가한다.

## SSD1685 명령 시퀀스

전체 갱신은 Good Display 공식 Arduino 예제의 normal initialization을 그대로 따른다.

| 단계        | 명령과 데이터                                      |
| ----------- | -------------------------------------------------- |
| Reset       | RESET LOW 10 ms, HIGH 10 ms, `0x12` software reset |
| Gate        | `0x01`: `0x67 0x01 0x00` — 360 gate                |
| RAM 방향    | `0x11`: `0x01` — X 증가, Y 감소                    |
| X window    | `0x44`: `0x00 0x16` — 23 byte                      |
| Y window    | `0x45`: `0x67 0x01 0x00 0x00` — 359에서 0          |
| Border/온도 | `0x3C`: `0x05`, `0x18`: `0x80`                     |
| Cursor      | `0x4E`: `0x00`, `0x4F`: `0x67 0x01`                |
| 화면 RAM    | `0x24`: 8,280 byte, `0x26`: 8,280 byte의 `0x00`    |
| 전체 갱신   | `0x22`: `0xF4`, 이어서 `0x20`                      |

패널 BUSY는 HIGH일 때 busy이며, LOW가 된 뒤에만 후속 명령을 보낸다. deep sleep에서는 BUSY가 HIGH로 유지되므로 deep sleep 명령 뒤에는 BUSY 해제를 기다리지 않는다. deep sleep에서 다시 동작하려면 hardware reset이 필요하다.

## 프레임버퍼

184×360 단색 프레임버퍼 하나는 8,280 byte다.

`184 × 360 / 8 = 8,280 byte`

nRF52840의 256 KB RAM에서는 전체 1비트 프레임버퍼를 유지할 수 있지만, Meshtastic의 기존 디스플레이 추상화와 버퍼 형식을 우선 재사용한다.

InkHUD Renderer는 행 우선, 왼쪽 픽셀이 각 byte의 MSB인 형식으로 이 버퍼를 만든다. SSD1685는 첫 프레임버퍼 행을 Y=359에 쓰므로 버퍼를 그대로 전송하면 Ieum 장착 방향에서 화면이 상하 반전된다. 드라이버는 행 359부터 행 0까지 역순으로 전송해 이를 보정하며 좌우 방향은 바꾸지 않는다. InkHUD 기본 회전값은 184×360 세로 UI의 `0`을 유지한다.

`IEUM_EINK_USE_PARTIAL_REFRESH=1`로 부분 갱신을 선택할 때만 드라이버가 이전 프레임용 8,280 byte를 추가로 할당한다. 기본 FAST 모드의 화면 관련 RAM은 InkHUD 현재 프레임 8,280 byte이며, 부분 갱신 모드에서는 이전 프레임을 포함해 합계 16,560 byte다. 추가 버퍼 할당에 실패하면 드라이버는 제조사 FAST 모드로 되돌아간다.

## 빠른 갱신과 부분 갱신

기본 `FAST` 동작은 공식 예제의 1.5초 빠른 갱신이다. 내부 온도 센서를 선택하고 `0x22/0x20`의 `0xB1`, 온도 레지스터 `0x1A`의 `0x6E 0x00`, 이어서 `0x91`을 적용한 뒤 `0xC7` 갱신을 사용한다. 구형 패널 지원 여부가 명시된 1초 Fast2의 `0x5A` 설정은 사용하지 않는다.

전체 화면 부분 갱신 코드는 `0x1C` 갱신을 사용한다. 패널 전원을 매번 끄기 때문에 SSD1685 RAM에 남아 있던 base map에 의존하지 않고, 마지막으로 성공한 프레임을 nRF52840 RAM에 보존했다가 `0x26` plane에 다시 쓴다. 첫 갱신이나 timeout 다음 갱신은 기준 프레임이 없으므로 자동으로 전체 갱신한다.

부분 갱신은 실기기에서 잔상과 전원 재인가 후의 base-map 재구성을 확인하기 전까지 `IEUM_EINK_USE_PARTIAL_REFRESH=0`으로 비활성화한다. 1로 바꾸면 InkHUD의 `FAST` 요청이 전체 화면 부분 갱신으로 바뀐다. InkHUD는 기본적으로 FAST 5회마다 FULL 갱신을 목표로 하며, 이는 제조사 예제의 “부분 갱신 5회 후 전체 갱신” 지침에 맞춘 초기값이다.

## 버튼 준비

`prepareIeumButtons()`가 두 active-low 외부 pull-up 버튼의 핀과 50 ms debounce, 500 ms long-press 기준을 한곳에서 설정한다. 역할을 추측하지 않기 위해 handler 등록과 `buttons->start()`는 아직 호출하지 않는다. 역할이 확정되면 이 함수에 InkHUD handler를 연결하고 `start()` 한 줄을 추가하면 된다.

## 펌웨어 주의점

- 일반 전체 갱신을 품질 기준으로 삼고, 빠른 갱신과 부분 갱신은 실물에서 별도로 검증한다.
- 잔상 누적을 막기 위해 부분 갱신만 계속 반복하지 않고 주기적으로 전체 갱신한다.
- BUSY 대기에 반드시 timeout을 둔다.
- 전원 차단 전에 deep sleep 명령과 BUSY 완료를 보장한다.
- 화면 유지에는 전력이 필요하지 않으므로 단순 상태 변화마다 갱신하지 않는다.
- 0.3초 부분 갱신과 1.5초 빠른 갱신 수치는 25 °C 제조사 시험값이므로 실제 케이스와 온도에서 다시 측정한다.
- 향후 패널 변경 시 컨트롤러와 LUT, 해상도, 픽셀 형식을 다시 확인한다.
- 실기기에서 행 역순 전송 후 UI 상하 방향이 정상임을 확인했다. 흑백 극성, 가장자리와 마지막 행/열은 시험 패턴으로 추가 확인한다.
- timeout 경로에서 LoRa, BLE와 USB가 계속 동작하고 EINK_EN이 LOW로 복귀하는지 확인한다.
- deep sleep 뒤 SCLK, MOSI, CS, DC, RESET, BUSY의 off-state 전압과 패널 rail 전류를 측정한다.
