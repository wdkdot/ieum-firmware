# Ieum 하드웨어 및 펌웨어 문서

Ieum은 RAK4630을 중심으로 제작한 휴대용 Meshtastic 노드다. LoRa 메시 통신, Bluetooth LE, E-ink 상태 표시, GNSS 위치 측정, 온습도·기압·가속도 측정을 하나의 배터리 구동 장치에 통합한다.

이 문서는 사람이 설계를 다시 이해하는 용도와 Codex가 펌웨어를 개발할 때 하드웨어의 사실 관계와 설계 의도를 확인하는 용도를 겸한다.

## 현재 기준

- PCB: 4층, 1.6 mm, 5대 제작
- 메인 모듈: RAK4630 (nRF52840 + SX1262)
- 배터리: 3,000 mAh 리튬 폴리머
- 기반 소프트웨어: Meshtastic firmware 포크 예정
- 문서 기준일: 2026-07-12
- 핀맵: RAK4630 모듈 핀과 nRF52840 GPIO 대응 확인 완료

## 문서

- [하드웨어 개요](hardware-overview.md)
- [RAK4630 핀 매핑](pin-mapping.md)
- [전원 시스템](power-system.md)
- [센서](sensors.md)
- [E-ink 디스플레이](display.md)
- [GNSS](gnss.md)
- [USB, 보호 회로 및 디버깅](usb-and-debug.md)
- [펌웨어 구조와 동작 정책](firmware-architecture.md)
- [Meshtastic 펌웨어 적용 계획](firmware-integration-plan.md)
- [펌웨어 MVP](mvp.md)
- [전력 운용 계획](power-budget.md)
- [초기 구동 및 검사 절차](bring-up-guide.md)
- [자료 출처](references.md)

## 문서 해석 규칙

- **확정**은 실제 발주 설계 또는 사용 부품으로 확인된 내용이다.
- **계획**은 펌웨어에서 구현하려는 동작이며 아직 검증되지 않을 수 있다.
- **TODO**는 회로도, PCB, 실물 또는 데이터시트와 추가 대조가 필요한 내용이다.
- 부품 데이터시트의 능력과 Ieum에서 실제 사용할 기능을 구분한다.
- GPIO 연결은 [RAK4630 핀 매핑](pin-mapping.md)을 기준으로 한다. 활성 레벨과 풀업·풀다운은 문서에서 미확정으로 표시된 경우 추측하지 않는다.
