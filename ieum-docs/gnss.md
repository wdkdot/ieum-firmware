# GNSS

## 부품

- 모듈: ATGM336H-5NR-32
- 기반 SoC: AT6558
- 인터페이스: UART
- 모듈 계열 크기: 약 9.7×10.1 mm
- 추적 채널: 32
- 지원 계열: GPS, BeiDou, GLONASS, Galileo, QZSS 및 SBAS
- 용도: 위치, 속도, 시간 및 위성 상태 획득

정확한 구매 suffix인 ATGM336H-5NR-32의 RF·Flash·출력 기본 설정은 보유 모듈 자료와 실물 출력을 대조한다. 일반 ATGM336H-5N 계열 데이터시트만으로 모든 옵션을 동일하다고 가정하지 않는다.

## 전원 구조

GNSS는 주 전원과 VBAT 백업 전원을 구분한다.

- 주 전원: 위치 측정 중 모듈 전체 동작
- VBAT: RTC와 획득 보조 정보를 유지하여 다음 시작 시간을 단축
- VCC_RF: 액티브 안테나 전원 바이어스
- RF 초크: RF 신호 경로와 DC 안테나 전원을 결합

주 전원을 끄더라도 백업 전원을 유지하면 완전한 cold start를 피할 가능성이 높다. 실제 warm/hot start 여부는 전원 차단 시간, 유효한 시간, 궤도 정보와 안테나 수신 상태에 좌우된다.

## UART

기본 출력은 NMEA 문장으로 예상되지만 baud rate, 출력 문장과 갱신 주기는 실물에서 확인해야 한다. 펌웨어는 RMC, GGA, GSA와 GSV 문장을 우선 처리할 수 있다.

UART RX/TX 직렬 저항과 MCU 핀은 회로도 및 핀맵에서 확정한다.

현재 펌웨어는 Meshtastic의 ATGM336H probe와 NMEA parser를 사용한다. GNSS 주 전원을 켠 뒤 1초의 임시 bring-up guard를 거쳐 `Serial1`을 RX P0.15/TX P0.16에 연결한다. fix 또는 timeout으로 측정을 마치면 UART 송신을 비우고 peripheral을 종료한 뒤 두 핀을 pull 없는 high-Z로 전환하고, 마지막으로 active-high GNSS EN을 LOW로 내려 주 전원을 차단한다. VBAT 백업은 이 경로에서 끄지 않는다.

1초 guard는 실측으로 확정된 최소 안정화 시간이 아니다. 실제 rail rise와 첫 정상 NMEA 출력 시점을 측정한 뒤 조정하며, UART off-state 핀 전압과 역급전 전류도 병합 전 확인한다.

## Ieum 위치 측정 정책

1. 기본 30분마다 위치 측정 시점을 만들며 `gps_update_interval` 설정을 따른다.
2. MMA8652FC가 장시간 정지를 나타내면 해당 측정을 생략할 수 있다.
3. 측정이 필요하면 GNSS 주 전원을 켠다.
4. 유효한 fix 또는 제한 시간 만료까지 기다린다.
5. 결과를 저장하고 Meshtastic 위치 데이터에 반영한다.
6. UART를 정리하고 GNSS 주 전원을 끈다.
7. VBAT 백업은 유지한다.

한 번의 fix 탐색은 최대 5분으로 제한한다. 이는 실내나 안테나 이상 상태에서 GNSS 주 전원이 다음 측정 시점까지 계속 켜지는 것을 막기 위한 초기 운용 한계이며, 실측 TTFF 분포를 확인한 뒤 조정한다.

정지 상태에서도 너무 오래 위치를 갱신하지 않는 문제를 막기 위해 최대 생략 횟수 또는 강제 갱신 간격을 둔다.

## 오류 처리

- 정해진 시간 안에 fix를 얻지 못하면 GNSS를 계속 켜두지 않는다.
- UART parser 오류가 전체 펌웨어를 막지 않게 한다.
- 유효하지 않은 fix를 이전의 정상 위치 위에 덮어쓰지 않는다.
- 디버그 로그에 전원 켜짐 시간, TTFF, 위성 수, fix 결과와 종료 이유를 남긴다.

기존 `HAS_GPS=0` 펌웨어에서 저장된 `gps_mode=NOT_PRESENT` 설정은 자동으로 변경하지 않는다. 해당 보드를 업데이트할 때 위치 설정에서 GNSS 모드를 `ENABLED`로 바꿔야 하며, 이후에는 `DISABLED`와 `ENABLED` 전환으로 전원 수명주기를 제어한다.
