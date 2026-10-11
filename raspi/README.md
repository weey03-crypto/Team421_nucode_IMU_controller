# raspi/

라즈베리파이에서 BLE GATT notify를 수신해 메카넘 UGV를 구동하는 코드가 들어갈 자리.
(우선순위 3단계. `firmware/02_ble_gatt_notify/` 완성 후 진행)

## 예정 구조

```
raspi/
  ble_receiver.py     # bleak으로 NU40-DK의 notify 수신, drive(vx, vy, w) 호출
  ugv_drive.py         # 메카넘휠 구동 함수 (기존 WASD 코드에서 입력부만 교체)
  ugv_service.py       # systemd 자동 실행용 엔트리포인트
```

## 설계 메모

- 입력부와 `drive(vx, vy, w)` 구동 함수를 분리해 테스트하기 쉽게 유지
- 페일세이프: notify가 0.3초 이상 끊기면 즉시 정지
- 부팅 시 자동 실행 (systemd)
- 맥에서도 같은 bleak 코드로 수신 확인/디버깅 가능하게 플랫폼 분기 없이 작성
