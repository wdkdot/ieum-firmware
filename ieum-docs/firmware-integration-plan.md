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

### 현재 구현 상태

2026-07-30 기준으로 초기 board-support variant와 `ieum` PlatformIO 환경에 GNSS, InkHUD와 GDEY0266T90H 지원을 추가했으며 `pio run -e ieum` 빌드를 확인했다.

- RAK4630 내부 SX1262 연결, 센서 I²C, GNSS·보조 UART, E-ink SPI와 확인된 보드 GPIO 번호를 정의했다.
- P0.09와 P0.10을 일반 GPIO로 사용할 수 있도록 nRF52 NFC 핀 설정을 빌드에 반영했다.
- 개인 소장용 노드이므로 정식 HardwareModel을 요청하지 않고 `PRIVATE_HW`로 식별한다.
- GNSS EN은 active high로 정의하고 부팅 초기에 LOW로 비활성화한다. GNSS 기능을 활성화했으며, 측정할 때만 주 전원을 켜고 임시 1초 guard 뒤 UART를 연결한다.
- E-ink EN은 active high로 정의하고 부팅 초기에 LOW로 비활성화한다. 공식 패널 자료에 따라 BUSY active high, RESET과 CS active low인 SSD1685 드라이버를 연결하고 InkHUD 빌드를 활성화했다.
- 일반 화면은 검증 예제의 프로필 3인 전체 `0xF7`, 부분 `0xDC`, white border `0x01`을 사용한다. 메뉴 안에서는 `0xF4` 기준 프레임 뒤 reset과 전원 차단 없이 `0x1C` 부분 갱신을 이어서 사용한다. 빠른 갱신 fallback은 공식 1.5초 `0xC7` 시퀀스다.
- 일반 화면 갱신과 메뉴 종료 뒤 deep sleep, SPI 종료, 신호 핀 high-Z와 TPS22919 OFF를 수행한다. 메뉴가 열린 동안에는 다음 입력을 위해 패널 세션을 유지한다. Applet 전환도 마지막 입력부터 5초 동안 세션을 유지하고, 유휴 종료 시 추가 갱신 없이 전원을 정리한다. BUSY timeout에는 추가 명령 전송 없이 즉시 전원 차단 경로를 사용한다.
- 기본 UI는 184×360 세로 방향과 단일 InkHUD tile이다. 실기기에서 확인한 상하 반전은 GDEY0266T90H 드라이버의 행 역순 전송으로 보정한다.
- 두 사용자 버튼은 외부 pull-up active low로 정의했다. 두 버튼 모두 InkHUD의 short/long press handler에 연결해 초기 안내 화면과 기본 UI를 조작하며, 보조 버튼의 최종 역할은 추후 분리한다.
- 두 LED는 active high로 정의하고 부팅 초기에 LOW로 끈다. 각 LED의 최종 펌웨어 역할은 별도로 확정한다.
- MMA8652FC INT1은 P0.09에 직결하며 push-pull active high, latched interrupt로 설정했다. MCU 입력은 no-pull과 rising edge를 사용한다.
- BQ25628E `INT`는 외부 pull-up된 open-drain active-low 256 µs pulse 입력으로 정의했다. TI 권장 pull-up은 10 kΩ이며, ISR은 I²C를 사용하지 않고 전원 thread의 flag/status poll만 예약한다.
- 부팅·종료와 GNSS 측정 사이에는 GNSS UART 및 E-ink 신호 핀을 pull 없는 기본 입력 상태로 두고, 두 active-high EN을 LOW로 비활성화한다.

빌드 성공은 부트로더 호환성, USB 복구, LoRa RF 동작 또는 전원 안전성을 검증하지 않는다. `wiscore_rak4631` 보드 설정 재사용은 실물 SWD·USB bring-up에서 확인해야 한다.

## 부품별 적용 방침

