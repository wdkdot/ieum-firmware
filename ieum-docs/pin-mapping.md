# Ieum RAK4630 핀 매핑

이 문서는 Ieum 회로의 RAK4630 모듈 핀과 nRF52840 GPIO를 펌웨어 구현 관점에서 대응시킨다. 기준은 2026-07-13에 확인한 U12 회로도와 사용자 재확인 내용이다.

일반 펌웨어 핀 정의에 불필요한 3V3와 GND 핀은 표에서 생략한다.

## 번호 표기 규칙

다음 세 번호는 서로 다른 의미이므로 혼용하지 않는다.

- **모듈 핀**: RAK4630 패키지의 물리 핀 번호다. 예: 모듈 핀 `25`.
- **nRF GPIO**: nRF52840의 포트와 비트 번호다. 예: `P1.01`.
- **펌웨어 핀 값**: Ieum nRF52 variant의 선형 Arduino 핀 번호로 사용할 값이다. `P0.nn`은 `nn`, `P1.nn`은 `32 + nn`이다. 따라서 `P1.01`의 펌웨어 핀 값은 `33`이다.

Ieum의 `variant.cpp`는 RAK4631 variant와 동일하게 `g_ADigitalPinMap`을 `P0.00`부터 `P1.15`까지 선형으로 구성해야 아래 펌웨어 핀 값이 성립한다.

## 펌웨어 GPIO 매핑

| 회로 net | 용도 | 모듈 핀 | nRF GPIO | 펌웨어 핀 값 | 펌웨어 관점 |
| --- | --- | ---: | --- | ---: | --- |
| `I2C1_SDA` | 센서 I²C 데이터 | 4 | `P0.13` | 13 | `PIN_WIRE_SDA` 후보 |
| `I2C1_SCL` | 센서 I²C 클록 | 5 | `P0.14` | 14 | `PIN_WIRE_SCL` 후보 |
| `GNSS_TX` | GNSS UART 송신 | 6 | `P0.15` | 15 | MCU가 수신하므로 `GPS_RX_PIN` |
| `GNSS_RX` | GNSS UART 수신 | 7 | `P0.16` | 16 | MCU가 송신하므로 `GPS_TX_PIN` |
| `GNSS_EN` | GNSS 주 전원 제어 | 8 | `P0.17` | 17 | 활성 레벨은 아직 미확정 |
| `UART1_RX` | 보조 UART 수신 | 9 | `P0.19` | 19 | MCU RX |
| `UART1_TX` | 보조 UART 송신 | 10 | `P0.20` | 20 | MCU TX |
| `NRF_LED1` | LED 1 | 11 | `P0.21` | 21 | 활성 레벨은 아직 미확정 |
| `EINK_EN` | E-ink 전원 제어 | 12 | `P0.10` | 10 | TPS22919 제어, 활성 레벨은 아직 미확정 |
| `MMA_INT` | MMA8652FC 인터럽트 | 13 | `P0.09` | 9 | GPIO 인터럽트 입력 |
| `SW_1` | 사용자 버튼 1 | 25 | `P1.01` | 33 | pull 및 활성 레벨은 아직 미확정 |
| `SW_2` | 사용자 버튼 2 | 26 | `P1.02` | 34 | pull 및 활성 레벨은 아직 미확정 |
| `NRF_LED2` | LED 2 | 27 | `P1.03` | 35 | 활성 레벨은 아직 미확정 |
| `SPI_SCK` | E-ink SPI 클록 | 29 | `P0.03` | 3 | `PIN_EINK_SCLK` 후보 |
| `EINK_BUSY` | E-ink BUSY | 30 | `P0.02` | 2 | 입력, busy 레벨은 아직 미확정 |
| `EINK_RST` | E-ink reset | 31 | `P0.28` | 28 | 활성 레벨은 아직 미확정 |
| `EINK_DC` | E-ink data/command | 32 | `P0.29` | 29 | 출력 |
| `SPI_MOSI` | E-ink SPI MOSI | 33 | `P0.30` | 30 | `PIN_EINK_MOSI` 후보 |
| `EINK_CS` | E-ink SPI chip select | 34 | `P0.26` | 26 | 활성 레벨은 아직 미확정 |
| `BQ_INT` | BQ25628E 인터럽트 | 40 | `P0.05` | 5 | 인터럽트 입력, 활성 레벨은 아직 미확정 |

`GNSS_TX`와 `GNSS_RX`는 GNSS 모듈 관점에서 이름이 붙었다. 따라서 `GNSS_TX`는 MCU의 RX 핀이고 `GNSS_RX`는 MCU의 TX 핀이다.

## 사용하지 않는 GPIO

| 모듈 핀 | nRF GPIO | 펌웨어 핀 값 | 회로도 상태 |
| ---: | --- | ---: | --- |
| 23 | `P0.24` | 24 | NC |
| 24 | `P0.25` | 25 | NC |
| 28 | `P1.04` | 36 | NC |
| 39 | `P0.31` / AIN7 | 31 | NC |
| 41 | `P0.04` / AIN2 | 4 | NC |

NC 핀은 variant에서 주변장치 기능에 배정하지 않는다.

## USB와 디버그 신호

이 신호들은 일반 GPIO 번호로 취급하지 않는다.

| 회로 net | 모듈 핀 | 모듈 신호 | 비고 |
| --- | ---: | --- | --- |
| `USB_VBUS` / `VBUS_NRF` | 1 | VBUS | USB 전원 감지 |
| `USB_D_N` | 2 | USB− | USB D− |
| `USB_D_P` | 3 | USB+ | USB D+ |
| `NRF_RST` | 17 | NRF_RESET | MCU reset |
| `SWDCLK` | 18 | SWDCLK | SWD 디버그 클록 |
| `SWDIO` | 19 | SWDIO | SWD 디버그 데이터 |

## 구현 시 주의사항

- `variant.h`의 핀 매크로에는 **모듈 핀 번호가 아니라 펌웨어 핀 값**을 사용한다.
- 예를 들어 버튼 1은 모듈 핀 `25`지만 `P1.01`이므로 코드 값은 `33`이다.
- `P0.09`와 `P0.10`은 NFC 겸용 핀이므로 Ieum 빌드에서 GPIO로 사용할 수 있도록 nRF52 설정을 확인한다.
- E-ink와 GNSS 전원 제어, LED, 버튼, BUSY 및 인터럽트의 활성 레벨과 pull 설정은 이 표만으로 추측하지 않는다.
- RAK4630 내부 SX1262 연결은 이 외부 핀 표가 아니라 기존 RAK4631 variant 및 RAK4630 자료를 별도로 대조한다.
