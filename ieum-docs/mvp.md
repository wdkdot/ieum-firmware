# Ieum 펌웨어 MVP

- 기준일: 2026-07-16
- 기준 통합 브랜치: `origin/ieum/main` (`d1e031c19`)
- 기준 upstream: `v2.7.26.54e0d8d` (`54e0d8d0a`)

## 문서 브랜치

이 문서는 `origin/ieum/main`에서 분기한 `codex/ieum-mvp-plan`에 둔다. MVP 계획은 특정 기능 구현의 일부가 아니라 여러 기능 브랜치의 상태와 통합 순서를 관리하는 문서이므로, `codex/mma8652fc-support`나 `codex/ieum-actions-cleanup`에 섞지 않는다.

검토가 끝나면 이 문서를 `ieum/main`으로 병합한다. 이후 각 기능은 최신 `ieum/main`에서 분기한 전용 브랜치에서 구현하고, 코드 검토와 빌드 확인을 마치면 순서대로 병합한다.

이 저장소는 공식 배포 저장소가 아니라 개인 Ieum 펌웨어를 체계적으로 관리하기 위한 저장소다. 아직 실기기가 없으므로 하드웨어 검증은 병합 조건으로 삼지 않는다. 데이터시트와 회로도를 기준으로 구현을 먼저 완료하고, 보드 제작 후 발견되는 핀·타이밍·전원·동작 차이는 후속 commit으로 수정한다.

## MVP 정의

Ieum MVP는 PCB 완성 전에 주요 하드웨어 지원 경로를 구현하고 하나의 `ieum` firmware로 빌드할 수 있는 첫 소프트웨어 통합본이다.

1. SWD와 USB로 항상 복구할 수 있다.
2. BLE 또는 USB로 설정하고 LoRa 메시지를 송수신할 수 있다.
3. AHT20-F와 BMP388의 환경 telemetry를 읽을 수 있다.
4. ATGM336H를 켜서 유효한 위치를 얻고 UART를 격리한 뒤 주 전원을 끌 수 있다.
5. MMA8652FC의 XYZ 값과 움직임 인터럽트를 읽을 수 있다.
6. BQ25628E의 식별·상태를 읽고, 검증된 보수적 충전 설정만 적용한다.
7. GDEY0266T90H에 InkHUD 전체 화면을 갱신하고 deep sleep과 전원 차단을 완료한다.
8. 위 기능을 하나의 빌드에 통합하고 주변장치 실패가 핵심 Meshtastic 기능을 막지 않게 한다.

MVP 병합 기준은 코드 검토, `pio run -e ieum`, 관련 정적 검사와 가능한 비하드웨어 테스트다. 실기기 시험은 보드 제작 후 별도 bring-up 단계에서 수행하며, MVP 코드의 병합을 막지 않는다.

## 브랜치별 구현 현황

상태 표기의 의미는 다음과 같다.

- **통합됨**: `origin/ieum/main`에 포함된 코드다.
- **구현됨**: 코드가 기능 브랜치에 있으나 아직 통합되지 않았다.
- **빌드 확인**: `pio run -e ieum`이 성공했다.

| 브랜치 | 끝 commit | 현재 내용 | 판정 |
| --- | --- | --- | --- |
| `origin/ieum/main` | `d1e031c19` | 프로젝트 문서, `ieum` PlatformIO 환경, RAK4630/SX1262 및 외부 GPIO 정의, 부팅·종료 시 GNSS/E-ink rail 비활성화 | 통합됨 |
| `origin/codex/ieum-project-docs` | `e69be6561` | `ieum-docs/` 하드웨어·통합 문서 | `ieum/main`에 통합됨 |
| `origin/codex/ieum-board-support-base` | `6608e2b62` | 초기 Ieum variant와 GPIO active level 정리 | `ieum/main`에 통합됨 |
| `origin/codex/ieum-board-support` | `57cb77701` | 초기 MMA8652FC 탐지, 12비트 XYZ, motion IRQ와 오류 backoff | 후속 브랜치가 대체함 |
| `origin/codex/mma8652fc-support` | `3925fbeaa` | 위 motion 지원과 latched interrupt source 해제 수정 | 구현됨, 빌드 확인 |
| `origin/codex/ieum-actions-cleanup` | `1b1027125` | upstream workflow 다수를 제거하고 `ieum_ci.yml`로 `pio run -e ieum` 실행 | 미통합, 범위 검토 필요 |

원래 작업 브랜치였던 `codex/mma8652fc-support` 밖의 독립된 구현은 `codex/ieum-actions-cleanup`의 CI 정리다. 이 브랜치는 보드 variant가 들어오기 전 commit에서 갈라졌으므로 브랜치 전체를 병합하기보다, 삭제 범위를 검토한 뒤 필요한 CI commit만 최신 `ieum/main` 기반 브랜치로 옮기는 편이 안전하다.

2026-07-16에 `codex/mma8652fc-support`의 `3925fbeaa`에서 `pio run -e ieum`을 실행해 성공했다. 사용량은 RAM 92,948/248,832 byte(37.4%), Flash 551,780/815,104 byte(67.7%)였다. 플래시, 부팅, 무선 또는 센서 시험은 보드 제작 후 수행한다.

## 남은 MVP 작업

