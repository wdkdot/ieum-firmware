# Meshtastic 펌웨어 적용 계획

## 목적과 원칙

Ieum은 RAK4630 기반의 새 nRF52840 Meshtastic 보드로 지원한다. Meshtastic의 메시 라우팅, LoRa, Bluetooth LE, USB와 protobuf 계층은 그대로 사용하고, Ieum 고유 하드웨어는 보드 variant와 독립된 주변장치 드라이버로 추가한다.

- 보드 핀과 하드웨어 기능은 `variants/nrf52840/ieum/`에 모은다.
- 다른 보드에서도 쓸 수 있는 센서와 전원 드라이버는 `src/`의 공통 계층에 둔다.
- 공통 `main.cpp`, `main-nrf52.cpp`, Router와 NodeDB 수정은 최소화한다.
- 센서나 화면이 실패해도 LoRa, BLE와 USB 복구 기능은 계속 동작해야 한다.
- GPIO, 활성 레벨과 pull-up/down은 회로도와 최종 핀맵을 확인하기 전까지 추측하지 않는다.

## 전체 파일 구조

```text
Meshtastic 기존 기능
├─ nRF52840 / BLE / USB
├─ RAK4630 / SX1262 LoRa
├─ ATGM336H GNSS
├─ AHT20 환경 텔레메트리
└─ BMP388 환경 텔레메트리

Ieum 보드 정의
└─ variants/nrf52840/ieum/
   ├─ platformio.ini
   ├─ variant.h
   ├─ variant.cpp
   └─ nicheGraphics.h

추가할 공통 드라이버
├─ src/motion/MMA8652FCSensor.*
├─ src/power/BQ25628E.*
└─ src/graphics/niche/Drivers/EInk/GDEY0266T90H.*
```

## Ieum 보드 variant

### `variants/nrf52840/ieum/platformio.ini`

Ieum 전용 PlatformIO 환경인 `ieum`을 정의한다. `nrf52840_base`를 상속하고 `-D IEUM` 보드 매크로, variant include 경로, source filter, InkHUD 설정과 필요한 라이브러리를 추가한다.

RAK4631과 같은 부트로더와 플래시 구성을 사용한다면 `board = wiscore_rak4631` 재사용을 우선 검토한다. 부트로더 또는 메모리 배치가 다르면 `boards/ieum.json`을 추가한다. 최초 PCB bring-up과 복구를 위해 SWD/J-Link용 빌드 환경을 별도로 둘 수 있다.

SX1262만 사용하는 제품 빌드에서는 SX128x, SX127x, LR11x0과 LR2021 등 사용하지 않는 무선 드라이버를 제외해 빌드 크기를 줄인다.

### `variants/nrf52840/ieum/variant.h`

회로의 물리적 연결과 보드 기능을 정의한다.

- RAK4630 내부 SX1262의 CS, SCK, MOSI, MISO, DIO1, BUSY, RESET과 전원
- I²C SDA/SCL
- GNSS UART RX/TX, 주 전원 EN과 활성 레벨
- E-ink SPI, CS, DC, RESET, BUSY, TPS22919 EN과 활성 레벨
- 사용자 버튼 2개와 pull 설정
- Heartbeat/debug LED와 활성 레벨
- MMA8652FC 인터럽트 핀
- 배터리 ADC가 있다면 ADC 핀, 기준전압과 분압 보정값
- LFXO 또는 LFRC 선택

RAK4630 내부 LoRa 연결은 기존 `variants/nrf52840/rak4631/variant.h`를 기준으로 비교한다. 외부 주변장치 연결은 반드시 Ieum 최종 핀맵을 사용한다.

### `variants/nrf52840/ieum/variant.cpp`

보드 고유 전원 초기화와 종료 처리를 담당한다.

- `earlyInitVariant()`: E-ink와 GNSS 전원을 안전한 비활성 상태로 만들고 LED와 제어 GPIO 초기 상태를 설정한다.
- `variant_shutdown()`: E-ink deep sleep 완료, GNSS 주 전원 차단, LED 차단과 역급전 방지 상태를 설정한다.
- `variant_nrf52LoopHook()`: 필요한 경우에만 보드 고유 저전압 감시 등에 사용한다.

공통 `src/platform/nrf52/main-nrf52.cpp`가 이 hook을 제공하므로 Ieum 초기화 때문에 공통 nRF52 시작 코드를 직접 수정하지 않는 것을 우선한다.

### `variants/nrf52840/ieum/nicheGraphics.h`

InkHUD와 GDEY0266T90H 드라이버를 연결한다.

