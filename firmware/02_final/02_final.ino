/*
  04_optimization.ino
  ---------------------------------------------------------------------------
  최종 통합 스케치 (드론 전용, UGV/BLE 제외)
  - MPU6050: pitch/roll(가속도) + yaw rate(자이로 Z)
  - USB HID 게임패드(TinyUSB): 드론 시뮬레이터(FPV Freerider) 조종용
  - OLED(SSD1306): heading-up HUD (HITL/HOTL 상태, 타겟 A/B 베어링, 배터리)
  - Serial(115200): 디버그/모니터링용 CSV 출력 — 별도 웹 모니터(imu_monitor.html)에서 사용
  - I2C 400kHz: display.display()의 블로킹 시간을 줄여 USB HID 서비싱 지연 완화 시도

  보드: NUCODE NU40-DK V2 (nRF52840)
  ---------------------------------------------------------------------------
*/

#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----------------------------- MPU6050 -------------------------------------
#define MPU_ADDR         0x68
#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_GYRO_ZOUT_H  0x47

// ----------------------------- 핀 정의 --------------------------------------
#define DEADMAN_PIN   11   // P0.11 (S1)
#define SLIDE_UP_PIN   3   // P0.03 (상승 리미트스위치)
#define SLIDE_DOWN_PIN 4   // P0.04 (하강 리미트스위치)
#define RETURN_PIN    24   // P0.24 (S3)
#define UXV_PIN       25   // P0.25 (S4) — 현재는 드론 단일 모드라 표시만
#define VBAT_PIN       2   // P0.02 배터리 분압

// ----------------------------- 매핑 상수 ------------------------------------
#define DEADZONE_8BIT     8
#define ANGLE_RANGE_DEG   45.0
#define YAW_RATE_RANGE    150.0
#define SLIDE_STEP        3
#define SLIDE_MAX         127
#define SLIDE_MIN        -127

// ----------------------------- OLED -----------------------------------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ----------------------------- USB HID 게임패드 -----------------------------
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_GAMEPAD() };
Adafruit_USBD_HID usb_hid;
hid_gamepad_report_t gp_report;

// ----------------------------- 센서/상태 변수 -------------------------------
float accX, accY, accZ, gyroZ;
int8_t altitudeValue = 0;

bool uxvIsDrone = true;
bool prevUxvPressed = false;
bool prevDeadmanPressed = false;

float headingDeg = 0;
uint32_t lastHeadingUpdate = 0;

// ----------------------------- 타겟(HUD용 더미) -----------------------------
struct Target {
  char label;
  bool connected;
  float worldBearingDeg;
  float distanceM;
  uint8_t batteryPct;
};
Target targetA = {'A', true, 45.0f, 8.0f, 70};
Target targetB = {'B', true, 200.0f, 43.0f, 35};

// Arduino 자동 프로토타입 생성 버그 우회용 전방 선언
// (struct 타입을 매개변수로 받는 함수는 명시적으로 선언해줘야 함)
void drawTargetLine(Target &t);

// =============================================================================
// MPU6050
// =============================================================================
void mpu6050_init() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_PWR_MGMT_1);
  Wire.write(0x00);              // 슬립 해제
  Wire.endTransmission(true);
}

bool mpu6050_read() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 6) != 6) return false;

  int16_t rawAX = (Wire.read() << 8) | Wire.read();
  int16_t rawAY = (Wire.read() << 8) | Wire.read();
  int16_t rawAZ = (Wire.read() << 8) | Wire.read();

  accX = rawAX / 16384.0;
  accY = rawAY / 16384.0;
  accZ = rawAZ / 16384.0;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_GYRO_ZOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 2) != 2) return false;

  int16_t rawGZ = (Wire.read() << 8) | Wire.read();
  gyroZ = rawGZ / 131.0;   // deg/s

  return true;
}

// =============================================================================
// 매핑 유틸
// =============================================================================
int8_t rangeToAxis(float v, float range) {
  float clamped = constrain(v, -range, range);
  int16_t mapped = (int16_t)((clamped / range) * 127.0);
  if (abs(mapped) < DEADZONE_8BIT) mapped = 0;
  return (int8_t)constrain(mapped, -127, 127);
}

void updateHeading() {
  uint32_t now = millis();
  float dt = (now - lastHeadingUpdate) / 1000.0;
  lastHeadingUpdate = now;
  headingDeg += gyroZ * dt;
  while (headingDeg < 0)   headingDeg += 360.0;
  while (headingDeg >= 360) headingDeg -= 360.0;
}

// =============================================================================
// OLED HUD
// =============================================================================
void drawBatteryIcon(int x, int y, uint8_t pct) {
  display.drawRect(x, y, 14, 6, SSD1306_WHITE);
  display.drawRect(x + 14, y + 2, 1, 2, SSD1306_WHITE);  // +극 돌출
  int fillW = map(pct, 0, 100, 0, 12);
  if (fillW > 0) display.fillRect(x + 1, y + 1, fillW, 4, SSD1306_WHITE);
}

