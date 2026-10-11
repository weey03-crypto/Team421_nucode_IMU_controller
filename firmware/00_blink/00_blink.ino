/*
  00_blink.ino — 보드/업로드 환경 확인용. 온보드 LED(P0.13)만 깜빡인다.
  이 보드 LED는 active-low (LOW = ON).
*/

#define LED0_PIN  13
#define LED_ON    LOW
#define LED_OFF   HIGH

void setup() {
  pinMode(LED0_PIN, OUTPUT);
  digitalWrite(LED0_PIN, LED_OFF);
}

void loop() {
  digitalWrite(LED0_PIN, LED_ON);
  delay(500);
  digitalWrite(LED0_PIN, LED_OFF);
  delay(500);
}
