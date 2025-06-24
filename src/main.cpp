#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include <Wire.h>
#include "bq4050.h"


#define dbgSerial Serial2
#define comSerial Serial


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
        comSerial.println();
      }
      if (currByte < 16)
        comSerial.print("0");
      comSerial.print(currByte, HEX);
      comSerial.print("\t");
    }
  }
  if (printResults)
  {
    comSerial.println();
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
      comSerial.print("byte value: ");
      comSerial.print(bytes[i], HEX);
      comSerial.print("\tlookup position: ");
      comSerial.print(data, HEX);
      comSerial.print("\tlookup value: ");
      comSerial.println(crc, HEX);
    }
  }

  return crc;
}

// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}


// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}

static void printMeshsolarCmd(const meshsolar_cmd_t* cmd) {
    comSerial.print("Command: ");
    comSerial.println(cmd->command);

    if (strcmp(cmd->command, "config") == 0) {
        comSerial.println("Battery Config:");
        comSerial.print("  Type: "); comSerial.println(cmd->battery.type);
        comSerial.print("  Cell Number: "); comSerial.println(cmd->battery.cell_number);
        comSerial.print("  Design Capacity: "); comSerial.println(cmd->battery.design_capacity);
        comSerial.print("  Cutoff Voltage: "); comSerial.println(cmd->battery.cutoff_voltage);

        comSerial.println("Temperature Protection:");
        comSerial.print("  High Temp (C): "); comSerial.println(cmd->temperature_protection.high_temp_c);
        comSerial.print("  High Temp Enabled: "); comSerial.println(cmd->temperature_protection.high_temp_enabled ? "true" : "false");
        comSerial.print("  Low Temp (C): "); comSerial.println(cmd->temperature_protection.low_temp_c);
        comSerial.print("  Low Temp Enabled: "); comSerial.println(cmd->temperature_protection.low_temp_enabled ? "true" : "false");
    }
    if (strcmp(cmd->command, "switch") == 0) {
        comSerial.print("FET Switch: ");
        comSerial.println(cmd->fet_en ? "ON" : "OFF");
    }
}

static bool parseJsonCommand(const char* json, meshsolar_cmd_t* cmd) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        comSerial.print("Parse failed: ");
        comSerial.println(error.c_str());
        return false;
    }

    // clear the command structure
    memset(cmd, 0, sizeof(meshsolar_cmd_t));

    if (!doc.containsKey("command")) {
        comSerial.println("Missing 'command' field");
        return false;
    }
    strlcpy(cmd->command, doc["command"] | "", sizeof(cmd->command));

    if (strcmp(cmd->command, "config") == 0) {
        if (!doc.containsKey("battery") || !doc.containsKey("temperature_protection")) {
            comSerial.println("Missing 'battery' or 'temperature_protection' field for 'config' command");
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
            comSerial.println("Missing fields in 'battery' or 'temperature_protection'");
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
            comSerial.println("Missing 'fet_en' field for 'switch' command");
            return false;
        }
        cmd->fet_en = doc["fet_en"] | false;
    } else if (strcmp(cmd->command, "reset") == 0) {

    } else {
        comSerial.println("Unknown command");
        return false;
    }

    return true;
}

