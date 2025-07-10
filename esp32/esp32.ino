#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <base64.hpp>
#include <UrlEncode.h>
#include <driver/gpio.h>

// WiFi 配置
const char* ssid = "HONOR 70";
const char* password = "Noeight12345678";

// 百度 AppBuilder 配置
const char* appbuilder_api_key = "bce-v3/ALTAK-kBdrgMDpaP7wcCNJ5lDWB/844ddcca48c20ff3a7ccbfd7434ff9285cc509e6";
const char* appbuilder_app_id = "0eee79f8-a09e-41d6-a3e8-962fe2f8b41c";

// 百度语音服务配置
const String BAIDU_API_KEY = "iiFtufT5V3bb28c8EHeJaGaD";
const String BAIDU_SECRET_KEY = "n4vZWrFz34SALbGaImWE9wRrtvKjAXh8";
const String ASR_URL = "https://vop.baidu.com/server_api";
const String TTS_URL = "https://tsn.baidu.com/text2audio";

// I2S 配置
#define I2S_IN_PORT I2S_NUM_0
#define I2S_OUT_PORT I2S_NUM_1
#define I2S_IN_WS 22
#define I2S_IN_SCK 21
#define I2S_IN_SD 20
#define I2S_OUT_BCLK 14
#define I2S_OUT_LRC 12
#define I2S_OUT_DOUT 13

// PWM配置
const int pwmPin = 2;
const int pwmFrequency = 5000;
const int pwmResolution = 8;

// 超声波传感器配置
const int TrigPin = 32;  // 使用GPIO32作为Trig引脚
const int EchoPin = 33;  // 使用GPIO33作为Echo引脚
const float ALARM_DISTANCE = 15.0; // 报警距离阈值(厘米)
const unsigned long ALARM_DURATION = 2000; // 报警持续时间阈值(毫秒)

// 蜂鸣器报警配置
const int BuzzerPin = 25; // 蜂鸣器控制引脚

// 全局变量
String appbuilder_conversation_id = "";
bool isWiFiConnected = false;
bool alarmActive = false;
unsigned long alarmStartTime = 0;

// 串口2解析状态
enum ParseState { WAIT_HEADER, WAIT_CMD, WAIT_DATA, WAIT_FOOTER };
ParseState parseState = WAIT_HEADER;
uint8_t currentCommand = 0;
uint8_t currentData = 0;

// ================== 超声波传感器功能 ================== //

void setupUltrasonic() {
  pinMode(TrigPin, OUTPUT);
  pinMode(EchoPin, INPUT);
  pinMode(BuzzerPin, OUTPUT);
  digitalWrite(BuzzerPin, HIGH); // 初始关闭蜂鸣器
}

float measureDistance() {
  // 发送触发信号
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);

  // 测量回波时间
  float duration = pulseIn(EchoPin, HIGH);
  float distance = duration / 58.0;  // 转换为厘米
  distance = (int(distance * 100.0)) / 100.0; // 保留两位小数
  
  return distance;
}

void checkDistanceAlarm() {
  static unsigned long lastMeasurementTime = 0;
  static bool belowThreshold = false;
  static unsigned long belowThresholdStart = 0;
  
  // 每200ms测量一次距离
  if (millis() - lastMeasurementTime >= 200) {
    lastMeasurementTime = millis();
    
    float distance = measureDistance();
    Serial.print("距离: ");
    Serial.print(distance);
    Serial.println("cm");
    
    // 检查距离是否低于阈值
    if (distance < ALARM_DISTANCE) {
      if (!belowThreshold) {
        // 首次低于阈值，记录开始时间
        belowThreshold = true;
        belowThresholdStart = millis();
      } else {
        // 持续低于阈值，检查持续时间
        if (millis() - belowThresholdStart >= ALARM_DURATION && !alarmActive) {
          // 触发报警
          alarmActive = true;
          alarmStartTime = millis();
          digitalWrite(BuzzerPin, LOW); // 打开蜂鸣器
          Serial.println("报警触发！");
          
          // 发送报警信息到显示屏
          Serial2.print("ai.t1.txt=\"警告！坐姿不对：");
          Serial2.print(distance);
          Serial2.print("cm\"\xff\xff\xff");
        }
      }
    } else {
      // 距离高于阈值，重置状态
      belowThreshold = false;
      
      // 如果报警已激活，检查是否结束
      if (alarmActive) {
        digitalWrite(BuzzerPin, HIGH); // 关闭蜂鸣器
        alarmActive = false;
        Serial.println("报警解除");
      }
    }
  }
}

