#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include <Wire.h>

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
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == terminator) {
            return true; // End of input
        } else {
            input += c; // Append character to input
        }
    }
    return false; // Should never reach here
}

#define SDA_PIN 33
#define SCL_PIN 32

#define BQ4050addr 0x0B //0x0B

#define BLOCK_CMD (0x44)

#define BQ4050_REG_CAPACITY_ALARM 0x01 // Remaining Capacity Alarm
#define BQ4050_REG_TIME_ALARM 0x02     // Remaining Time Alarm
#define BQ4050_REG_BAT_MODE 0x03       // Battery Mode
#define BQ4050_REG_TEMP 0x08
#define BQ4050_REG_VOLT 0x09
#define BQ4050_REG_CURRENT 0x0A
#define BQ4050_REG_AVG_CURRENT 0x0B
#define BQ4050_REG_RSOC 0x0D // Relative State of Charge
#define BQ4050_REG_ASOC 0x0E // Absolute State of Charge
#define BQ4050_REG_RC 0x0F   // predicted remaining battery capacity
#define BQ4050_REG_FCC 0x10  // Full Charge Capacity
#define BQ4050_REG_ATTE 0x12 // Average Time To Empty
#define BQ4050_REG_ATTF 0x13 // Average Time To Full
#define BQ4050_REG_RMC 0x0F  // Remaining Capacity
#define BQ4050_REG_MAC 0x44

/* ManufacturerAccess */
#define PCHG_FET_Toggle 0x1E
#define CHG_FET_Toggle 0x1F
#define DSG_FET_Toggle 0x20
#define FETcontrol 0x22

#define MAC_CMD_FW_VER 0x0002
#define MAC_CMD_SECURITY_KEYS 0x0035


byte crctable[256];
boolean printResults;

void CalculateTable_CRC8(){
  // Function that generates byte array as a lookup table to quickly create a CRC8 for the PEC
  const byte generator = 0x07;
  /* iterate over all byte values 0 - 255 */
  for (int divident = 0; divident < 256; divident++)
  {
    byte currByte = (byte)divident;
    /* calculate the CRC-8 value for current byte */
    for (byte bit = 0; bit < 8; bit++)
    {
      if ((currByte & 0x80) != 0)
      {
        currByte <<= 1;
        currByte ^= generator;
      }
      else
      {
        currByte <<= 1;
      }
    }
    /* store CRC value in lookup table */
    crctable[divident] = currByte;
    if (printResults)
    {
      if (divident % 16 == 0 && divident > 2)
      {
        Serial.println();
      }
      if (currByte < 16)
        Serial.print("0");
      Serial.print(currByte, HEX);
      Serial.print("\t");
    }
  }
  if (printResults)
  {
    Serial.println();
  }
}

uint16_t readBQ4050Register(uint8_t reg){
    Wire.beginTransmission(BQ4050addr);
    Wire.write(reg);
    Wire.endTransmission();
    delay(5); // Wait for the device to process the command
    Wire.requestFrom(BQ4050addr, 2);
    if (Wire.available() == 2){
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        return (msb << 8) | lsb;
    }
    return 0xFFFF;
}





void setup() {
    Serial.begin(115200); 
    time_t timeout = millis();
    while (!Serial){
        if ((millis() - timeout) < 10000){
        delay(100);
        }
    }

    Wire.setPins(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.begin(); 
    CalculateTable_CRC8();

    delay(3000); // Wait for serial to initialize


    Serial.println("================================");
    Serial.println("     MeshTower BQ4050 example   ");
    Serial.println("================================");

    Serial.begin(115200);
    Serial.println("Please input JSON string and end with newline:");
}


void loop() {
    String input = "";
    meshsolar_cmd_t cmd;
    static uint32_t cnt = 0; 
    cnt++;

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

    if(cnt % 1000 == 0) {
        uint16_t voltage = readBQ4050Register(BQ4050_REG_VOLT);
        Serial.print("Voltage                     :\t");
        Serial.print(voltage);
        Serial.println("\tmV");

        uint16_t remainingCapacity = readBQ4050Register(BQ4050_REG_CAPACITY_ALARM);
        Serial.print("Remaining Capacity         :\t");
        Serial.print(remainingCapacity);
        Serial.println("\tmAh");
    }
    delay(1);
}