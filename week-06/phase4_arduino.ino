// ESP32-CAM: your working esp32cam image server + servo on GPIO14.
// The PC (face_trigger.py) detects the face. When it sees one it
//   1) keeps calling  http://<ip>/servo   -> servo sweeps
//   2) saves the photo itself
// No calls for 1 second (face gone) -> servo stops and goes limp.
//
// Board: ESP32 Wrover Module, PSRAM enabled, 115200 baud.

#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

const char* WIFI_SSID = "[Put Wifi ID]";
const char* WIFI_PASS = "[Put Wifi Password]";

// ---------- Servo settings ----------
const int      SERVO_PIN     = 14;
const int      SERVO_CHANNEL = 4;      // NOT 0 or 1: the camera clock (XCLK) uses LEDC channel 0 / timer 0
const uint32_t SERVO_FREQ    = 50;     // 50 Hz = standard hobby servo
const uint8_t  SERVO_RES     = 14;     // 14-bit duty
const int      SWEEP_MIN     = 45;     // degrees
const int      SWEEP_MAX     = 135;
const int      SWEEP_STEP    = 4;      // degrees per 20 ms
const uint32_t HOLD_MS       = 1000;   // keep sweeping this long after the last /servo call

volatile uint32_t lastFaceBeatMs = 0;

void servoWriteAngle(int angle) {
  int us = map(angle, 0, 180, 500, 2400);                       // pulse width in microseconds
  uint32_t duty = (uint32_t)(((uint64_t)us * ((1UL << SERVO_RES) - 1)) / 20000);
  ledcWriteChannel(SERVO_CHANNEL, duty);
}

void servoTask(void*) {
  int angle = 90;
  int dir = 1;
  bool wasActive = false;
  for (;;) {
    bool active = (lastFaceBeatMs != 0) && (millis() - lastFaceBeatMs < HOLD_MS);
    if (active) {
      angle += dir * SWEEP_STEP;
      if (angle >= SWEEP_MAX) { angle = SWEEP_MAX; dir = -1; }
      if (angle <= SWEEP_MIN) { angle = SWEEP_MIN; dir = 1; }
      servoWriteAngle(angle);
    } else if (wasActive) {
      ledcWriteChannel(SERVO_CHANNEL, 0);                       // stop pulses: servo idle, no buzzing
    }
    wasActive = active;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------- Camera web server (your code, unchanged) ----------
WebServer server(80);

static auto loRes  = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes  = esp32cam::Resolution::find(800, 600);

void serveJpg()
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi()
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid()
{
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

// NEW: "a face is in view right now" heartbeat from the PC
void handleServo()
{
  lastFaceBeatMs = millis();
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  // Servo AFTER the camera, so the camera has already claimed its own LEDC timer
  bool servoOk = ledcAttachChannel(SERVO_PIN, SERVO_FREQ, SERVO_RES, SERVO_CHANNEL);
  Serial.println(servoOk ? "SERVO OK (GPIO14, LEDC channel 4)" : "SERVO ATTACH FAIL");
  ledcWriteChannel(SERVO_CHANNEL, 0);                           // idle until a face shows up
  xTaskCreate(servoTask, "servo", 3072, nullptr, 1, nullptr);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /servo   (sweeps the servo for 1 s; open it in a browser to test)");

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/servo", handleServo);

  server.begin();
}

void loop()
{
  server.handleClient();
}
