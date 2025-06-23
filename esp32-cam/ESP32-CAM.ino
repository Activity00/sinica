#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "base64.h"
#include <ArduinoJson.h>

#define CAMERA_MODEL_AI_THINKER
#define RELAY_PIN 14  // 继电器使用的引脚
#define SOIL_SENSOR_AO 12 // 土壤湿度传感器 模拟信号
#define SOIL_SENSOR_DO 13 // 土壤湿度传感器 数字信号
#include "camera_pins.h"

// WiFi 配置
const char* ssid = "ChinaNet-PPX";
const char* password = "Bx123369";

// WebSocket 服务器配置
const char* ws_server = "192.168.2.209";
const uint16_t ws_port = 8989;
const char* ws_path = "/ws/esp32-cam/";  // 你的 WebSocket 路径
const char* device_id = "esp32-cam-001";

// 摄像头配置
camera_config_t config;
WebSocketsClient webSocket;

// 控制变量
bool streamingEnabled = false;
unsigned long lastStreamTime = 0;
const int streamInterval = 33;  // ms
const int CHUNK_SIZE = 1024;
bool cameraInitialized = false; // 摄像头是否已经初始化
unsigned long lastCameraUseTime = 0; 
const unsigned long CAMERA_IDLE_TIMEOUT_MS = 10 * 60 * 1000;  // 默认10分钟、十分钟摄像头没有使用就释放


bool watering = false;
unsigned long waterStartTime = 0;
unsigned long waterDuration = 0;  // 单位：毫秒


void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-CAM WebSocket Client...");

  // 初始化摄像头配置（同你原始代码）
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
  config.pixel_format = PIXFORMAT_JPEG;  // 1. 降低分辨率和调低jpeg质量，测试延时变化
  config.frame_size = FRAMESIZE_SVGA;  // 或更低 FRAMESIZE_CIF, FRAMESIZE_QVGA
  config.jpeg_quality = 12;  // 范围12~63，越大质量越低，编码越快
  config.fb_count = 2;
  
  // 转到需要的时候按需初始化
  // if (esp_camera_init(&config) != ESP_OK) {
  //   Serial.println("Camera init failed");
  //   return;
  // }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected with IP: ");
  Serial.println(WiFi.localIP());

  String full_path = String(ws_path) + "?device_id=" + device_id;
  // WebSocket 配置
  webSocket.begin(ws_server, ws_port, full_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(20000, 1000, 2);

  // 初始化继电器
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // 默认关闭

  pinMode(SOIL_SENSOR_DO, INPUT);

  Serial.println("GPIO init done!");
}

void loop() {
  webSocket.loop();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi reconnected");
  }

  if (streamingEnabled && (millis() - lastStreamTime > streamInterval)) {
    captureAndSendFrame();
    lastStreamTime = millis();
  } 

    // 非阻塞方式自动停止浇水
  if (watering && millis() - waterStartTime >= waterDuration) {
    digitalWrite(RELAY_PIN, LOW);
    watering = false;
    Serial.println("Watering finished (non-blocking)");
  }

  // 摄像头自动释放逻辑
  deinitCameraIfIdle();

  delay(10);
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


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      streamingEnabled = false;
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      WiFi.setSleep(true);
      break;

    case WStype_TEXT:
      Serial.printf("Received text: %.*s\n", length, payload);
      // 这里你可以根据收到的命令控制摄像头，比如start/stop
      handleCommand((char*)payload, length);
      break;

    case WStype_ERROR:
      Serial.println("WebSocket error");
      break;

    default:
      break;
  }
}

void handleCommand(char* payload, size_t length) {
  // 先用字符串构造 JSON 文档
  StaticJsonDocument<200> doc;  // 够用即可，根据实际调整大小
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print(F("JSON parse failed: "));
    Serial.println(error.f_str());
    return;
  }

  // 取 action 字段
  const char* action = doc["action"];
  if (!action) {
    Serial.println("No action field found");
    return;
  }

  if (strcmp(action, "start_stream") == 0) {
    streamingEnabled = true;
    Serial.println("Streaming started");
  } 
  else if (strcmp(action, "stop_stream") == 0) {
    streamingEnabled = false;
    Serial.println("Streaming stopped");
  } 
  else if (strcmp(action, "take_photo") == 0) {
    captureLatestPhoto();
  } 
  else if (strcmp(action, "start_water") == 0) {
    int duration = 10;  // 默认10秒
    if (doc.containsKey("ts")) {
      int requested = doc["ts"];
      if (requested > 0 && requested <= 60) {
        duration = requested;
      } 
    }
   startWater(duration);
  }
  else if (strcmp(action, "soil_data") == 0) {
    readAndSendSoilData();
  }
  else {
    Serial.printf("Unknown action: %s\n", action);
  }
}

void startWater(int durationSeconds) {
  Serial.printf("Start watering for %d seconds (non-blocking)\n", durationSeconds);
  digitalWrite(RELAY_PIN, HIGH);
  watering = true;
  waterStartTime = millis();
  waterDuration = durationSeconds * 1000;
}

// 视频流用，连续调用
void captureAndSendFrame() {
  if (!initCameraIfNeeded()) return;

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.printf("Stream frame size: %zu bytes\n", fb->len);
  webSocket.sendBIN(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// 拍照用，丢弃旧帧确保最新
void captureLatestPhoto() {
  if (!initCameraIfNeeded()) return;

  // 丢弃2~3帧，提高刷新率
  for (int i=0; i<3; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    } else {
      Serial.println("Camera capture failed when discarding old frame");
      return;
    }
  }

  // 抓最新帧
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed when getting latest frame");
    return;
  }

  Serial.printf("Photo size: %zu bytes\n", fb->len);
  webSocket.sendBIN(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// 读取土壤数据并发送JSON格式（WebSocket + 串口）
void readAndSendSoilData() {
  // 1. 读取传感器数据
  int analogValue = analogRead(SOIL_SENSOR_AO);
  bool isDry = digitalRead(SOIL_SENSOR_DO);

  // 2. 构建JSON对象
  StaticJsonDocument<200> doc;  // 根据实际需求调整大小
  doc["device_id"] = device_id;  // 设备标识
  doc["type"] = "soil";
  
  JsonObject soil = doc.createNestedObject("data");
  soil["analog"] = analogValue;   // 模拟值
  soil["digital"] = isDry;        // 数字值（true=DRY, false=WET）
  

  // 3. 序列化为JSON字符串
  char jsonStr[200];
  serializeJson(doc, jsonStr);

  // 4. 串口打印（美化格式，调试用）
  Serial.println("=== Soil Data ===");
  serializeJsonPretty(doc, Serial);
  Serial.println("\n================");

  // 5. 通过WebSocket发送
  if (webSocket.isConnected()) {
    webSocket.sendTXT(jsonStr);  // 发送纯JSON
  } else {
    Serial.println("WebSocket not connected!");
  }
}

