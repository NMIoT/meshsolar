#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include <Wire.h>
#include "bq4050.h"

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

byte Compute_CRC8(byte *bytes, uint8_t byteLen){
  // Function to check byte array to be sent out against lookup table to efficiently calculate PEC
  byte crc = 0;
  for (int i = 0; i < byteLen; i++)
  {
    /* XOR-in next input byte */
    byte data = (byte)(bytes[i] ^ crc);
    /* get current CRC value = remainder */
    crc = (byte)(crctable[data]);
    if (printResults)
    {
      Serial.print("byte value: ");
      Serial.print(bytes[i], HEX);
      Serial.print("\tlookup position: ");
      Serial.print(data, HEX);
      Serial.print("\tlookup value: ");
      Serial.println(crc, HEX);
    }
  }

  return crc;
}

// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}


// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}

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

size_t meshsolarStatusToJson(const meshsolar_status_t* status, String& output) {
    StaticJsonDocument<512> doc;
    doc["command"] = status->command;
    doc["soc_gauge"] = status->soc_gauge;
    doc["charge_current"] = status->charge_current;
    doc["total_voltage"] = status->total_voltage;
    doc["learned_capacity"] = status->learned_capacity;

    JsonArray cells = doc.createNestedArray("cells");
    for (int i = 0; i < status->cell_count; ++i) {
        JsonObject cell = cells.createNestedObject();
        cell["cell_num"] = status->cells[i].cell_num;
        cell["temperature"] = status->cells[i].temperature;
        cell["voltage"] = status->cells[i].voltage;
    }

    output = "";
    return serializeJson(doc, output);
}

uint16_t bq4050_rd_word(uint8_t reg){
    Wire.beginTransmission(BQ4050addr);
    Wire.write(reg);
    Wire.endTransmission();
    delay(10); // Wait for the device to process the command
    Wire.requestFrom(BQ4050addr, 2);
    if (2 == Wire.available()){
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        return (msb << 8) | lsb;
    }
    return 0xFFFF;
}

void bq4050_wd_word(uint8_t reg, uint16_t value){
  Wire.beginTransmission(BQ4050addr);
  Wire.write(reg);
  Wire.write(value & 0xFF);
  Wire.write(value >> 8);
  uint8_t result = Wire.endTransmission();
  delay(10); // Wait for the device to process the command
  if (result != 0)
  {
    Serial.print("Write to register 0x");
    Serial.print(reg, HEX);
    Serial.println(" failed.");
  }
}


// 发送MAC块命令
bool writeBQ4050BlockCommand(uint16_t cmd) {
    Wire.beginTransmission(BQ4050addr);
    Wire.write(BLOCK_ACCESS_CMD); // 0x44
    Wire.write(0x02);             // 数据长度2字节
    Wire.write(cmd & 0xFF);       // 低字节
    Wire.write((cmd >> 8) & 0xFF);// 高字节
    uint8_t ack = Wire.endTransmission();
    delay(10);
    return (ack == 0);
}

// 读取MAC块数据
bool readBQ4050BlockData(uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(BQ4050addr);
    Wire.write(BLOCK_ACCESS_CMD); // 0x44
    Wire.endTransmission(false); // 不结束传输，继续发送数据
    delay(10); // 等待设备处理命令
    uint8_t count = Wire.requestFrom(BQ4050addr, len + 2); // [Count][Data...][PEC]
    Serial.print("Read BQ4050 Block Data Count: ");
    Serial.println(count);

    if (count < len + 2) return false;
    uint8_t block_len = Wire.read(); // 第一个字节是数据长度
    if (block_len < len) return false;
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    Wire.read(); // 读PEC字节（可选校验）
    return true;
}

// 读取固件版本号
bool bq4050_read_fw_version(uint8_t *fw_buf, uint8_t fw_len) {
    if (!writeBQ4050BlockCommand(MAC_CMD_HW_VER)) {
        Serial.println("Write MAC_CMD_HW_VER failed!");
        return false;
    }
    delay(5);
    if (!readBQ4050BlockData(fw_buf, fw_len)) {
        Serial.println("Read HW version failed!");
        return false;
    }

    Serial.print("BQ4050 HW Version Block: ");
    for (uint8_t i = 0; i < fw_len; i++) {
        Serial.print(fw_buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    return true;
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
    delay(1000);

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
        uint16_t voltage = bq4050_rd_word(BQ4050_REG_VOLT);
        Serial.print("Voltage                    :\t");
        Serial.print(voltage);
        Serial.println("\tmV");

        uint16_t remainingCapacity = bq4050_rd_word(BQ4050_REG_TIME_ALARM);
        Serial.print("Remaining Capacity         :\t");
        Serial.print(remainingCapacity);
        Serial.println("\tmAh");

        uint16_t fcc = bq4050_rd_word(BQ4050_REG_FCC);
        Serial.print("Full Charge Capacity       :\t");
        Serial.print(fcc);
        Serial.println("\tmAh");


        if(remainingCapacity == 300){
            Serial.println("Remaining Capacity is 300mAh, setting it to 1000mAh");
            bq4050_wd_word(BQ4050_REG_TIME_ALARM, 1000);
        }
        else{
            Serial.println("Remaining Capacity is not 300mAh, no need to set it");
            bq4050_wd_word(BQ4050_REG_TIME_ALARM, 300);
        }



        // uint8_t fw_buf[13] = {0,};
        // bq4050_read_fw_version(fw_buf, 13);
        // Serial.print("BQ4050 hardware Version    :\t");
        // for(uint8_t i = 0; i < 13; i++) {
        //     Serial.print(fw_buf[i], HEX);
        // }
        // Serial.println();
        Serial.println("================================");
    }
    delay(1);
}