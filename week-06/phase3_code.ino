#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "soc/soc.h"           // for disabling the brownout reset
#include "soc/rtc_cntl_reg.h"  // for disabling the brownout reset

// 1. Enter your 2.4GHz Wi-Fi credentials (or mobile hotspot)
const char* WIFI_SSID = "[PUT WIFI ID]";
const char* WIFI_PASS = "[PUT PASSWORD]";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
int stream_port_number; // Stream server port number

// =====================================================================
// PHASE 3: Pins
// Servo signal : GPIO 14
// Push button  : GPIO 13 <-> GND (internal pull-up, no resistor needed)
// =====================================================================
const int servo_pin  = 14;
const int button_pin = 13;

// PWM values for servo's min and max positions (50Hz, 16-bit)
// Default range: 3700 (~1.13ms) to 7000 (~2.14ms)
// Narrower range = the servo never strains against its end stops,
// which lowers the current surge. (Phase 2 Step 3 range)
const int servo_min_position = 3700;
const int servo_max_position = 5800;

// Default range (can strain some servos at the ends):
// const int servo_min_position = 3700;
// const int servo_max_position = 7000;

const int servo_speed = 50;             // PWM units per step (one step every 20 ms) - slower = gentler on power
const unsigned long servo_step_ms = 20;
const unsigned long debounce_ms   = 30;

int servo_position = servo_min_position;
bool servo_direction = true; // true = forward, false = backward

// stream_handler: handles live video streaming ONLY (servo is no longer here)
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
  static int64_t last_frame = 0;

  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 400) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) {
      break;
    }
  }

  last_frame = 0;
  return res;
}

// web_handler: builds the web page
static esp_err_t web_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");

  char webPage[200] = "";
  strcat(webPage, "<!doctype html> <html> <head> <title id='title'>ESP32-CAM</title> </head> <body> <img id='stream' src='http://");

  // Stream server address (example: 192.168.0.145:81)
  char address[30] = "";
  sprintf(address, "%d.%d.%d.%d:%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3], stream_port_number);

  strcat(webPage, address);
  strcat(webPage, "/stream'> </body> </html>");

  int pageLength = strlen(webPage);
  return httpd_resp_send(req, (const char *)webPage, pageLength);
}

// startCameraServer: starts the web and stream servers
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = web_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    stream_port_number = config.server_port;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("====");

  // Workaround: stop the ESP32 from resetting itself when the 5V dips
  // slightly while the servo starts moving. (Hides the symptom; the real
  // fix is still better power, see notes.)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // PHASE 3: Push button with internal pull-up (pressed = LOW)
  pinMode(button_pin, INPUT_PULLUP);

  // PHASE 1 (STEP 4): Pin configuration for AI Thinker ESP32-CAM
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = 5;
  config.pin_d1       = 18;
  config.pin_d2       = 19;
  config.pin_d3       = 21;
  config.pin_d4       = 36;
  config.pin_d5       = 39;
  config.pin_d6       = 34;
  config.pin_d7       = 35;
  config.pin_xclk     = 0;
  config.pin_pclk     = 22;
  config.pin_vsync    = 25;
  config.pin_href     = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn     = 32;
  config.pin_reset    = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count     = 2;

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Servo PWM initialization (GPIO 14, 50Hz, 16-bit resolution, Channel 2)
  ledcAttachChannel(servo_pin, 50, 16, 2);

  Serial.println("");
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  delay(100);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println("");
  delay(100);
  startCameraServer();
  Serial.print("Camera ready. Access it at: http://");
  Serial.print(WiFi.localIP());
  Serial.println("");

  // Move the servo to its start position ONCE, after everything else is
  // stable. This avoids a big current jump on the very first button press.
  ledcWrite(servo_pin, servo_position);
}

// PHASE 3: loop() reads the button and pans the servo only while it is held.
// It runs independently of the video stream, so streaming is never blocked.
void loop() {
  static bool last_reading   = false;
  static bool stable_pressed = false;
  static unsigned long last_change = 0;
  static unsigned long last_step   = 0;

  // Debounced button read (pressed = LOW)
  bool reading = (digitalRead(button_pin) == LOW);
  if (reading != last_reading) {
    last_reading = reading;
    last_change = millis();
  }
  if (millis() - last_change >= debounce_ms) {
    stable_pressed = reading;
  }

  // Pan only while pressed. On release we simply stop updating, so the
  // servo holds its last position.
  if (stable_pressed && (millis() - last_step >= servo_step_ms)) {
    last_step = millis();

    if (servo_direction) {
      servo_position += servo_speed;
      if (servo_position >= servo_max_position) {
        servo_position = servo_max_position;
        servo_direction = false;
      }
    } else {
      servo_position -= servo_speed;
      if (servo_position <= servo_min_position) {
        servo_position = servo_min_position;
        servo_direction = true;
      }
    }
    ledcWrite(servo_pin, servo_position);
  }

  delay(1);
}
