# Team421 — NUCODE IMU UxV Controller

소총(레일) 부착형 IMU 기울기 컨트롤러. 병사가 사격 자세와 전방 경계를 유지한 채
드론(UAV)과 메카넘 UGV를 하나의 컨트롤러로 조종한다.

- **대회**: D4D Hackathon (국방·방위산업 해커톤), 혼자 24시간 진행
- **메인 트랙**: T4 Tactical Edge — T4-08 IMU 기반 직관형 드론 조종 인터페이스
- **서브 챌린지**: SC2 NUCODE Board Challenge
- **보드**: NUCODE NU40-DK V2 (nRF52840, BLE 5.4 / 802.15.4, 네이티브 USB)

## 한 줄 정의

IMU 기울기 컨트롤러를 소총에 부착 → **USB HID 게임패드**로 드론 시뮬레이터(FPV Freerider) 조종,
**BLE GATT**로 라즈베리파이 메카넘 UGV 조종. 하나의 조종기로 공중·지상 무인체계 모두.

## 운용 개념

- 평소: UAV/UGV가 자율적으로 병사를 따라오며 표적 탐지 (**HOTL**, Human-on-the-Loop)
- 데드맨 스위치를 누르면: 잠시 수동 조종 (**HITL**, Human-in-the-Loop)
- 손을 떼면 정지/호버링, 복귀 버튼으로 다시 HOTL 재개
- 완전 자율 교전(**HOOTL**)은 의도적으로 배제 — 표적 확인·판단은 항상 사람

자세한 설계 근거(JARUS 자동화 수준, 가변 자율성, 실사례 비교 등)는 [`docs/concept.md`](docs/concept.md) 참고.

## 레포 구조

```
firmware/
  01_mpu6050_hid_gamepad/   # 1단계: IMU 기울기 → USB HID 게임패드 (최소 요건)
  02_ble_gatt_notify/       # 2단계: BLE GATT notify로 조종 데이터 송신
raspi/                      # 라즈베리파이 bleak 수신 + 메카넘 UGV 구동
docs/                       # 설계 문서, 핀 배정, 실험 설계
hardware/                   # 하드웨어 구성도, 배선도, 사진
```

## 하드웨어 요약 (NU40-DK V2 / nRF52840)

| 기능 | 핀 | 비고 |
|---|---|---|
| I2C SDA / SCL | P0.26 / P0.27 | MPU6050, OLED, 충전IC 공유 (Qwiic) |
| 데드맨 | P0.11 (S1) | 외부 스위치 병렬 |
| 슬라이드 상승/하강 | P0.03 / P0.04 | 내부 풀업, GND 단락 |
| 복귀 버튼 | P0.24 (S3) | |
| UxV 전환 버튼 | P0.25 (S4) | |
| 진동 모터 PWM | P0.28 | 트랜지스터 베이스 |
| 배터리 잔량 | P0.02 | 보드 내장 분압 |

전체 핀/전원/통신 구조는 [`docs/pinout.md`](docs/pinout.md) 참고.

## 개발 환경

- Arduino IDE, 추가 보드 매니저 URL:
  `https://raw.githubusercontent.com/Nucode01/Adafruit_nRF52_Arduino/refs/heads/master/package_nuduino_index.json`
- 보드 매니저에서 "NUCODE" 검색 → **NUBoards nRF52 by NUCODE** 설치
- 라이브러리: Adafruit TinyUSB (HID), Adafruit Bluefruit nRF52 (BLE)
- macOS + Arduino IDE, UF2 부트로더(USB 드래그앤드롭)

## 진행 순서 (우선순위)

1. **[진행 중]** NU40-DK + MPU6050 기울기 읽기 → USB HID 게임패드 인식 → FPV Freerider 조종
2. 엑스박스 컨트롤러 vs 이 장치 비교 실험 (학습곡선 / 정확도 / 경계유지 / NASA-TLX)
3. BLE GATT 송신 → 라즈베리파이 bleak 수신 → 메카넘 UGV 조종 + 페일세이프(0.3초 무신호 정지)
4. 진동 피드백 / OLED 디스플레이 / 버튼 기능
5. 반동 필터, 걷기 필터, 방향 재정렬
6. (옵션) 조이스틱+트리거로 표적 마킹

## 데모 / 제출물 (NUCODE 챌린지 요건)

- [ ] 실제 보드 1개 이상 실동작 데모 영상
- [ ] 하드웨어 구성도 (`hardware/`)
- [ ] 공개 GitHub 저장소 (코드 + 배선도 + README)

## 현재 미확인 항목

- 배터리 커넥터(1.25mm) 극성/규격, 단독 구동 시 3V3 핀 실전압
- 충전 IC(BQ25186) 기본 충전 전류 및 설정 방법
- TinyUSB HID 게임패드가 맥 + FPV Freerider에서 정상 인식되는지
- 행사장 총기 모형 반입 가능 여부