| 부품                   | 현재 지원 | 적용 방침                                                                               |
| ---------------------- | --------- | --------------------------------------------------------------------------------------- |
| RAK4630 / nRF52840     | 지원됨    | 기존 nRF52 플랫폼과 RAK4631 variant를 기준으로 Ieum variant 작성                        |
| SX1262                 | 지원됨    | RAK4630 내부 연결과 RF switch/TCXO 설정 확인                                            |
| AHT20-F                | 지원됨    | 기존 AHT10/AHT20 환경 센서 드라이버 재사용                                              |
| BMP388_TOKMAS          | 초기 지원 | `0x0D == 0x11` 전용 탐지와 Tokmas/SPA06 호환 command-mode 드라이버 사용                 |
| ATGM336H-5NR-32        | 지원됨    | 기존 ATGM336H GNSS 지원과 Ieum UART/전원 핀 연결                                        |
| MMA8652FC              | 초기 지원 | 0x1D/WHO_AM_I 탐지, 12비트 XYZ, 6.25 Hz Low Power와 INT1 motion IRQ                     |
| BQ25628E               | 초기 지원 | 별도 power 드라이버와 Ieum 전원 관리자에서 식별, 보수적 설정, 상태·fault·ADC와 INT 처리 |
| GDEY0266T90H / SSD1685 | 초기 지원 | InkHUD 전체 갱신과 기본 부분 갱신, 공식 빠른 갱신 fallback; 장기 잔상 검증 필요         |
| TPS22919-Q1            | 초기 지원 | 갱신별 ON/OFF, deep sleep과 high-Z 수명주기; 역급전 전류 실측 필요                      |

## 기존 지원을 재사용하는 부품

### AHT20-F

Meshtastic의 `src/modules/Telemetry/Sensor/AHT10.*`는 AHT10과 AHT20을 함께 지원하며 `src/detect/ScanI2CTwoWire.cpp`도 일반 주소 `0x38`을 탐지한다. 새 드라이버를 만들지 않고 다음을 검증한다.

- 부팅 직후 초기화와 calibration 상태
- 필터가 적용된 AHT20-F의 응답 지연
- 온도·습도 telemetry 출력
- I²C 오류 발생 시 복구

### BMP388_TOKMAS

Tokmas 부품은 Bosch BMP388과 이름만 같고 레지스터 맵과 보정 계수 형식이 다르다. `0x0D == 0x11`을 `BMP388_TOKMAS`로 별도 탐지하고 `src/modules/Telemetry/Sensor/TokmasBMP388Sensor.*`를 사용한다.

드라이버는 온도와 압력을 8배 oversampling의 command mode로 각각 측정하고, `0x10`-`0x24`의 보정 계수와 Tokmas 보상식을 적용한다. 센서·계수 준비와 측정 완료에는 데이터시트 기준의 유한 timeout을 사용해 실패한 센서가 나머지 부팅을 막지 않게 한다. 실제 보드에서 AHT20과 함께 측정값 범위, 재부팅, 설정 저장과 장시간 안정성을 검증해야 한다.

### ATGM336H GNSS

`src/gps/GPS.*`의 ATGM336H 탐지와 설정 지원을 사용한다. Ieum variant는 UART RX/TX, 주 전원 EN과 활성 레벨을 정의하고 기본 30분 측정 간격과 최대 5분 fix 탐색 한계를 적용한다. 실제 모듈의 NMEA 출력과 baud rate는 bring-up에서 확인한다.

주 전원을 켠 뒤 임시 1초 안정화 guard를 두고 UART를 attach한다. fix를 얻거나 timeout이 끝나면 UART를 종료하고 RX/TX를 high-Z로 만든 뒤 주 전원을 차단하며 VBAT 백업은 유지한다. 유효하지 않은 fix로 마지막 정상 위치를 덮어쓰지 않는다. 1초 guard, 최대 탐색 시간과 off-state 역급전은 실기기 측정 후 확정한다.

## 새로 추가할 드라이버

### MMA8652FC

추가 파일:

```text
src/motion/MMA8652FCSensor.h
src/motion/MMA8652FCSensor.cpp
```

예상 수정 파일:

- `src/detect/ScanI2C.h`: 장치 타입 추가
- `src/detect/ScanI2CTwoWire.cpp`: 주소와 WHO_AM_I 판별
- `src/motion/AccelerometerThread.h`: 드라이버 생성 분기 추가
- Ieum `variant.h`: 인터럽트 핀 정의