static bool listenString(String& input, char terminator = '\n') {
    while (comSerial.available() > 0) {
        char c = comSerial.read();
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
    // 保留2位小数
    doc["total_voltage"] = String(status->total_voltage, 2);
    doc["learned_capacity"] = String(status->learned_capacity, 2);

    JsonArray cells = doc.createNestedArray("cells");
    for (int i = 0; i < status->cell_count; ++i) {
        JsonObject cell = cells.createNestedObject();
        cell["cell_num"] = status->cells[i].cell_num;
        cell["temperature"] = String(status->cells[i].temperature, 2);
        cell["voltage"] = String(status->cells[i].voltage, 2);
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
    comSerial.print("Write to register 0x");
    comSerial.print(reg, HEX);
    comSerial.println(" failed.");
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

// // 读取MAC块数据
// bool readBQ4050BlockData() {
//     uint8_t len = 4 + 2; // 读取数据长度
//     uint8_t buf[32] = {0}; 

//     Wire.beginTransmission(BQ4050addr);
//     Wire.write(BLOCK_ACCESS_CMD);
//     Wire.endTransmission(false); 
//     delay(10); 
//     uint8_t count = Wire.requestFrom(BQ4050addr, len); // [Count][Data...][PEC]
//     if(count != len) {
//         comSerial.println("Not enough data available from BQ4050!");
//         return false;
//     }

//     uint8_t block_len = Wire.read(); // 第一个字节是数据长度

//     comSerial.print("Block Length: ");
//     comSerial.println(block_len);

//     for (uint8_t i = 0; i < len; i++) {
//         buf[i] = Wire.read();
//         if (buf[i] < 0x10) comSerial.print("0");
//         comSerial.print(buf[i], HEX);
//         comSerial.print(" ");
//     }
//     // Wire.read(); // 读PEC字节（可选校验）
//     return true;
// }


bool readBQ4050BlockData() {
    uint8_t buf[32] = {0};
    
    // 1. 发送写地址 + 命令码（BLOCK_ACCESS_CMD）
    Wire.beginTransmission(BQ4050addr);
    Wire.write(BLOCK_ACCESS_CMD);
    Wire.endTransmission(false); // 保持连接

    // 2. 发送读地址 + 请求数据（含字节计数）
    uint8_t count = Wire.requestFrom(BQ4050addr, (uint8_t)32); // 请求最大可能长度
    if (count == 0) {
        comSerial.println("No data received!");
        return false;
    }

    // 3. 读取字节计数（第一个字节）
    uint8_t block_len = Wire.read();
    if (block_len > sizeof(buf)) {
        comSerial.println("Data too long for buffer!");
        return false;
    }

    // 4. 读取后续数据（根据block_len）
    for (uint8_t i = 0; i < block_len + 1; i++) {
        buf[i] = Wire.read();
        comSerial.print(buf[i] < 0x10 ? "0" : "");
        comSerial.print(buf[i], HEX);
        comSerial.print(" ");
    }

    // 5. 可选：读取PEC校验字节
    // if (Wire.available()) {
    //     uint8_t pec = Wire.read();
    //     // 校验PEC...
    // }
    return true;
}


// 读取固件版本号
bool bq4050_read_fw_version() {
    if (!writeBQ4050BlockCommand(MAC_CMD_HW_VER)) {
        comSerial.println("Write MAC_CMD_HW_VER failed!");
        return false;
    }
    if (!readBQ4050BlockData()) {
        comSerial.println("Read HW version failed!");
        return false;
    }

    // comSerial.print("BQ4050 HW Version Block: ");
    // for (uint8_t i = 0; i < fw_len; i++) {
    //     comSerial.print(fw_buf[i], HEX);
    //     comSerial.print(" ");
    // }
    // comSerial.println();
    return true;
}


void setup() {
    comSerial.begin(115200); 
    time_t timeout = millis();
    while (!comSerial){
        if ((millis() - timeout) < 10000){
        delay(100);
        }
    }
    dbgSerial.begin(115200); // For debugging, if needed
    dbgSerial.setPins(PIN_SERIAL2_RX, PIN_SERIAL2_TX);


    Wire.setPins(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.begin(); 
    CalculateTable_CRC8();
    delay(1000);

    
    dbgSerial.println("================================");
    dbgSerial.println("     MeshTower BQ4050 example   ");
    dbgSerial.println("================================");
    dbgSerial.println("Please input JSON string and end with newline:");
}


void loop() {
    String input = "";
    meshsolar_cmd_t cmd;
    static uint32_t cnt = 0; 
    static meshsolar_status_t bat_status;
    cnt++;

    input = ""; // Reset input for new command
    if(listenString(input, '\n')) {
        comSerial.print("Received command: ");
        comSerial.println(input);
        bool res = parseJsonCommand(input.c_str(), &cmd);
        if (res) {
            printMeshsolarCmd(&cmd);
            /*  add some func call back here base on cmd sector */




        } else {
            comSerial.println("Failed to parse command");
        }
    }

#if 0
    if(0 == cnt % 1000) {
        uint16_t voltage = bq4050_rd_word(BQ4050_REG_VOLT);
        comSerial.print("Voltage                    :\t");
        comSerial.print(voltage);
        comSerial.println("\tmV");

        uint16_t Vcell1 = bq4050_rd_word(BQ4050_CELL1_VOLTAGE);
        comSerial.print("Cell 1 Voltage             :\t");
        comSerial.print(Vcell1);
        comSerial.println("\tmV");

        uint16_t Vcell2 = bq4050_rd_word(BQ4050_CELL2_VOLTAGE);
        comSerial.print("Cell 2 Voltage             :\t");
        comSerial.print(Vcell2);
        comSerial.println("\tmV");

        uint16_t Vcell3 = bq4050_rd_word(BQ4050_CELL3_VOLTAGE);
        comSerial.print("Cell 3 Voltage             :\t");
        comSerial.print(Vcell3);
        comSerial.println("\tmV");

        uint16_t Vcell4 = bq4050_rd_word(BQ4050_CELL4_VOLTAGE);
        comSerial.print("Cell 4 Voltage             :\t");
        comSerial.print(Vcell4);
        comSerial.println("\tmV");

        uint16_t remainingCapacity = bq4050_rd_word(BQ4050_REG_TIME_ALARM);
        comSerial.print("Remaining Capacity         :\t");
        comSerial.print(remainingCapacity);
        comSerial.println("\tmAh");

        uint16_t fcc = bq4050_rd_word(BQ4050_REG_FCC);
        comSerial.print("Full Charge Capacity       :\t");
        comSerial.print(fcc);
        comSerial.println("\tmAh");


        if(remainingCapacity == 300){
            // comSerial.println("Remaining Capacity is 300mAh, setting it to 1000mAh");
            bq4050_wd_word(BQ4050_REG_TIME_ALARM, 1000);
        }
        else{
            // comSerial.println("Remaining Capacity is not 300mAh, no need to set it");
            bq4050_wd_word(BQ4050_REG_TIME_ALARM, 300);
        }



        // uint8_t fw_buf[2] = {0,};
        bq4050_read_fw_version();
        // comSerial.print("BQ4050 hardware Version    :\t");
        // for(uint8_t i = 0; i < sizeof(fw_buf); i++) {
        //     comSerial.print(fw_buf[i], HEX);
        // }
        comSerial.println();
        comSerial.println("================================");
    }
#endif




    if(0 == cnt % 1500){
        dbgSerial.println(cnt);    

        bat_status.cell_count = 4; // cell count
        bat_status.soc_gauge = random(0, 101); // 0~100%
        bat_status.charge_current = random(0, 2001); // 0~2000 mA
        bat_status.total_voltage = random(1100, 1680) / 100.0; // 11.00~16.80 V
        bat_status.learned_capacity = random(40, 81) / 10.0; // 4.0~8.0 Ah
        for (int i = 0; i < bat_status.cell_count; ++i) {
            bat_status.cells[i].cell_num = i + 1;
            bat_status.cells[i].temperature = random(200, 400) / 10.0; // 20.0~40.0℃
            bat_status.cells[i].voltage = random(320, 430) / 100.0; // 3.20~4.30V
        }
        strlcpy(bat_status.command, "status", sizeof(bat_status.command));
        String json = "";
        meshsolarStatusToJson(&bat_status, json);
        dbgSerial.println(json);
    }
    delay(1);
}