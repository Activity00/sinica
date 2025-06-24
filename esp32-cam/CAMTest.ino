#include "esp_camera.h"
#include <WiFi.h>
#include "base64.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define CAMERA_MODEL_AI_THINKER
#define RELAY_PIN 14  // 继电器使用的引脚
#include "camera_pins.h"

// WiFi 配置
const char* ssid = "ChinaNet-PPX";
const char* password = "Bx123369";

const char* command_url = "http://127.0.0.1:8989/api/get_commands/";
const char* report_url  = "http://127.0.0.1:8989/api/report_result/";
const char* device_id   = "esp32-cam-001";  // 用于标识设备

// 摄像头配置
camera_config_t config;

// 控制变量
const int CHUNK_SIZE = 1024;
bool cameraInitialized = false;
unsigned long lastCameraUseTime = 0;
const unsigned long CAMERA_IDLE_TIMEOUT_MS = 10 * 60 * 1000;  // 10分钟摄像头空闲超时

// 浇水控制
bool watering = false;
unsigned long waterStartTime = 0;
unsigned long waterDuration = 0;

// 动态轮询控制
unsigned long lastCommandCheck = 0;
unsigned long commandInterval = 60 * 1000;  // 默认1分钟
unsigned long lastCommandReceivedTime = 0;
const unsigned long ACTIVE_MODE_DURATION = 5 * 60 * 1000;  // 5分钟活跃模式
const unsigned long NORMAL_INTERVAL = 10 * 1000;  // 正常模式1分钟
const unsigned long ACTIVE_INTERVAL = 10 * 1000;   // 活跃模式10秒

// 全局 WiFiClientSecure 对象
WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-CAM WebSocket Client...");

  // 初始化摄像头配置
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // 初始化 WiFiClientSecure
  client.setInsecure(); // 跳过证书验证（仅用于测试，生产环境应使用正确证书）

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected with IP: ");
  Serial.println(WiFi.localIP());

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("GPIO init done!");
}

void loop() {
  handleWiFiConnection();
  handleWatering();
  updatePollingInterval();
  checkCommandsIfNeeded();
  deinitCameraIfIdle();
  delay(10);
}

void handleWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi reconnected");
  }
}

void handleWatering() {
  if (watering && millis() - waterStartTime >= waterDuration) {
    digitalWrite(RELAY_PIN, LOW);
    watering = false;
    reportResult("water_finished");
    Serial.println("Watering finished (non-blocking)");
  }
}

void updatePollingInterval() {
  // 检查是否应该退出活跃模式
  if (lastCommandReceivedTime > 0 && millis() - lastCommandReceivedTime > ACTIVE_MODE_DURATION) {
    lastCommandReceivedTime = 0;
    commandInterval = NORMAL_INTERVAL;
    Serial.println("Returning to normal polling mode (1 minute)");
  }
}

void checkCommandsIfNeeded() {
  if (millis() - lastCommandCheck >= commandInterval) {
    checkCommands();
    lastCommandCheck = millis();
  }
}

bool initCameraIfNeeded() {
  if (cameraInitialized) {
    lastCameraUseTime = millis();
    return true;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return false;
  }

  cameraInitialized = true;
  lastCameraUseTime = millis();
  Serial.println("Camera initialized");
  return true;
}

void deinitCameraIfIdle() {
  if (!cameraInitialized) return;

  if (millis() - lastCameraUseTime > CAMERA_IDLE_TIMEOUT_MS) {
    esp_camera_deinit();
    cameraInitialized = false;
    Serial.println("Camera deinitialized after timeout");
  }
}

void startWater(int durationSeconds = 10) {
  Serial.printf("Start watering for %d seconds (non-blocking)\n", durationSeconds);
  digitalWrite(RELAY_PIN, HIGH);
  watering = true;
  waterStartTime = millis();
  waterDuration = durationSeconds * 1000;
  reportResult("water_started");
}

void takeAndReportPhoto() {
  if (!initCameraIfNeeded()) {
    reportResult("photo_failed");
    return;
  }

  // 丢弃旧帧确保最新
  for (int i = 0; i < 2; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    reportResult("photo_failed");
    return;
  }

  String photoBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, report_url);  // 使用全局client对象
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(2048);
    doc["device_id"] = device_id;
    doc["type"] = "photo";
    doc["result"] = "ok";
    doc["data"] = photoBase64;

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    Serial.printf("[Photo Report] HTTP POST code: %d\n", code);
    if (code <= 0) {
      Serial.printf("[Photo Report] Error: %s\n", http.errorToString(code).c_str());
    }
    http.end();
  }
}

void reportResult(String resultType) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, report_url);  // 使用全局client对象
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);
    doc["device_id"] = device_id;
    doc["type"] = resultType;
    doc["result"] = "ok";

    String json;
    serializeJson(doc, json);
    int code = http.POST(json);
    Serial.printf("[Result Report] POST code: %d\n", code);
    if (code <= 0) {
      Serial.printf("[Result Report] Error: %s\n", http.errorToString(code).c_str());
    }
    http.end();
  }
}

void checkCommands() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, skip command check.");
    return;
  }

  HTTPClient http;
  http.begin(client, command_url);  // 使用全局client对象
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    JsonArray cmds = doc["commands"];

    bool receivedCommand = false;

    for (JsonVariant cmd : cmds) {
      String c = cmd.as<String>();
      Serial.println("Received Command: " + c);

      if (c == "photo") {
          takeAndReportPhoto();
          receivedCommand = true;
      } else if (c == "water") {
          if (!watering) {
              startWater();
              receivedCommand = true;
          } else {
              Serial.println("Currently watering, ignoring command: " + c);
          }
      }
    }

    if (receivedCommand) {
      // 进入活跃模式
      lastCommandReceivedTime = millis();
      commandInterval = ACTIVE_INTERVAL;
      Serial.println("Entering active polling mode (10s)");
    }

  } else {
    Serial.printf("Command GET failed, code: %d\n", httpCode);
    if (httpCode <= 0) {
      Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
    }
  }

  http.end();
}