초기 구현은 12비트 XYZ, ±2 g, 6.25 Hz Low Power ODR, INT1 latched motion IRQ와 I²C 오류 backoff·재초기화를 제공한다. 주소는 0x1D이며 WHO_AM_I 레지스터 0x0D에서 0x4A를 확인한다. INT1은 P0.09에 직결하고 push-pull active high로 구동하며, MCU는 no-pull rising-edge 입력을 사용한다. IRQ 처리 후 `INT_SOURCE`와 `FF_MT_SRC`를 읽어 원인을 확인하고 latch를 해제한다. ±4/8 g, Auto-WAKE/SLEEP과 FIFO는 실물 검증 후 확장한다. MMA8653FC와의 물리적 호환만으로 레지스터 동작까지 같다고 가정하지 않는다.

### BQ25628E

예상 추가 파일:

```text
src/power/BQ25628E.h
src/power/BQ25628E.cpp
src/power/BQ25628ESettings.h
src/power/BQ25628ESettings.cpp
```

구현한 초기 드라이버 범위:

- I²C 주소와 part ID 확인
- USB 입력 전류 제한
- 충전 전류와 충전 전압 설정
- 충전, 전원과 오류 상태 읽기
- ADC 상태 읽기
- watchdog 처리
- ship/shutdown 기능
- 통신 실패 시 안전한 기본 동작 유지

제품 포크 초기에는 `IEUM` 빌드에서만 생성되는 Ieum 전원 관리 계층으로 연결한다. 주소 `0x6A`와 part number `4`를 확인하고, 16-bit 레지스터는 little-endian으로 처리한다. 읽기는 register address 뒤 repeated START를 사용하는 단일 transaction이며 START 사이에 100 µs 간격을 둔다. read-to-clear interrupt flag는 startup과 주기적 poll에서 읽고, ISR은 thread 실행만 예약한다.

초기 설정은 입력 500 mA, 충전 320 mA/4.00 V, 6.3 V input OVP, 외부 ILIM 활성, watchdog 비활성이다. InkHUD Power 메뉴에서 4.00 V battery-care 모드와 4.20 V full-charge 모드를 전환하며, 성공한 선택은 전용 versioned 설정 파일에 저장한다. 저장값은 BQ25628E 초기화 전에 읽고, 파일이 없거나 손상되면 4.00 V를 사용한다. 첫 I²C 쓰기는 watchdog 비활성화이며 이후 충전 설정을 적용한다. ADC는 `REG0x27=0x00`으로 모든 채널을 활성화한 9-bit one-shot을 사용하며, 실기기에서 확인한 변환 시간을 수용하도록 실제 경과시간 기준 150 ms timeout을 둔다. 일시적인 ADC 실패에는 이전 정상값을 유지하고 연속 실패는 측정 불가로 처리하며, 초기 I²C 탐색 실패도 주기적으로 재시도한다. 설정은 주기적으로 read-back해 adapter 제거로 초기화되는 입력 제한이나 예상하지 못한 reset을 복원한다. Ship/Shutdown API는 제공하지만 일반 종료에 연결하지 않는다. 통신 실패 시 칩의 autonomous charger 동작을 유지하며 LoRa, BLE와 USB 부팅을 막지 않는다.

안정화 후 Meshtastic 공통 PMIC 구조로의 통합을 검토한다. 실기기 충전 전에는 배터리 최대 전압·전류와 TS 동작을 확인하고, ADC 정확도와 USB 입력 제한을 측정해 설정을 확정한다.

### GDEY0266T90H / SSD1685

사용 패널은 Good Display GDEY0266T90H, 2.66인치, 184×360, 흑백, SSD1685로 확정한다. UI는 Meshtastic InkHUD를 사용한다.

예상 추가 파일:

```text
src/graphics/niche/Drivers/EInk/GDEY0266T90H.h
src/graphics/niche/Drivers/EInk/GDEY0266T90H.cpp
variants/nrf52840/ieum/nicheGraphics.h
```