- E-ink 드라이버 객체 생성
- 사용할 SPI 인스턴스와 제어 핀 연결
- 184×360 패널 방향과 기본 회전 설정
- InkHUD applet 구성
- TPS22919 전원 제어와 패널 갱신 수명주기 연결

## 부품별 적용 방침

| 부품 | 현재 지원 | 적용 방침 |
|---|---|---|
| RAK4630 / nRF52840 | 지원됨 | 기존 nRF52 플랫폼과 RAK4631 variant를 기준으로 Ieum variant 작성 |
| SX1262 | 지원됨 | RAK4630 내부 연결과 RF switch/TCXO 설정 확인 |
| AHT20-F | 지원됨 | 기존 AHT10/AHT20 환경 센서 드라이버 재사용 |
| BMP388_TOKMAS | 지원됨 | 기존 BMP3XX 드라이버로 시작하고 forced mode 절전은 후속 검토 |
| ATGM336H-5NR-32 | 지원됨 | 기존 ATGM336H GNSS 지원과 Ieum UART/전원 핀 연결 |
| MMA8652FC | 미지원 | 공통 motion 드라이버, I²C 탐지와 생성 분기 추가 |
| BQ25628E | 미지원 | 별도 power 드라이버와 Ieum 전원 관리자 추가 |
| GDEY0266T90H / SSD1685 | 직접 지원 없음 | InkHUD용 패널 드라이버 추가 |
| TPS22919-Q1 | GPIO 제어 가능 | E-ink 갱신 수명주기에 맞춰 전원 ON/OFF |

## 기존 지원을 재사용하는 부품

### AHT20-F

Meshtastic의 `src/modules/Telemetry/Sensor/AHT10.*`는 AHT10과 AHT20을 함께 지원하며 `src/detect/ScanI2CTwoWire.cpp`도 일반 주소 `0x38`을 탐지한다. 새 드라이버를 만들지 않고 다음을 검증한다.

- 부팅 직후 초기화와 calibration 상태
- 필터가 적용된 AHT20-F의 응답 지연
- 온도·습도 telemetry 출력
- I²C 오류 발생 시 복구

### BMP388_TOKMAS

`src/modules/Telemetry/Sensor/BMP3XXSensor.*`와 Adafruit BMP3XX 라이브러리를 재사용한다. 초기 bring-up에서는 기존 설정으로 측정 안정성을 확인한다.

Ieum은 1분 간격 측정이므로 후속 단계에서 forced mode 단발 측정 후 sleep 전환을 검토한다. 가능하면 Ieum 전용 분기보다 BMP3XX 공통 저전력 기능으로 구현한다.

### ATGM336H GNSS

`src/gps/GPS.*`에 ATGM336H 탐지와 설정 지원이 이미 있다. Ieum variant에서 UART RX/TX, baud rate, 주 전원 EN과 활성 레벨을 정의하고 실제 모듈의 NMEA 출력과 baud rate를 bring-up에서 확인한다.

주 전원은 fix를 얻거나 timeout이 끝난 뒤 차단하고 VBAT 백업은 유지한다. 유효하지 않은 fix로 마지막 정상 위치를 덮어쓰지 않는다.

## 새로 추가할 드라이버

### MMA8652FC

예상 추가 파일:

```text
src/motion/MMA8652FCSensor.h
src/motion/MMA8652FCSensor.cpp
```

예상 수정 파일:

- `src/detect/ScanI2C.h`: 장치 타입 추가
- `src/detect/ScanI2CTwoWire.cpp`: 주소와 WHO_AM_I 판별
- `src/motion/AccelerometerThread.h`: 드라이버 생성 분기 추가
- Ieum `variant.h`: 인터럽트 핀 정의

초기 범위는 12비트 XYZ, ±2/4/8 g, 6.25 또는 12.5 Hz low-power ODR, motion/still 인터럽트, source 레지스터 판별과 sleep/wake 처리다. I²C timeout, 오류 backoff와 재초기화도 포함한다. MMA8653FC와의 물리적 호환만으로 레지스터 동작까지 같다고 가정하지 않는다.

### BQ25628E

예상 추가 파일:

```text
src/power/BQ25628E.h
src/power/BQ25628E.cpp
```

초기 드라이버 범위:

- I²C 주소와 part ID 확인
- USB 입력 전류 제한
- 충전 전류와 충전 전압 설정
- 충전, 전원과 오류 상태 읽기
- ADC 상태 읽기
- watchdog 처리
- ship/shutdown 기능
- 통신 실패 시 안전한 기본 동작 유지

