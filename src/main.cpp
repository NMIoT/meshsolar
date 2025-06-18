#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"

// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}

static void printMeshsolarCmd(const meshsolar_cmd_t* cmd) {
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

static bool parseJsonCommand(const char* json, meshsolar_cmd_t* cmd) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.print("Parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    // clear the command structure
    memset(cmd, 0, sizeof(meshsolar_cmd_t));

    if (!doc.containsKey("command")) {
        Serial.println("Missing 'command' field");
        return false;
    }
    strlcpy(cmd->command, doc["command"] | "", sizeof(cmd->command));

    if (strcmp(cmd->command, "config") == 0) {
        if (!doc.containsKey("battery") || !doc.containsKey("temperature_protection")) {
            Serial.println("Missing 'battery' or 'temperature_protection' field for 'config' command");
            return false;
        }
        JsonObject battery = doc["battery"];
        JsonObject tp = doc["temperature_protection"];
        if (!battery.containsKey("type") ||
            !battery.containsKey("cell_number") ||
            !battery.containsKey("design_capacity") ||
            !battery.containsKey("cutoff_voltage") ||
            !tp.containsKey("high_temp_c") ||
            !tp.containsKey("high_temp_enabled") ||
            !tp.containsKey("low_temp_c") ||
            !tp.containsKey("low_temp_enabled")) {
            Serial.println("Missing fields in 'battery' or 'temperature_protection'");
            return false;
        }
        strlcpy(cmd->battery.type, battery["type"] | "", sizeof(cmd->battery.type));
        cmd->battery.cell_number = battery["cell_number"] | 0;
        cmd->battery.design_capacity = battery["design_capacity"] | 0;
        cmd->battery.cutoff_voltage = battery["cutoff_voltage"] | 0;

        cmd->temperature_protection.high_temp_c = tp["high_temp_c"] | 0;
        cmd->temperature_protection.high_temp_enabled = tp["high_temp_enabled"] | false;
        cmd->temperature_protection.low_temp_c = tp["low_temp_c"] | 0;
        cmd->temperature_protection.low_temp_enabled = tp["low_temp_enabled"] | false;
    } else if (strcmp(cmd->command, "switch") == 0) {
        if (!doc.containsKey("fet_en")) {
            Serial.println("Missing 'fet_en' field for 'switch' command");
            return false;
        }
        cmd->fet_en = doc["fet_en"] | false;
    } else if (strcmp(cmd->command, "reset") == 0) {

    } else {
        Serial.println("Unknown command");
        return false;
    }

    return true;
}

static bool listenString(String& input, char terminator = '\n') {
    while (true) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            if (c == terminator) {
                return true; // End of input
            } else {
                input += c; // Append character to input
            }
        }
    }
    return false; // Should never reach here
}

void setup() {
  Serial.begin(115200); 
  time_t timeout = millis();
  while (!Serial){
    if ((millis() - timeout) < 10000){
      delay(100);
    }
  }
  Serial.println("================================");
  Serial.println("     MeshTower BQ4050 example   ");
  Serial.println("================================");

  Serial.begin(115200);
  Serial.println("Please input JSON string and end with newline:");
}




void loop() {
    String input = "";
    meshsolar_cmd_t cmd;

    input = ""; // Reset input for new command
    if(listenString(input, ' ')) {
        bool res = parseJsonCommand(input.c_str(), &cmd);
        if (res) {
            printMeshsolarCmd(&cmd);
            /*  add some func call back here base on cmd sector */
            
        } else {
            Serial.println("Failed to parse command");
        }
    }



}