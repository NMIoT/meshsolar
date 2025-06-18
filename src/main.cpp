#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>

void setup() {
  // delay(1000); 
  Serial.begin(115200); 
  time_t timeout = millis();
  // Waiting for Serial
  while (!Serial){
    if ((millis() - timeout) < 10000){
      delay(100);
    }
  }
  Serial.println("================================");
  Serial.println("MeshTower BQ4050 example");
  Serial.println("================================");
}



const char* json = "{\"temp\":23.5,\"cap\":\"86%\"}";

void loop() {
    while (Serial.available() > 0) {
      char c = Serial.read();
      Serial.write(c);
    }

    // 创建解析缓冲区
    StaticJsonDocument<1024> doc;

    // 解析 JSON
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      Serial.print("解析失败: ");
      Serial.println(error.c_str());
      return;
    }

    // 读取字段
    float temp = doc["temp"];
    const char* cap = doc["cap"];

    Serial.print("温度: ");
    Serial.println(temp);
    Serial.print("电量: ");
    Serial.println(cap);

    delay(1000); // 延时1秒

}