제품 포크 초기에는 `IEUM` 빌드에서만 생성되는 Ieum 전원 관리 계층으로 연결한다. 안정화 후 Meshtastic 공통 PMIC 구조로의 통합을 검토한다. 충전 설정값은 BQ25628E 데이터시트, 회로와 배터리 사양을 대조한 뒤 확정한다.

### GDEY0266T90H / SSD1685

사용 패널은 Good Display GDEY0266T90H, 2.66인치, 184×360, 흑백, SSD1685로 확정한다. UI는 Meshtastic InkHUD를 사용한다.

예상 추가 파일:

```text
src/graphics/niche/Drivers/EInk/GDEY0266T90H.h
src/graphics/niche/Drivers/EInk/GDEY0266T90H.cpp
variants/nrf52840/ieum/nicheGraphics.h
```

`src/graphics/niche/Drivers/EInk/SSD16XX.*`와 `SSD1682.*` 구조를 우선 검토하되 SSD1685 초기화 명령, RAM 방향, LUT와 해상도는 제조사 자료로 확인한다.

화면 갱신 순서:

1. TPS22919 E-ink 전원을 켠다.
2. 전원 안정화 시간을 기다린다.
3. 패널 reset과 SSD1685 초기화를 수행한다.
4. InkHUD의 1비트 프레임을 전송한다.
5. 전체, 빠른 또는 부분 갱신을 시작한다.
6. timeout을 두고 BUSY 해제를 기다린다.
7. 패널을 deep sleep으로 전환한다.
8. SPI와 제어 핀을 역급전 방지 상태로 전환한다.
9. TPS22919를 끈다.

부분 갱신만 계속 반복하지 않고 실물의 잔상 특성에 따라 주기적으로 전체 갱신한다.

## 움직임 기반 GNSS 정책

기본 GPS와 위치 전송이 안정된 뒤 다음 정책을 별도 단계로 구현한다.

- `MOVING`: 정기 GNSS 측정
- `POSSIBLY_STILL`: debounce 대기
- `STILL`: 일부 정기 측정 생략
- `FORCE_UPDATE`: 최대 생략 횟수 또는 최대 시간 도달 시 측정

Ieum 전용 `#ifdef`를 `GPS.cpp` 여러 위치에 넣기보다 motion 상태를 이벤트로 제공하고 GPS 또는 Position 계층이 구독하도록 설계한다. 정지 상태에서도 위치가 영구히 갱신되지 않도록 강제 갱신 조건을 둔다.

## 가급적 수정하지 않을 영역

초기 Ieum 지원 단계에서는 다음 영역을 수정하지 않는다.

- `src/main.cpp`
- `src/platform/nrf52/main-nrf52.cpp`
- `src/mesh/Router.*`
- `src/mesh/NodeDB.*`
- `src/mesh/generated/**`

`src/mesh/generated/`는 생성 파일이므로 직접 수정하지 않는다. Meshtastic 앱과 protobuf에 정식 `IEUM` 하드웨어 모델을 표시하는 작업은 보드 bring-up과 분리하고 protobufs upstream 절차로 진행한다.

## 구현 전 확정할 자료

- RAK4630 GPIO와 회로 net 대응표
- I²C SDA/SCL 핀
- AHT20, BMP388, MMA8652FC와 BQ25628E의 주소 선택 상태
- MMA8652FC 인터럽트 핀
- GNSS RX/TX/EN과 EN 활성 레벨
- E-ink SPI/CS/DC/RESET/BUSY/EN과 EN 활성 레벨
- 버튼 2개의 핀과 활성 레벨
- LED 핀과 활성 레벨
- E-ink와 LoRa의 SPI bus 공유 여부
- RAK4630 부트로더, 플래시 방식과 SWD 복구 절차

## 단계별 개발 순서

1. Ieum variant와 SWD/USB 복구 경로
2. RAK4630 LoRa 송수신과 BLE 연결
3. I²C 스캔과 각 부품 식별
4. 기존 AHT20/BMP388 telemetry
5. 기존 ATGM336H GNSS와 전원 차단
6. MMA8652FC 기본 XYZ와 인터럽트 드라이버
7. BQ25628E 상태 읽기와 안전한 충전 설정
8. InkHUD와 GDEY0266T90H 전체 갱신
9. E-ink deep sleep, 전원 차단과 빠른/부분 갱신
10. 움직임 기반 GNSS 절전 정책
11. 24시간 이상 소비전력 측정과 장기 안정성 시험

각 단계에서 앞 단계의 LoRa, BLE와 USB 복구 기능이 계속 정상인지 함께 확인한다.
