# 펌웨어 구조와 동작 정책

## 목표

Meshtastic firmware를 포크하여 Ieum 전용 보드 정의와 하드웨어 지원을 추가한다. 가능한 한 upstream 구조를 유지하고, 범용으로 기여할 수 있는 센서 드라이버는 보드 전용 코드와 분리한다.

구체적인 파일 구성, 기존 지원 재사용 범위와 신규 드라이버 목록은 [Meshtastic 펌웨어 적용 계획](firmware-integration-plan.md)을 따른다.

## 버전 관리 방침

- upstream: Meshtastic firmware
- 제품 포크 이름 후보: `ieum-firmware`
- 재현 가능한 빌드를 위해 개발 기준 Meshtastic tag 또는 commit을 기록한다.
- upstream 변경은 주기적으로 병합하되 하드웨어 초기화 코드는 한정된 파일에 모은다.
- 릴리스에는 기반 upstream tag, Ieum commit, 빌드 환경을 함께 기록한다.

## 권장 계층

~~~mermaid
flowchart TD
    App["Meshtastic 기능"] --> Board["Ieum 보드 구성"]
    App --> Telemetry["Telemetry 모듈"]
    Board --> Drivers["센서·전원 드라이버"]
    Telemetry --> Drivers
    Drivers --> HAL["I²C / SPI / UART / GPIO"]
~~~

### 보드 구성

- 보드 식별자와 빌드 target
- 핀 정의
- 디스플레이 종류와 해상도
- 센서 존재 여부
- GNSS UART와 전원 제어
- 버튼 및 LED

### 범용 드라이버

- BMP388은 기존 Bosch BMP3 드라이버 재사용 가능성을 우선 확인한다.
- MMA8652FC는 MMA8653FC와의 공통 계열 드라이버를 검토한다.
- AHT20은 기존 Meshtastic 지원 여부와 사용 라이브러리를 확인한다.
- BQ25628E 지원은 센서 telemetry가 아닌 전원 관리 기능으로 분리한다.

## 부팅 순서

1. 전원 제어 GPIO를 안전한 비활성 상태로 설정한다.
2. 로그 및 기본 MCU 서비스를 시작한다.
3. I²C를 초기화한다.
4. BQ25628E와 전원 상태를 확인한다.
5. 필수 주변장치와 선택 주변장치를 구분해 probe한다.
6. 버튼, LED와 가속도 인터럽트를 구성한다.
7. 필요할 때만 E-ink와 GNSS를 켠다.
8. Meshtastic 무선 및 서비스 동작을 시작한다.

센서가 응답하지 않아도 LoRa 통신과 USB 복구 기능은 계속 동작해야 한다.

## 주기 작업

| 작업 | 초기 계획 | 비고 |
|---|---:|---|
| 온습도 측정 | 1분 | AHT20-F |
| 기압 측정 | 1분 | BMP388 forced mode 검토 |
| 가속도 ODR | 6.25 또는 12.5 Hz | Low Power 우선 |
| GNSS 위치 | 30분 | 정지 상태에서 생략 가능 |
| Heartbeat LED | 5-10초마다 짧게 | PWM 밝기 제한 |
| E-ink 갱신 | 이벤트 기반 | 불필요한 전체 갱신 억제 |

## 상태 관리

가속도계의 움직임 상태와 GNSS 정책을 분리된 상태 머신으로 구현한다.

- MOVING: 정기 GNSS 측정
- POSSIBLY_STILL: debounce 시간 대기
- STILL: 일부 GNSS 측정 생략
- FORCE_UPDATE: 최대 간격 도달 또는 사용자 요청

## 오류 및 로그

- 모든 I²C와 BUSY 대기에 timeout을 둔다.
- 오류가 반복되면 일정 시간 backoff한다.
- 로그에는 부품명, 주소, 동작 모드와 오류 원인을 남긴다.
- 전원 전환, GNSS TTFF, E-ink 갱신 시간, 센서 재초기화를 측정 가능한 이벤트로 만든다.
- 알 수 없는 부품 ID를 정상으로 간주하지 않는다.

## 미확정 항목

- 최종 핀맵
- 정확한 Meshtastic 기반 tag
- Ieum board target 이름
- BMP388 오버샘플링과 IIR 설정
- MMA8652FC ODR 6.25 Hz와 12.5 Hz 중 기본값
- GNSS fix timeout과 최대 생략 횟수
- E-ink 실제 갱신 및 안정화 시간
- BQ25628E의 최종 레지스터 설정