`src/graphics/niche/Drivers/EInk/SSD16XX.*` 구조를 재사용하고, SSD1685 초기화 명령, RAM 방향, 해상도와 update control 값은 Good Display 공식 패널 사양과 Arduino 예제로 확인했다.

일반 화면 갱신 순서:

1. TPS22919 E-ink 전원을 켠다.
2. 패널 reset과 SSD1685 초기화를 수행한다. 별도 전원 안정화 시간은 실측 후 추가한다.
3. InkHUD의 1비트 프레임을 전송한다.
4. 전체, 빠른 또는 부분 갱신을 시작한다.
5. timeout을 두고 BUSY 해제를 기다린다.
6. 패널을 deep sleep으로 전환한다.
7. SPI와 제어 핀을 역급전 방지 상태로 전환한다.
8. TPS22919를 끈다.

현재 일반 화면은 검증 예제의 프로필 3인 `0x22=0xF7` 전체 갱신과 `0x22=0xDC` 부분 갱신, `0x3C=0x01` white border를 사용한다. 프로필 4의 `0xC0` Hi-Z는 Ieum 패널에서 VBD 가장자리가 검게 남아 제외했다. 메뉴 진입 시 `0xF4` 전체 갱신으로 기준 프레임과 구동 회로를 준비하고, 메뉴 안의 후속 갱신은 rail과 SPI를 유지한 채 reset 없이 `0x1C`를 사용한다. 메뉴 종료 시 `0xF7` 전체 갱신 후 deep sleep과 TPS22919 OFF를 수행한다. Applet 전환은 첫 `0xDC` 뒤 5초 동안 같은 세션의 `0x1C`를 재사용하며, 유휴 종료는 추가 갱신 없이 deep sleep과 TPS22919 OFF만 수행한다. 부분 갱신은 이전 프레임을 MCU에 보존해 `0x26` base plane을 복원하고 `0x21=0x00,0x40`으로 RAM 극성과 184-source 모드를 명시한다. 초기 시험에서 5회마다 전체 갱신을 강제할 필요는 없었으므로 고정 횟수 정책은 두지 않고, InkHUD display-health maintenance와 장기 잔상 측정 결과에 따라 전체 갱신을 수행한다.

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

`src/mesh/generated/`는 생성 파일이므로 직접 수정하지 않는다. Ieum은 개인 소장용 노드로 유지하며 Meshtastic 앱이나 protobuf에 정식 `IEUM` 하드웨어 모델을 추가하지 않는다.

## 구현 전 확정할 자료

- RAK4630 GPIO와 회로 net 대응표
- I²C SDA/SCL 핀
- AHT20, BMP388와 BQ25628E의 주소 선택 상태
- GNSS 전원 안정화 시간과 UART off-state
- E-ink 전원 안정화 시간과 PCB에서의 off-state 역급전 전류
- E-ink 흑백 극성, 가장자리와 마지막 행/열, 빠른/부분 갱신의 잔상과 온도별 BUSY 시간
- 버튼 2개의 최종 펌웨어 역할
- LED 2개의 최종 펌웨어 역할
- E-ink와 LoRa의 SPI bus 공유 여부
- RAK4630 부트로더, 플래시 방식과 SWD 복구 절차

## 단계별 개발 순서

1. Ieum variant와 SWD/USB 복구 경로
2. RAK4630 LoRa 송수신과 BLE 연결
3. I²C 스캔과 각 부품 식별
4. 기존 AHT20과 Tokmas BMP388 telemetry
5. 기존 ATGM336H GNSS와 전원 차단
6. MMA8652FC 기본 XYZ와 인터럽트 드라이버
7. BQ25628E 상태 읽기와 안전한 충전 설정
8. InkHUD와 GDEY0266T90H 전체 갱신
9. E-ink deep sleep, 전원 차단과 빠른/부분 갱신
10. 움직임 기반 GNSS 절전 정책
11. 24시간 이상 소비전력 측정과 장기 안정성 시험

각 단계에서 앞 단계의 LoRa, BLE와 USB 복구 기능이 계속 정상인지 함께 확인한다.