// ================== 语音功能模块 ================== //

void setupI2S() {
  // 输入配置
  i2s_config_t i2s_config_in = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
  };
  i2s_pin_config_t pin_config_in = {
    .bck_io_num = I2S_IN_SCK,
    .ws_io_num = I2S_IN_WS,
    .data_out_num = -1,
    .data_in_num = I2S_IN_SD
  };

  // 输出配置
  i2s_config_t i2s_config_out = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
  };
  i2s_pin_config_t pin_config_out = {
    .bck_io_num = I2S_OUT_BCLK,
    .ws_io_num = I2S_OUT_LRC,
    .data_out_num = I2S_OUT_DOUT,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_IN_PORT, &i2s_config_in, 0, NULL);
  i2s_driver_install(I2S_OUT_PORT, &i2s_config_out, 0, NULL);
  i2s_set_pin(I2S_IN_PORT, &pin_config_in);
  i2s_set_pin(I2S_OUT_PORT, &pin_config_out);
}

String getBaiduAccessToken() {
  HTTPClient http;
  String url = "https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=" + 
               BAIDU_API_KEY + "&client_secret=" + BAIDU_SECRET_KEY;

  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("获取Token失败: %d\n", httpCode);
    http.end();
    return "";
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);
  return doc["access_token"].as<String>();
}

String recognizeSpeech() {
  String accessToken = getBaiduAccessToken();
  if (accessToken == "") return "";

  HTTPClient http;
  String url = ASR_URL + "?cuid=123456&token=" + accessToken;

  // 读取音频数据
  int16_t audioBuffer[1024];
  size_t bytes_read = 0;
  i2s_read(I2S_IN_PORT, audioBuffer, sizeof(audioBuffer), &bytes_read, portMAX_DELAY);

  if (bytes_read == 0) {
    Serial.println("无音频数据");
    return "";
  }
  
  http.begin(url);
  http.addHeader("Content-Type", "audio/pcm;rate=16000");
  http.addHeader("Content-Length", String(bytes_read));

  int httpCode = http.POST((uint8_t*)audioBuffer, bytes_read);
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("语音识别失败: %d\n", httpCode);
    http.end();
    return "";
  }

  String response = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  return doc["result"][0].as<String>();
}

void synthesizeSpeech(String text) {
  if (text == "") return;

  String accessToken = getBaiduAccessToken();
  if (accessToken == "") return;

  HTTPClient http;
  String url = TTS_URL + "?tex=" + urlEncode(text) + "&lan=zh&cuid=123456&ctp=1&tok=" + accessToken;

  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("语音合成失败: %d\n", httpCode);
    http.end();
    return;
  }

  String audioDataBase64 = http.getString();
  http.end();

  unsigned int decodedSize = decode_base64_length(
    (unsigned char*)audioDataBase64.c_str(), 
    audioDataBase64.length()
  );

  if (decodedSize > 4096) {
    Serial.println("音频数据过大");
    return;
  }

  uint8_t audioBuffer[4096];
  unsigned int audioDataSize = decode_base64(
    (unsigned char*)audioDataBase64.c_str(),
    audioDataBase64.length(),
    audioBuffer
  );

  if (audioDataSize == 0) {
    Serial.println("Base64解码失败");
    return;
  }

  size_t bytesWritten;
  i2s_write(I2S_OUT_PORT, audioBuffer, audioDataSize, &bytesWritten, portMAX_DELAY);
}

// ================== 主程序功能 ================== //

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("连接WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n已连接，IP: " + WiFi.localIP());
  isWiFiConnected = true;
}

String getConversation_id() {
  HTTPClient http;
  http.begin("https://qianfan.baidubce.com/v2/app/conversation");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Appbuilder-Authorization", "Bearer " + String(appbuilder_api_key));

  DynamicJsonDocument requestJson(1024);
  requestJson["app_id"] = appbuilder_app_id;
  
  String requestBody;
  serializeJson(requestJson, requestBody);
  
  int httpCode = http.POST(requestBody);
  String conversation_id = "";
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    conversation_id = doc["conversation_id"].as<String>();
    Serial.println("会话ID: " + conversation_id);
  } else {
    Serial.println("获取会话ID错误: " + http.errorToString(httpCode));
  }
  
  http.end();
  return conversation_id;
}