void drawTargetLine(Target &t) {
  if (!t.connected) return;

  float relBearing = t.worldBearingDeg - headingDeg;
  while (relBearing < -180) relBearing += 360;
  while (relBearing > 180)  relBearing -= 360;

  float rad = relBearing * PI / 180.0;
  int cx = 64, cy = 40;
  int lineLen = 26;
  int x2 = cx + (int)(sin(rad) * lineLen);
  int y2 = cy - (int)(cos(rad) * lineLen);

  display.drawLine(cx, cy, x2, y2, SSD1306_WHITE);

  int labelX = constrain(x2 - 4, 0, 118);
  int labelY = constrain(y2 - 4, 0, 50);
  display.setCursor(labelX, labelY);
  display.print(t.label);
  display.print(':');
  display.print((int)t.distanceM);
}

void drawHUD(bool deadman) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 상단: 타겟 A/B 배터리
  drawBatteryIcon(2, 2, targetA.batteryPct);
  display.setCursor(18, 2);
  display.print("A");

  drawBatteryIcon(100, 2, targetB.batteryPct);
  display.setCursor(96, 2);
  display.print("B");

  // 중앙: 플레이어 삼각형 마커 (항상 정면 고정, heading-up)
  int cx = 64, cy = 40;
  display.fillTriangle(cx, cy - 6, cx - 5, cy + 5, cx + 5, cy + 5, SSD1306_WHITE);

  // 타겟 베어링 라인
  drawTargetLine(targetA);
  drawTargetLine(targetB);

  // 하단: 상태 텍스트
  display.setCursor(0, 56);
  display.print(deadman ? "HITL" : "HOTL");
  display.print(" UxV:");
  display.print(uxvIsDrone ? "DRONE" : "UGV");

  display.display();
}

// =============================================================================
// setup / loop
// =============================================================================
void setup() {
  delay(250);   // OLED 내부 charge pump 안정화 대기 (콜드부트 레이스 방지)

  pinMode(DEADMAN_PIN, INPUT_PULLUP);
  pinMode(SLIDE_UP_PIN, INPUT_PULLUP);
  pinMode(SLIDE_DOWN_PIN, INPUT_PULLUP);
  pinMode(RETURN_PIN, INPUT_PULLUP);
  pinMode(UXV_PIN, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000);   // 100kHz -> 400kHz, display.display() 블로킹 시간 단축

  mpu6050_init();

  int attempts = 0;
  while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C) && attempts < 5) {
    delay(100);
    attempts++;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("NU40-DK ready");
  display.display();

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  while (!TinyUSBDevice.mounted()) delay(1);

  memset(&gp_report, 0, sizeof(gp_report));
  lastHeadingUpdate = millis();

  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 1500) {
    delay(10);   // 모니터 안 열려있어도 1.5초 후엔 그냥 진행 (블로킹 방지)
  }
}

void loop() {
  bool deadman   = (digitalRead(DEADMAN_PIN)   == LOW);
  bool slideUp   = (digitalRead(SLIDE_UP_PIN)   == LOW);
  bool slideDown = (digitalRead(SLIDE_DOWN_PIN) == LOW);
  bool returnBtn = (digitalRead(RETURN_PIN)   == LOW);
  bool uxvBtn    = (digitalRead(UXV_PIN)      == LOW);

  prevDeadmanPressed = deadman;

  if (uxvBtn && !prevUxvPressed) {
    uxvIsDrone = !uxvIsDrone;
  }
  prevUxvPressed = uxvBtn;

  mpu6050_read();
  updateHeading();

  // 가속도 기반 pitch/roll (atan2)
  float pitch = atan2(accY, sqrt(accX * accX + accZ * accZ)) * 180.0 / PI;
  float roll  = atan2(-accX, accZ) * 180.0 / PI;

  if (deadman) {
    gp_report.y  = rangeToAxis(pitch, ANGLE_RANGE_DEG);
    gp_report.x  = rangeToAxis(roll, ANGLE_RANGE_DEG);
    gp_report.rz = rangeToAxis(gyroZ, YAW_RATE_RANGE);

    if (slideUp)   altitudeValue = constrain(altitudeValue + SLIDE_STEP, SLIDE_MIN, SLIDE_MAX);
    if (slideDown) altitudeValue = constrain(altitudeValue - SLIDE_STEP, SLIDE_MIN, SLIDE_MAX);
    gp_report.z = altitudeValue;

    gp_report.buttons = (1 << 0);
  } else {
    gp_report.y  = 0;
    gp_report.x  = 0;
    gp_report.rz = 0;
    gp_report.z  = altitudeValue;   // 고도는 유지 (호버링 흉내)
    gp_report.buttons = 0;
  }

  if (usb_hid.ready()) {
    usb_hid.sendReport(0, &gp_report, sizeof(gp_report));
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 100) {
    lastDraw = millis();
    drawHUD(deadman);
  }

  static uint32_t lastSerial = 0;
  if (millis() - lastSerial > 50) {   // 20Hz
    lastSerial = millis();
    Serial.print(pitch, 1);           Serial.print(',');
    Serial.print(roll, 1);            Serial.print(',');
    Serial.print(gyroZ, 1);           Serial.print(',');
    Serial.print(headingDeg, 1);      Serial.print(',');
    Serial.print(altitudeValue);      Serial.print(',');
    Serial.print(deadman ? 1 : 0);    Serial.print(',');
    Serial.print(uxvIsDrone ? 1 : 0); Serial.print(',');
    Serial.println(returnBtn ? 1 : 0);
  }

  delay(10);
}