| 순서 | 작업 | 현재 상태 | 완료 조건 | 권장 브랜치 |
| ---: | --- | --- | --- | --- |
| 1 | SWD·USB·기본 부팅 | variant 구현됨 | RAK4631 기반 부트로더·USB·복구 설정을 문서화하고 `ieum` build에 포함 | `codex/ieum-bringup` |
| 2 | RAK4630 LoRa·BLE | 기존 Meshtastic 지원 재사용 | Ieum variant에서 기존 SX1262와 nRF52 BLE 경로가 함께 빌드됨 | `codex/ieum-bringup` |
| 3 | I²C inventory와 환경 telemetry | AHT20/BMP3XX 공통 드라이버 존재 | 주소 후보와 ID probe, AHT20/BMP388 telemetry 생성, timeout·오류 복구 경로 구현 | `codex/ieum-telemetry` |
| 4 | ATGM336H 수명주기 | 핀과 rail-off 초기화만 구현, `HAS_GPS=0` | 전원 ON/OFF, 안정화 timeout, UART attach/detach·high-Z와 fix 종료 경로 구현 | `codex/ieum-gnss-power` |
| 5 | MMA8652FC | 기능 브랜치 구현 및 빌드 성공 | WHO_AM_I 0x4A, 12비트 XYZ, INT1 source/latch 처리와 I²C 오류 복구 코드 통합 | 기존 `codex/mma8652fc-support` |
| 6 | BQ25628E | INT 핀 정의만 존재, 드라이버 없음 | 주소·part ID, status/fault/ADC, interrupt flag, watchdog와 데이터시트 기반 보수적 설정 구현 | `codex/ieum-bq25628e` |
| 7 | GDEY0266T90H InkHUD | 핀과 rail-off 초기화만 구현, 화면 비활성 | SSD1685 전체 갱신, BUSY timeout, deep sleep, GPIO high-Z와 TPS22919 수명주기 구현 | `codex/ieum-inkhud` |
| 8 | 소프트웨어 통합 | 미수행 | 모든 기능을 함께 빌드하고 주변장치 오류가 LoRa/BLE/USB 흐름을 막지 않도록 구성 | `codex/ieum-mvp-integration` |

CI 정리는 런타임 기능은 아니지만 병합 품질을 위한 선행 작업이다. `codex/ieum-actions-cleanup`이 삭제하는 upstream workflow가 너무 넓으므로, 최소한 Ieum 빌드와 필요한 정적 검사만 남기는 정책을 별도 검토한 뒤 적용한다.

## 구현 중 격리할 미확정 자료

다음 값은 MVP 개발과 병합을 막지는 않지만, 비슷한 보드나 샘플 코드에서 확정값처럼 추측하지 않는다. 코드에서는 명시적인 상수·기본값·timeout 또는 비활성 설정으로 격리하고 보드 제작 후 수정하기 쉽게 둔다.

- RAK4631 부트로더·메모리 배치 재사용의 실물 SWD/USB 호환성
- 실제 장착될 I²C 부품의 주소와 ID
- ATGM336H-5NR-32의 baud rate, NMEA 설정, 전원 안정화 시간과 UART off-state
- BQ25628E 주소·part ID, 배터리 최대 충전 전압·전류, 입력 전류 제한과 watchdog 정책
- SSD1685 초기화·LUT·RAM 방향, BUSY/RESET/CS active level과 전원 안정화 시간
- GNSS와 E-ink 전원 차단 상태의 신호 pin 전압 및 역급전 전류

위 항목이 불명확하면 상태를 `unknown`으로 기록하고, 회로도·데이터시트 또는 향후 실기기 측정 중 무엇이 필요한지 남긴다. 불명확한 주변장치가 부팅, LoRa, BLE 또는 USB 복구를 무한 대기하게 만들지 않는다.

## MVP 완료 체크리스트

- [ ] `pio run -e ieum` 성공
- [ ] `trunk fmt` 통과
- [ ] SWD와 USB 복구 구성을 코드와 문서에 반영
- [ ] 기존 LoRa와 BLE 지원을 Ieum variant build에 포함
- [ ] AHT20-F와 BMP388 probe·telemetry·오류 복구 구현
- [ ] ATGM336H fix timeout·UART 격리·전원 수명주기 구현
- [ ] MMA8652FC XYZ·INT1·latch 해제·오류 복구 구현
- [ ] BQ25628E 식별·상태·fault·보수적 충전 설정 구현
- [ ] GDEY0266T90H 전체 갱신·BUSY timeout·deep sleep·rail-off 구현
- [ ] 주변장치 실패가 LoRa, BLE와 USB 흐름을 막지 않도록 bounded wait 적용
- [ ] 코드 검토와 빌드 확인을 마친 기능을 `ieum/main`에 순차 병합

## MVP 이후

실기기가 제작된 뒤 다음 검증과 최적화를 진행하고, 발견된 차이는 후속 commit으로 보완한다.

- 완성 보드의 SWD·USB 복구와 RAK4631 부트로더 호환성 확인
- LoRa 송수신, BLE, I²C inventory와 각 센서 실기기 확인
- GNSS TTFF·off-state backfeed, BQ25628E 충전과 E-ink rail-off 측정
- 24시간 이상 통합 안정성 및 평균전류 측정
- MMA8652FC Auto-WAKE/SLEEP, FIFO, 탭·방향·충격 기능
- 움직임 상태에 따른 GNSS 측정 생략과 강제 갱신 정책
- BMP388 forced-mode 저전력 최적화
- E-ink 빠른/부분 갱신, 잔상 기준과 주기적 전체 갱신
- 버튼 2개와 LED 2개의 최종 UX 역할
- 2-3주 배터리 목표 최적화
- 제품 정책상 필요해질 때만 정식 Meshtastic HardwareModel 등록 검토
