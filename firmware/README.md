# firmware/

NU40-DK V2(nRF52840) Arduino 스케치. 번호 순서대로 진행하는 것을 전제로 둔다.

## 01_mpu6050_hid_gamepad/

**가장 먼저 돌려볼 예제.** MPU6050으로 pitch/roll을 읽어 USB HID 게임패드 축으로
내보낸다. 목표는 단 하나: *"보드가 맥에 게임패드로 뜨고, 기울이면 축이 움직인다"*
를 확인하는 것. NUCODE 챌린지 최소 요건(실동작 데모)과 T4-08 핵심을 동시에 만족.

준비물: NU40-DK V2, GY-521(MPU6050), 점퍼 와이어 또는 Qwiic 케이블, USB-C 케이블.

배선: VCC→3V3, GND→GND, SDA→P0.26, SCL→P0.27 (또는 Qwiic 커넥터로 바로 연결).

설치:
1. Arduino IDE 환경설정 > 추가 보드 매니저 URL에
   `https://raw.githubusercontent.com/Nucode01/Adafruit_nRF52_Arduino/refs/heads/master/package_nuduino_index.json`
   추가
2. 보드 매니저에서 "NUCODE" 검색 → **NUBoards nRF52 by NUCODE** 설치
   (Adafruit TinyUSB, Bluefruit 라이브러리가 함께 설치됨)
3. 보드를 NU40-DK로 선택 후 이 스케치 업로드
4. 시리얼 모니터(115200)로 pitch/roll 값 확인
5. https://hardwaretester.com/gamepad 에서 축이 기울임에 따라 움직이는지 확인
6. FPV Freerider 컨트롤러 설정에서 축 매핑 후 비행 테스트

확인 안 되면 (TinyUSB HID가 안 잡히는 경우): 대안으로 Bluefruit BLE HID 게임패드로
전환해서 맥에 무선 페어링하는 방법을 시도 (`docs/` 참고, 추후 추가).

## 02_ble_gatt_notify/

(다음 단계) BLE GATT notify로 피치/롤/요/데드맨/슬라이드/버튼 상태를 라즈베리파이로
송신. `raspi/`의 bleak 수신 코드와 짝을 이룸. 아직 비어있음 — 1단계 확인 후 작성.
