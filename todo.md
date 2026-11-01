# Ieum 저전압 자동 종료 문제 분석 및 개선 계획

**작성일:** 2026-11-01  
**프로젝트:** Ieum Meshtastic Node  
**대상 저장소:** [wdkdot/ieum-firmware](https://github.com/wdkdot/ieum-firmware)  
**대상 하드웨어:** RAK4630 (nRF52840) + BQ25628E  
**상태:** 코드 수정·빌드·판정 호스트 테스트 및 실기기 BLE OTA 플래시 완료 / 저전압 동작 검증 필요

---

## 현재 구현 업데이트 (2026-11-01)

아래 원인 분석은 변경 전 코드의 기록이다. 실제 장치에서 보호회로 차단까지 진행된 원인은 설치 버전과 로그가 없어 아직 미확정이다. 현재 체크아웃은 PR #15를 포함한다.

이번 수정은 기존 Meshtastic `Power → EVENT_LOW_BATTERY → doDeepSleep → cpuDeepSleep` 흐름과 observer, 저장 API를 재사용한다. 새 전원관리 FSM이나 nRF52 플랫폼 전용 종료 구현은 추가하지 않는다.

- Ieum 보드 설정에만 검증용 초기값 **3.30 V 미만 / 최소 20초 간격 / 3회 연속**을 정의했다. OCV 잔량 계산은 그대로다. 최종 값은 실기기 검증 후 확정한다.
- USB 연결·상태 불명·배터리 없음·ADC 실패·0 V·전압 회복 시 카운터를 초기화한다. 이전 정상 ADC 캐시값은 종료 판정에 사용하지 않는다. 포화 카운터로 이벤트 중복을 막는다.
- Ieum 저전압 종료는 `portMAX_DELAY`를 사용하여 tracker/sensor 절전 설정에서도 일정 시간 후 리셋하는 경로를 피한다. NodeDB와 송신 이력 및 InkHUD의 기존 메시지/설정 저장을 재사용한다.
- InkHUD는 현재 갱신을 마친 뒤 열린 패널 세션을 닫고, 추가 종료 화면 갱신과 멜로디를 생략한다. 기존 preflight의 30초 timeout에서는 재부팅 대신 종료를 계속한다.
- I²C가 살아 있는 수면 종료 단계에서 BQ25628E 지연 Shutdown을 요청한다. 명령 직전 status 읽기 성공 및 USB 입력 부재가 필수다. 실패하면 BATFET 차단 없이 MCU 종료를 계속한다.
- `variant_shutdown()`은 nRF52의 `Wire.end()` 이후 실행되므로 PMIC 명령을 추가하지 않았다. 일반 종료와 일반 수면의 BATFET 정책은 유지했다.

상세한 현행 정책과 실기기 확인 항목은 [전원 시스템](ieum-docs/power-system.md)에 기록한다. 아래 완료 조건은 실측 성공 전까지 체크하지 않는다. 운영자가 BLE OTA로 실기기 플래시 완료를 확인했다. 저전압 종료·BATFET 차단·USB 복구 시험은 아직 미검증이다.

---

## 1. 문제 개요

### 1.1 발생 증상

Ieum 노드에서 배터리 전압이 낮아졌을 때 펌웨어가 정상적인 저전압 종료 절차를 수행하지 않고, 배터리 보호회로가 방전을 차단할 때까지 동작하는 문제가 관찰되었다.

기대 동작은 배터리 전압이 안전한 임계값 아래로 내려가면 MCU가 저전압 상태를 감지하고, 필요한 데이터를 저장한 후 전원 차단 또는 저전력 상태로 진입하는 것이다.

현재 관찰된 현상은 배터리 보호회로의 최종 차단에 의존하고 있다는 점에서 개선이 필요하다.

### 1.2 영향

- 배터리 보호회로가 동작하기 전까지 불필요한 방전 지속
- 전원 차단으로 인한 비정상적인 펌웨어 종료 가능성
- 저장하지 않은 설정 및 상태 데이터의 손실 가능성
- 배터리 과방전 및 수명 저하 위험 증가
- 정상 종료와 배터리 보호회로 차단을 구별하기 어려움

## 2. 하드웨어 및 펌웨어 구조

### 2.1 전원 공급 경로

Ieum의 전원 구조는 다음과 같다.

`LiPo 배터리 ↔ BQ25628E → SYS → SGM6036-3.3 → 3V3_MAIN → RAK4630`

- **BQ25628E:** 배터리 충전, Power Path, 전압·전류 측정 및 BATFET 제어
- **SGM6036-3.3:** SYS 전압을 3.3V로 변환
- **RAK4630:** Meshtastic 펌웨어 실행 및 전원관리 제어
- **배터리 보호회로:** 셀의 최종 과방전 보호

배터리 전압은 RAK4630의 ADC 분압 회로가 아닌 BQ25628E의 내장 ADC를 통해 측정한다.

### 2.2 정상적인 저전압 종료 조건

기존 Meshtastic 펌웨어의 `src/Power.cpp`에는 다음과 같은 저전압 보호 로직이 구현되어 있다.

- 배터리가 연결된 상태일 것
- USB 외부 전원이 없을 것
- 배터리 전압이 OCV 테이블의 최솟값보다 낮을 것
- 해당 조건이 11회 연속 감지될 것

Ieum에 사용된 기본 OCV 테이블의 최솟값은 **3,100mV**이다.

전원 상태를 약 20초 간격으로 측정하므로, 첫 번째 저전압 측정 이후 종료 조건이 충족되기까지 약 200초가 필요하다.

해당 조건이 충족되면 `EVENT_LOW_BATTERY`가 발생한다.

## 3. 소스 및 커밋 분석

### 3.1 초기 BQ25628E 통합의 문제

**관련 커밋:** [2d0e886f — Add initial BQ25628E power support (#5)](https://github.com/wdkdot/ieum-firmware/commit/2d0e886f6f9b338166f4d9c0f16f84e87745285c)

초기 구현에서는 BQ25628E의 ADC 측정값을 읽어 Meshtastic의 `PowerStatus`에 반영했다.

그러나 저전압 감지 로직은 별도의 `batteryLevel` 객체에서 전압을 읽었다.

`Power::setup()`에서 BQ25628E 초기화에 성공하더라도 `batteryLevel`에 해당 전압 공급자가 등록되지 않았기 때문에, ADC 분압 회로가 없는 Ieum에서는 `batteryLevel`이 null 상태로 남을 수 있었다.

그 결과 다음과 같은 상황이 발생할 수 있었다.

| 항목                       | 초기 구현             |
| -------------------------- | --------------------- |
| BQ25628E ADC 측정          | 지원                  |
| 배터리 전압 상태 표시      | 지원                  |
| `batteryLevel` 공급자 등록 | 누락                  |
| 저전압 카운터 증가         | 실행되지 않을 수 있음 |
| 저전압 종료 이벤트         | 발생하지 않을 수 있음 |

이는 배터리 전압이 정상적으로 표시되더라도 자동 종료가 실행되지 않는 현상을 설명할 수 있는 직접적인 소프트웨어 결함이다.

### 3.2 기존 수정 커밋 확인

**관련 커밋:** [eaa12f12 — Register BQ25628E with existing low-voltage protection (#15)](https://github.com/wdkdot/ieum-firmware/commit/eaa12f12e16929d035e584914becf8553259db0d)

**커밋 날짜:** 2026-08-21

해당 커밋에서는 BQ25628E를 기존 Meshtastic 배터리 관리 시스템에 통합하는 작업이 이루어졌다.

주요 수정 내용:

1. `BQ25628EBatteryLevel` 어댑터 추가
2. BQ25628E 초기화 성공 시 `batteryLevel` 등록
3. `PowerStatus`와 저전압 감지 경로의 전압 공급원 통합
4. USB 입력 상태 및 배터리 존재 판정 처리
5. 저전압 환경에서도 BQ25628E ADC 측정이 유지되도록 개선

특히 BQ25628E는 배터리 단독 구동 중 약 3.2V 미만에서 TS ADC가 사용하는 REGN 전원을 이용할 수 없으므로, 해당 조건을 고려하지 않으면 ADC 변환이 실패할 수 있다.

이를 해결하기 위해 USB 입력이 없는 경우 TS ADC 채널을 비활성화하고 VBAT 측정을 유지하도록 변경했다.

**분석 결과:** 기존의 전압 공급자 미등록 및 저전압 ADC 변환 문제는 PR #15에서 소스 수준으로 수정되었다.

단, 실제 기기에 해당 커밋이 반영된 펌웨어가 설치되어 있는지는 확인되지 않았다.

### 3.3 현재 저전압 종료 처리의 한계

현재 `src/PowerFSM.cpp`에서는 저전압 이벤트 발생 시 일반적인 종료 함수 대신 `lowBattSDSEnter()`를 실행한다.

이 함수는 `doDeepSleep(..., false, true)`를 호출한다.

이로 인해 다음과 같은 차이가 발생한다.

| 기능                      | 일반 종료 | 저전압 종료    |
| ------------------------- | --------- | -------------- |
| 종료 화면 표시            | 지원      | 별도 처리 없음 |
| NodeDB 명시적 저장        | 수행      | 생략           |
| 메시지 저장소 명시적 저장 | 수행      | 별도 호출 없음 |
| nRF52840 저전력 처리      | 수행      | 수행           |
| BQ25628E BATFET 차단      | 미연동    | 미연동         |

특히 세 번째 인자인 `skipSaveNodeDb=true`로 인해 Deep Sleep 진입 과정에서 NodeDB 저장을 생략한다.

기존 저전압 처리 로직은 일반적인 종료 절차와 동일하지 않으며, 필요한 데이터 저장과 전원 차단 정책을 별도로 개선할 필요가 있다.

### 3.4 BQ25628E Shutdown 미연동

현재 `src/power/BQ25628E.cpp`에는 다음 API가 존재한다.

- `enterShipMode(bool delayed)`
- `enterShutdownMode(bool delayed)`

하지만 저전압 종료 경로에서는 이 함수를 호출하지 않는다.

nRF52840의 System OFF는 MCU의 소비전력을 줄이지만 BQ25628E의 BATFET을 차단하지 않는다. 따라서 SYS 전원 경로가 유지될 수 있다.

Ieum의 `variant_shutdown()`도 GNSS, E-Ink 및 LED GPIO를 정리할 뿐 BATFET 제어는 수행하지 않는다.

즉, 현재 펌웨어에는 **MCU 종료와 배터리 전원 경로 차단을 연계하는 동작이 구현되어 있지 않다.**

## 4. 원인 및 해결 상태

| 원인                                           | 분석 결과                           | 상태             |
| ---------------------------------------------- | ----------------------------------- | ---------------- |
| BQ25628E 전압 공급자 미등록                    | 초기 구현에서 확인                  | PR #15 수정      |
| 저전압 구간 ADC 변환 실패 가능성               | TS ADC 및 REGN 의존성 확인          | PR #15 개선      |
| 저전압 감지 후 일반 종료 절차 생략             | PowerFSM에서 확인                   | 개선 필요        |
| NodeDB 저장 생략                               | `skipSaveNodeDb=true` 확인          | 정책 검토 필요   |
| BQ25628E BATFET 차단 미연동                    | Shutdown API는 있으나 호출하지 않음 | 개선 필요        |
| 실제 장치에서 종료가 실행되지 않는 정확한 조건 | 펌웨어 버전 및 로그 미확인          | 실기기 검증 필요 |

## 5. 권장 개선 방향

### 5.1 저전압 감지 경로 검증

먼저 현재 기기에 적용된 펌웨어가 PR #15를 포함하는지 확인한다.

해당 커밋이 포함되어 있다면 다음 상태를 점검한다.

- BQ25628E 초기화 및 I²C 통신 성공 여부
- ADC 측정값 유효성
- USB 입력 상태 판정
- 배터리 연결 상태 판정
- 저전압 카운터 증가 여부
- `EVENT_LOW_BATTERY` 발생 여부

ADC 오류가 발생하거나 측정값이 무효화되면 잘못된 전압을 근거로 종료하지 않도록 해야 하며, 오류 상태는 별도로 기록하는 것이 좋다.

### 5.2 Ieum 전용 종료 임계값 도입

기본 임계값 3.10V 대신 Ieum의 배터리와 전원 구조를 고려한 별도 임계값을 사용하는 방안을 검토한다.

초기 검증 후보로는 3.2~3.3V 수준을 고려할 수 있으나, 최종 값은 배터리 방전 특성과 실제 부하에서의 순간 전압 강하를 측정해 결정한다.

또한 기존 OCV 테이블의 최솟값을 변경하면 잔량 계산에도 영향을 줄 수 있으므로, 배터리 잔량 계산과 자동 종료 임계값은 분리하는 것이 적절하다.

연속 저전압 판정 횟수와 측정 주기 역시 보호회로 차단 전에 종료 작업을 완료할 수 있도록 검토해야 한다.

### 5.3 저전압 종료 절차 개선

권장 종료 절차는 다음과 같다.

1. 저전압 조건 충족
2. 중복 종료 요청 방지 및 새로운 작업 시작 억제
3. 진행 중인 LoRa 송신과 주변장치 작업 정리
4. 필요한 설정 및 상태 데이터 저장
5. 가능하다면 E-Ink 종료 안내 표시
6. GNSS와 E-Ink 전원 및 GPIO 정리
7. BQ25628E에 지연 Shutdown 명령 전송
8. nRF52840 System OFF 진입
9. BATFET 차단 후 USB 연결 시 재부팅

E-Ink 화면 갱신과 플래시 저장은 전력을 소비하므로, 실제 종료 시점에 배터리 전압이 충분하지 않다면 생략하거나 제한하는 정책도 필요하다.

### 5.4 BQ25628E Shutdown Mode 연동

Ieum은 저전압 종료 시 BQ25628E의 Shutdown Mode를 사용하는 것이 적합한 후보이다.

기대 효과:

- BATFET 차단으로 SYS 전력 공급 중단
- MCU뿐 아니라 시스템 전체의 불필요한 소비전력 감소
- 배터리 보호회로가 최종 차단하기 전에 방전 중단
- USB 충전기 연결을 통한 재시작

드라이버에 구현된 `enterShutdownMode(true)`는 지연 BATFET 차단을 요청하며, 설정상 지연 시간은 약 12.5초이다.

주의사항:

- I²C 통신이 종료되기 전에 명령을 전송해야 한다.
- USB 입력이 없는 상태에서 실행해야 한다.
- I²C 명령 실패 시에도 MCU가 안전한 저전력 상태로 진입해야 한다.
- BATFET 차단으로 인한 전원 공급 중단이 실제 하드웨어에서 정상적으로 이루어지는지 검증해야 한다.
- 사용자에 의한 일반 종료와 저전압 보호 종료는 서로 다른 정책이 필요할 수 있다.

## 6. 실기기 검증 계획

### 6.1 시험 환경

배터리 보호회로가 강제로 차단되기 전에 동작을 관찰할 수 있도록, 전류 제한 기능이 있는 가변 DC 전원공급기를 이용해 배터리 입력 전압을 모사하는 방식을 권장한다.

리튬 배터리를 실제로 과방전시키는 시험은 피한다.

확인 항목:

- BQ25628E의 VBAT 측정값
- SYS 전압
- 3V3_MAIN 전압
- MCU 종료 여부
- BATFET 차단 여부
- USB 재연결 후 정상 부팅 여부

### 6.2 시험 시나리오

| 시험                           | 기대 결과                    |
| ------------------------------ | ---------------------------- |
| 정상 전압에서 배터리 단독 구동 | 정상 작동                    |
| 종료 임계값 미만으로 전압 하강 | 저전압 카운터 증가           |
| 일시적인 전압 강하 후 복구     | 종료 취소 또는 카운터 초기화 |
| 연속 저전압 유지               | 종료 이벤트 발생             |
| 저전압 상태에서 ADC 오류 발생  | 안전한 오류 처리             |
| 저전압 종료 명령 실행          | MCU System OFF               |
| Shutdown Mode 실행             | SYS 공급 차단                |
| 종료 후 USB 연결               | 정상 부팅                    |

### 6.3 로그 확인

주요 확인 로그:

- `BQ25628E initialization recovered`
- `BQ25628E ADC conversion failed`
- `Battery: usbPower=..., batMv=...`
- `Low voltage counter: ...`
- `Low voltage detected, trigger deep sleep`
- `State: Lower batt SDS`

종료 기능 개선 후에는 다음과 같은 전용 로그도 추가하는 것이 좋다.

- 저전압 종료 요청 및 측정 전압
- 종료 처리 시작 및 완료
- 데이터 저장 성공 여부
- BQ25628E Shutdown 명령 성공 여부
- 최종 System OFF 진입

## 7. 완료 조건

다음 조건이 모두 충족되면 저전압 자동 종료 기능이 정상적으로 구현된 것으로 판단한다.

- [ ] BQ25628E의 배터리 전압이 저전압 보호 로직에 정상 전달됨
- [ ] ADC가 저전압 구간에서도 안정적으로 측정됨
- [ ] 설정한 임계값 이하에서 저전압 이벤트가 발생함
- [ ] 순간적인 전압 강하로 인한 불필요한 종료가 방지됨
- [ ] 종료 전에 필요한 상태 데이터가 안전하게 처리됨
- [ ] nRF52840이 정상적으로 System OFF에 진입함
- [ ] BQ25628E Shutdown Mode가 정상 실행됨
- [ ] BATFET이 차단되어 SYS 공급이 중단됨
- [ ] USB 전원 연결 시 정상적으로 재부팅함
- [ ] 정상 종료 및 재부팅 과정에서 설정 데이터가 유지됨

## 8. 결론

Ieum의 저전압 종료 문제는 두 가지로 구분할 수 있다.

**첫 번째는 초기 BQ25628E 통합 과정에서 발생한 소프트웨어 결함이다.** 측정된 배터리 전압이 기존 Meshtastic의 저전압 보호 로직에 정상적으로 연결되지 않았으며, 이는 2026년 8월 21일의 PR #15에서 수정되었다.

**두 번째는 현재도 남아 있는 전원관리 정책의 한계이다.** 저전압 이벤트가 발생하면 기존 Meshtastic의 Deep Sleep 경로로 진입하지만, 일반적인 종료 과정과 BQ25628E의 BATFET 차단은 수행하지 않는다.

따라서 향후 작업의 우선순위는 다음과 같다.

1. 실제 기기에 설치된 펌웨어 버전 및 PR #15 반영 여부 확인
2. 저전압 감지와 System OFF 동작의 실기기 검증
3. Ieum 전용 종료 임계값 및 종료 절차 설계
4. BQ25628E Shutdown Mode 연동
5. 방전·종료·USB 재부팅 전체 과정 검증

최종 목표는 **배터리 보호회로의 강제 차단에 의존하지 않고, 펌웨어가 배터리 저전압을 사전에 감지하여 데이터를 안전하게 처리하고 시스템 전원을 차단하도록 만드는 것**이다.

---

## 9. 참고 소스 및 커밋

**Ieum 펌웨어**

- [wdkdot/ieum-firmware](https://github.com/wdkdot/ieum-firmware)
- [PR #5 — 초기 BQ25628E 전원 지원](https://github.com/wdkdot/ieum-firmware/commit/2d0e886f6f9b338166f4d9c0f16f84e87745285c)
- [PR #13 — BQ25628E 충전 설정 영속화](https://github.com/wdkdot/ieum-firmware/commit/cf156eb6e10a088681a6d2708e4adc9e23ad2722)
- [PR #15 — 저전압 보호 연동 수정](https://github.com/wdkdot/ieum-firmware/commit/eaa12f12e16929d035e584914becf8553259db0d)

**주요 소스 파일**

- [`src/Power.cpp`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/src/Power.cpp)
- [`src/PowerFSM.cpp`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/src/PowerFSM.cpp)
- [`src/sleep.cpp`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/src/sleep.cpp)
- [`src/power/BQ25628E.cpp`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/src/power/BQ25628E.cpp)
- [`src/power/BQ25628E.h`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/src/power/BQ25628E.h)
- [`variants/nrf52840/ieum/variant.cpp`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/variants/nrf52840/ieum/variant.cpp)
- [`ieum-docs/power-system.md`](https://github.com/wdkdot/ieum-firmware/blob/ieum/main/ieum-docs/power-system.md)

**Meshtastic 원본**

- [meshtastic/firmware](https://github.com/meshtastic/firmware)
- [저전압 카운터 처리 개선 커밋 #11821](https://github.com/meshtastic/firmware/commit/8a9e10d120f21133f55dc3642d50bc87fbcc183b)

마지막 커밋은 2026년 9월 상위 Meshtastic에서 이루어진 저전압 카운터 관련 수정으로, Ieum 포크에는 별도 반영 여부를 확인해야 한다.
