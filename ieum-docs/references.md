# 자료 출처

문서 작성 시 확인한 주요 자료다. 레지스터를 구현할 때는 링크된 최신 데이터시트의 revision을 다시 확인한다.

## 메인 모듈

- [RAKwireless RAK4630 Datasheet](https://docs.rakwireless.com/product-categories/wisduo/rak4630-module/datasheet/)
- [RAKwireless RAK4631 Datasheet](https://docs.rakwireless.com/product-categories/wisblock/rak4631/datasheet/)
- Nordic Semiconductor nRF52840 Product Specification — 공식 최신본 추가 필요
- Semtech SX1262 Datasheet — 공식 최신본 추가 필요

## 센서

- [ASAIR AHT20 Datasheet](https://www.aosong.com/userfiles/files/media/Data%20Sheet%20AHT20.pdf)
- [Bosch Sensortec BMP388 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp388-ds001.pdf)
- [NXP MMA8652FC Datasheet, Rev. 3.3](https://www.nxp.com/docs/en/data-sheet/MMA8652FC.pdf)
- [NXP MMA8653FC Datasheet, 비교용](https://www.nxp.com/docs/en/data-sheet/MMA8653FC.pdf)

## 전원

- [Texas Instruments BQ25628E Datasheet, SLUSFA4C, revised February 2025](https://www.ti.com/lit/ds/symlink/bq25628e.pdf)

## 디스플레이

- [Good Display GDEY0266T90H 제품 페이지 및 다운로드](https://www.good-display.com/product/501.html)
- [Good Display GDEY0266T90H ESP32 Sample Code](https://www.good-display.com/companyfile/1292.html)

제품 페이지에서 최신 `GDEY0266T90H Specification`, SSD1685 자료와 제조사 샘플 코드를 내려받아 사용한다. Ieum에 사용하는 패널은 2.66인치, 184×360, 흑백, SSD1685 사양이다.

## GNSS

- [ATGM336H-5N User Manual PDF](https://www.tinytronics.nl/product_files/002176_ATGM336H.pdf)

## 원문 추가가 필요한 부품

- Texas Instruments TPS22919-Q1
- SGMicro SGM6036-3.3
- H5VU25UC
- H5VH16U

## 프로젝트 내부 자료

- KiCad 회로도 및 PCB
- JLCPCB BOM/CPL
- 실제 구매 주문서의 제조사 부품 번호
- 보드 bring-up 측정 기록
- Meshtastic firmware의 고정 tag 또는 commit

## 추가 확인 항목

- BMP388_TOKMAS의 정확한 제조·주문 suffix와 필터 사양
- AHT20-F의 필터 사양
- ATGM336H-5NR-32가 일반 5N 계열과 달라지는 옵션
- H5VU25UC와 H5VH16U의 공식 데이터시트 출처
- SGM6036-3.3의 정확한 주문 suffix