String Conversation_Get(String query) {
  HTTPClient http;
  http.begin("https://qianfan.baidubce.com/v2/app/conversation/runs");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Appbuilder-Authorization", "Bearer " + String(appbuilder_api_key));

  DynamicJsonDocument requestJson(1024);
  requestJson["app_id"] = appbuilder_app_id;
  requestJson["query"] = query;
  requestJson["conversation_id"] = appbuilder_conversation_id;
  requestJson["stream"] = false;
  
  String requestBody;
  serializeJson(requestJson, requestBody);
  
  int httpCode = http.POST(requestBody);
  String response_text = "";
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, response);
    response_text = doc["answer"].as<String>();
  } else {
    response_text = "错误: " + String(http.errorToString(httpCode));
  }
  
  http.end();
  return response_text;
}

void processIncomingCommand(uint8_t command, uint8_t data) {
  switch (command) {
    case 0x01: // 桌子高度控制
      Serial.print("桌子高度控制: ");
      Serial.print(data);
      Serial.println("%");
      break;
      
    case 0x02: // 灯光亮度控制
      Serial.print("灯光亮度控制: ");
      Serial.print(data);
      Serial.println("%");
      
      int pwmValue = map(data, 0, 100, 0, 255);
      ledcWrite(pwmPin, pwmValue);
      
      // 发送确认回执
      uint8_t ack[4] = {0x05, command, data, 0x06};
      Serial2.write(ack, 4);
      break;
  }
}

void processLightingCommand(String response) {
  if (response.indexOf("02 02") == 0 && response.endsWith("02")) {
    int dataStart = response.indexOf(' ', 5) + 1;
    int dataEnd = response.lastIndexOf(' ');
    String hexValue = response.substring(dataStart, dataEnd);
    
    char* endptr;
    long brightness = strtol(hexValue.c_str(), &endptr, 16);
    
    if (*endptr == '\0' && brightness >= 0 && brightness <= 100) {
      int pwmValue = map(brightness, 0, 100, 0, 255);
      ledcWrite(pwmPin, pwmValue);
      
      uint8_t cmd[4] = {0x03, 0x02, (uint8_t)brightness, 0x04};
      Serial2.write(cmd, 4);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  // PWM初始化
  ledcAttach(pwmPin, pwmFrequency, pwmResolution);
  ledcWrite(pwmPin, 0);
  
  // 语音I2S初始化
  setupI2S();
  
  // 超声波传感器初始化
  setupUltrasonic();
  
  // 网络连接
  connectToWiFi();
  appbuilder_conversation_id = getConversation_id();
  
  Serial.println("系统就绪");
}

void loop() {
  static unsigned long lastRecognitionTime = 0;
  
  // 处理串口1输入
  if (Serial.available()) {
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();
    
    if (userInput.length() > 0) {
      String response = Conversation_Get(userInput);
      
      Serial2.print("ai.t1.txt=\"");
      Serial2.print(response);
      Serial2.print("\"\xff\xff\xff");
      
      processLightingCommand(response);
      synthesizeSpeech(response);
    }
  }
  
  // 处理串口2输入
  while (Serial2.available() > 0) {
    uint8_t incomingByte = Serial2.read();
    
    switch (parseState) {
      case WAIT_HEADER:
        if (incomingByte == 0x03) parseState = WAIT_CMD;
        break;
      case WAIT_CMD:
        if (incomingByte == 0x01 || incomingByte == 0x02) {
          currentCommand = incomingByte;
          parseState = WAIT_DATA;
        } else {
          parseState = WAIT_HEADER;
        }
        break;
      case WAIT_DATA:
        currentData = incomingByte;
        parseState = WAIT_FOOTER;
        break;
      case WAIT_FOOTER:
        if (incomingByte == 0x04) {
          processIncomingCommand(currentCommand, currentData);
        }
        parseState = WAIT_HEADER;
        break;
    }
  }
  
  // 每10秒语音识别
  if (millis() - lastRecognitionTime >= 10000) {
    lastRecognitionTime = millis();
    
    Serial.println("开始语音识别...");
    String text = recognizeSpeech();
    
    if (text != "") {
      Serial.println("识别结果: " + text);
      String response = Conversation_Get(text);
      
      Serial2.print("ai.t1.txt=\"");
      Serial2.print(response);
      Serial2.print("\"\xff\xff\xff");
      
      processLightingCommand(response);
      synthesizeSpeech(response);
    }
  }
  
  // 检查距离报警
  checkDistanceAlarm();
  
  delay(10);
}