#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"


const char *json_config = 
"{"
"  \"command\": \"config\","
"  \"battery\": {"
"    \"type\": \"LiFePO4\","
"    \"cell_number\": 4,"
"    \"design_capacity\": 3200,"
"    \"cutoff_voltage\": 2800"
"  },"
"  \"temperature_protection\": {"
"    \"high_temp_c\": 60,"
"    \"high_temp_enabled\": true,"
"    \"low_temp_c\": -10,"
"    \"low_temp_enabled\": true"
"  }"
"}";

const char *json_reset = 
"{"
"  \"command\": \"reset\""
"}";

const char *json_switch = 
"{"
"  \"command\": \"switch\","
"  \"fet_en\": true"
"}";





meshsolar_cmd_t cmd;


void printMeshsolarCmd(const meshsolar_cmd_t* cmd) {
    Serial.print("Command: ");
    Serial.println(cmd->command);

    if (strcmp(cmd->command, "config") == 0) {
        Serial.println("Battery Config:");
        Serial.print("  Type: "); Serial.println(cmd->battery.type);
        Serial.print("  Cell Number: "); Serial.println(cmd->battery.cell_number);
        Serial.print("  Design Capacity: "); Serial.println(cmd->battery.design_capacity);
        Serial.print("  Cutoff Voltage: "); Serial.println(cmd->battery.cutoff_voltage);

        Serial.println("Temperature Protection:");
        Serial.print("  High Temp (C): "); Serial.println(cmd->temperature_protection.high_temp_c);
        Serial.print("  High Temp Enabled: "); Serial.println(cmd->temperature_protection.high_temp_enabled ? "true" : "false");
        Serial.print("  Low Temp (C): "); Serial.println(cmd->temperature_protection.low_temp_c);
        Serial.print("  Low Temp Enabled: "); Serial.println(cmd->temperature_protection.low_temp_enabled ? "true" : "false");
    }
    if (strcmp(cmd->command, "switch") == 0) {
        Serial.print("FET Switch: ");
        Serial.println(cmd->fet_en ? "ON" : "OFF");
    }
}



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

  Serial.begin(115200);
  Serial.println("Please input JSON string and end with newline:");
}

void loop() {
    static String input;
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == ' ') {
            // prase JSON
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                Serial.print("Parse failed: ");
                Serial.println(error.c_str());
                input = "";
                return;
            }

            // clear the command structure
            memset(&cmd, 0, sizeof(cmd));

            // prase command
            strlcpy(cmd.command, doc["command"] | "", sizeof(cmd.command));

            // prase battery
            if (doc.containsKey("battery")) {
                JsonObject battery = doc["battery"];
                strlcpy(cmd.battery.type, battery["type"] | "", sizeof(cmd.battery.type));
                cmd.battery.cell_number = battery["cell_number"] | 0;
                cmd.battery.design_capacity = battery["design_capacity"] | 0;
                cmd.battery.cutoff_voltage = battery["cutoff_voltage"] | 0;
            }

            // prase temperature_protection
            if (doc.containsKey("temperature_protection")) {
                JsonObject tp = doc["temperature_protection"];
                cmd.temperature_protection.high_temp_c = tp["high_temp_c"] | 0;
                cmd.temperature_protection.high_temp_enabled = tp["high_temp_enabled"] | false;
                cmd.temperature_protection.low_temp_c = tp["low_temp_c"] | 0;
                cmd.temperature_protection.low_temp_enabled = tp["low_temp_enabled"] | false;
            }

            // prase fet_en
            if (doc.containsKey("fet_en")) {
                cmd.fet_en = doc["fet_en"] | false;
            }

            // print the command structure
            printMeshsolarCmd(&cmd);

            input = "";
        } else {
            input += c;
        }
    }
}