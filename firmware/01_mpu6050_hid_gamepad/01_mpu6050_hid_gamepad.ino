/*
  01_mpu6050_hid_gamepad.ino
  ---------------------------------------------------------------------------
  목표(우선순위 1): NU40-DK V2 + MPU6050 기울기 읽기 -> USB HID 게임패드로
  맥에 인식 -> FPV Freerider 조종 입력으로 사용.

  보드: NUCODE NU40-DK V2 (nRF52840)
  라이브러리: Adafruit TinyUSB (Arduino 보드매니저 "NUBoards nRF52 by NUCODE"
              설치 시 함께 설치됨)
  센서: GY-521 MPU6050, I2C 0x68 (SDA=P0.26, SCL=P0.27, 또는 Qwiic 커넥터)

  * 이 1단계 예제는 가속도계만으로 pitch/roll을 계산하는 "가장 단순한" 버전이다.
    흔들림에 약하므로, 추후 자이로를 섞는 Madgwick/Mahony 필터로 교체할 것
    (우선순위 5: 반동/걷기 필터 단계에서 같이 작업).
  * 지금 단계의 목적은 "보드가 HID 게임패드로 맥에 잡히고, 기울이면 축이 움직인다"
    는 것을 최대한 빨리 확인하는 것.

  확인 방법 (macOS):
    1) 이 스케치를 업로드
    2) 시스템정보 > USB 에서 장치가 보이는지 확인, 또는
       https://hardwaretester.com/gamepad 에서 축이 움직이는지 확인
    3) FPV Freerider 컨트롤러 설정에서 축 매핑
  ---------------------------------------------------------------------------
*/

#include <Wire.h>
#include <Adafruit_TinyUSB.h>

// ----- MPU6050 -----
#define MPU_ADDR        0x68
#define MPU_REG_PWR_MGMT_1   0x6B
#define MPU_REG_ACCEL_XOUT_H 0x3B

// ----- 데드존 / 매핑 범위 -----
// 손을 "중립"으로 들었을 때 흔들림을 무시하기 위한 데드존 (0~127 기준)
const int8_t  DEADZONE_8BIT   = 8;
// 가속도계 각도(度) -> HID 축(-127~127) 매핑에 쓸 기울기 한계값
// (총기 장착 기준, pitch/roll ±45도 정도를 풀스케일로 가정. 실측 후 조정)
const float   ANGLE_RANGE_DEG = 45.0;

// ----- HID 게임패드 디스크립터 -----
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};

Adafruit_USBD_HID usb_hid;
hid_gamepad_report_t gp_report;

float accX, accY, accZ;

void mpu6050_init() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_REG_PWR_MGMT_1);
  Wire.write(0x00);              // sleep 해제
  Wire.endTransmission(true);
}

bool mpu6050_read_accel() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return false;

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  // +-2g 기본 설정 기준 스케일 (16384 LSB/g)
  accX = rawX / 16384.0f;
  accY = rawY / 16384.0f;
  accZ = rawZ / 16384.0f;
  return true;
}

int8_t angleToAxis(float angleDeg) {
  float v = (angleDeg / ANGLE_RANGE_DEG) * 127.0f;
  if (v > 127) v = 127;
  if (v < -127) v = -127;
  int8_t out = (int8_t)v;
  if (abs(out) < DEADZONE_8BIT) out = 0;
  return out;
}

void setup() {
  Serial.begin(115200);

  // NU40-DK 기본 I2C 핀 = P0.26(SDA)/P0.27(SCL). Wire.begin()이 자동으로 사용.
  Wire.begin();
  mpu6050_init();

  // --- TinyUSB HID 게임패드 등록 ---
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // 네이티브 USB가 맥에 마운트될 때까지 대기
  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }

  memset(&gp_report, 0, sizeof(gp_report));

  Serial.println("NU40-DK HID gamepad ready.");
}

void loop() {
  if (mpu6050_read_accel()) {
    // pitch: 총구 위/아래, roll: 좌우 캔트
    float pitch = atan2(-accX, sqrt(accY * accY + accZ * accZ)) * 180.0f / PI;
    float roll  = atan2(accY, accZ) * 180.0f / PI;

    gp_report.y = angleToAxis(pitch); // 전진/후진 축 (FPV Freerider에서 매핑)
    gp_report.x = angleToAxis(roll);  // 좌/우 축

    Serial.print("pitch="); Serial.print(pitch, 1);
    Serial.print(" roll="); Serial.print(roll, 1);
    Serial.print(" -> x="); Serial.print(gp_report.x);
    Serial.print(" y="); Serial.println(gp_report.y);
  }

  if (usb_hid.ready()) {
    usb_hid.sendReport(0, &gp_report, sizeof(gp_report));
  }

  delay(10); // 약 50~100Hz
}
