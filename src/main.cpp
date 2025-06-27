#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
// #include <Wire.h>
#include "SoftwareWire.h"
#include "bq4050.h"


#define dbgSerial Serial2
#define comSerial Serial

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);

uint8_t crctable[256];

// uint8_t crctable[256] = {
//   0x00,0x07,0x0E,0x09,0x1C,0x1B,0x12,0x15,0x38,0x3F,0x36,0x31,0x24,0x23,0x2A,0x2D,
//   0x70,0x77,0x7E,0x79,0x6C,0x6B,0x62,0x65,0x48,0x4F,0x46,0x41,0x54,0x53,0x5A,0x5D,
//   0xE0,0xE7,0xEE,0xE9,0xFC,0xFB,0xF2,0xF5,0xD8,0xDF,0xD6,0xD1,0xC4,0xC3,0xCA,0xCD,
//   0x90,0x97,0x9E,0x99,0x8C,0x8B,0x82,0x85,0xA8,0xAF,0xA6,0xA1,0xB4,0xB3,0xBA,0xBD,
//   0xC7,0xC0,0xC9,0xCE,0xDB,0xDC,0xD5,0xD2,0xFF,0xF8,0xF1,0xF6,0xE3,0xE4,0xED,0xEA,
//   0xB7,0xB0,0xB9,0xBE,0xAB,0xAC,0xA5,0xA2,0x8F,0x88,0x81,0x86,0x93,0x94,0x9D,0x9A,
//   0x27,0x20,0x29,0x2E,0x3B,0x3C,0x35,0x32,0x1F,0x18,0x11,0x16,0x03,0x04,0x0D,0x0A,
//   0x57,0x50,0x59,0x5E,0x4B,0x4C,0x45,0x42,0x6F,0x68,0x61,0x66,0x73,0x74,0x7D,0x7A,
//   0x89,0x8E,0x87,0x80,0x95,0x92,0x9B,0x9C,0xB1,0xB6,0xBF,0xB8,0xAD,0xAA,0xA3,0xA4,
//   0xF9,0xFE,0xF7,0xF0,0xE5,0xE2,0xEB,0xEC,0xC1,0xC6,0xCF,0xC8,0xDD,0xDA,0xD3,0xD4,
//   0x69,0x6E,0x67,0x60,0x75,0x72,0x7B,0x7C,0x51,0x56,0x5F,0x58,0x4D,0x4A,0x43,0x44,
//   0x19,0x1E,0x17,0x10,0x05,0x02,0x0B,0x0C,0x21,0x26,0x2F,0x28,0x3D,0x3A,0x33,0x34,
//   0x4E,0x49,0x40,0x47,0x52,0x55,0x5C,0x5B,0x76,0x71,0x78,0x7F,0x6A,0x6D,0x64,0x63,
//   0x3E,0x39,0x30,0x37,0x22,0x25,0x2C,0x2B,0x06,0x01,0x08,0x0F,0x1A,0x1D,0x14,0x13,
//   0xAE,0xA9,0xA0,0xA7,0xB2,0xB5,0xBC,0xBB,0x96,0x91,0x98,0x9F,0x8A,0x8D,0x84,0x83,
//   0xDE,0xD9,0xD0,0xD7,0xC2,0xC5,0xCC,0xCB,0xE6,0xE1,0xE8,0xEF,0xFA,0xFD,0xF4,0xF3
// };
bool printResults;

// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}


// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}


static meshsolar_cmd_t g_bat_cmd = {
    "", // command
    {   // battery
        "LiFePO4", // type
        3,         // cell_number
        3200,      // design_capacity
        2800       // cutoff_voltage
    },
    {   // temperature_protection
        60,   // high_temp_c
        true, // high_temp_enabled
        -10,  // low_temp_c
        true  // low_temp_enabled
    },
    false // fet_en
};
static meshsolar_status_t g_bat_sta = {0,};    // Initialize status structure


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

static void printMeshsolarCmd(const meshsolar_cmd_t* cmd) {
    if (strcmp(cmd->command, "config") == 0) {
        dbgSerial.println("Battery Config:");
        dbgSerial.print("       Type: "); dbgSerial.println(cmd->battery.type);
        dbgSerial.print("       Cell Number: "); dbgSerial.println(cmd->battery.cell_number);
        dbgSerial.print("       Design Capacity: "); dbgSerial.println(cmd->battery.design_capacity);
        dbgSerial.print("       Cutoff Voltage: "); dbgSerial.println(cmd->battery.cutoff_voltage);

        dbgSerial.println("Temperature Protection:");
        dbgSerial.print("       High Temp (C): "); dbgSerial.println(cmd->temperature_protection.high_temp_c);
        dbgSerial.print("       High Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.high_temp_enabled ? "true" : "false");
        dbgSerial.print("       Low Temp (C): "); dbgSerial.println(cmd->temperature_protection.low_temp_c);
        dbgSerial.print("       Low Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.low_temp_enabled ? "true" : "false");
    }
    else if (strcmp(cmd->command, "switch") == 0) {
        dbgSerial.print("FET Switch: ");
        dbgSerial.println(cmd->fet_en ? "ON" : "OFF");
    }
    else if (strcmp(cmd->command, "reset") == 0) {
        dbgSerial.println("Device Reset Command Received");
    }
    else{
        dbgSerial.print("Unknown command: ");
        dbgSerial.println(cmd->command);
    }
}

static bool parseJsonCommand(const char* json, meshsolar_cmd_t* cmd) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        dbgSerial.print("Parse failed: ");
        dbgSerial.println(error.c_str());
        return false;
    }

    // clear the command structure
    memset(cmd, 0, sizeof(meshsolar_cmd_t));

    if (!doc.containsKey("command")) {
        dbgSerial.println("Missing 'command' field");
        return false;
    }
    strlcpy(cmd->command, doc["command"] | "", sizeof(cmd->command));

    if (strcmp(cmd->command, "config") == 0) {
        if (!doc.containsKey("battery") || !doc.containsKey("temperature_protection")) {
            dbgSerial.println("Missing 'battery' or 'temperature_protection' field for 'config' command");
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
            dbgSerial.println("Missing fields in 'battery' or 'temperature_protection'");
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
            dbgSerial.println("Missing 'fet_en' field for 'switch' command");
            return false;
        }
        cmd->fet_en = doc["fet_en"] | false;
    } else if (strcmp(cmd->command, "reset") == 0) {

    } else {
        dbgSerial.println("Unknown command");
        return false;
    }

    return true;
}

static bool listenString(String& input, char terminator = '\n') {
    while (comSerial.available() > 0) {
        char c = comSerial.read();
        if (c == terminator) return true; // End of input
         else input += c; // Append character to input
    }
    return false; // Should never reach here
}

size_t meshsolarStatusToJson(const meshsolar_status_t* status, String& output) {
    StaticJsonDocument<512> doc;
    doc["command"] = status->command;
    doc["soc_gauge"] = status->soc_gauge;
    doc["charge_current"] = status->charge_current;
    doc["total_voltage"] = String(status->total_voltage/1000.0f, 3);
    doc["learned_capacity"] = String(status->learned_capacity, 3);

    JsonArray cells = doc.createNestedArray("cells");
    for (int i = 0; i < status->cell_count; ++i) {
        JsonObject cell = cells.createNestedObject();
        cell["cell_num"] = status->cells[i].cell_num;
        cell["temperature"] = String(status->cells[i].temperature, 2) + "C";
        cell["voltage"] = String(status->cells[i].voltage/1000.0f, 3) + "V";
    }

    output = "";
    return serializeJson(doc, output);
}

uint16_t bq4050_rd_word(uint8_t reg){
    Wire.beginTransmission(BQ4050addr);
    Wire.write(reg);
    Wire.endTransmission();//send stop signal
    delay(1); // Wait for the device to process the command
    Wire.requestFrom(BQ4050addr, 2);//send start signal
    if (2 == Wire.available()){
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        return (msb << 8) | lsb;
    }
    return 0xFFFF;
}

bool bq4050_rd_word_with_pec(uint8_t reg, uint16_t *value){
    uint8_t byteArr[] = {
        (uint8_t)(BQ4050addr <<1 ), // slave write address, lsb = 0 
        reg,                      
    };
    uint8_t PECcheck = Compute_CRC8(byteArr, sizeof(byteArr));

    Wire.beginTransmission(BQ4050addr);
    Wire.write(reg);
    Wire.write(PECcheck);
    Wire.endTransmission();//send stop signal
    delay(5);              // Wait for the device to process the command
    Wire.requestFrom(BQ4050addr, 3);//send start signal
    if (3 != Wire.available()) return false; // Ensure we have 3 bytes to read

    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    PECcheck    = Wire.read();

    uint8_t retArr[] = {
        (uint8_t)(BQ4050addr<<1) + 1, // slave write address, lsb = 1 
        lsb, 
        msb,                      
    };

    // dbgSerial.print("PEC received: 0x");
    // dbgSerial.println(PECcheck, HEX);
    // dbgSerial.print("PEC calculated: 0x");
    // dbgSerial.println(Compute_CRC8(retArr, sizeof(retArr)), HEX);




    if(PECcheck == Compute_CRC8(retArr, sizeof(retArr))){
        *value = (msb << 8) | lsb;
        return true; // Return true if PEC matches
    }
    return false;
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


bool readBQ4050BlockData(byte regAddress, byte *dataVal, uint8_t arrLen ) {
    uint8_t buf[32] = {
        BQ4050addr<<1,     
        BLOCK_ACCESS_CMD, 
        (BQ4050addr<<1) + 1,
    };

    Wire.beginTransmission(BQ4050addr);
    Wire.write(BLOCK_ACCESS_CMD);
    Wire.endTransmission(false); 
    Wire.requestFrom(BQ4050addr, arrLen + 2); // send start signal
    uint32_t start = millis();
    while (Wire.available() < 1) {
        if (millis() - start > 500){
            dbgSerial.println("SMBus read timeout!");
            return false; 
        }
    }

    // data len, pec not included
    buf[3] = Wire.read(); // data length
    if (buf[3] > 32) {
        dbgSerial.println("Block data length exceeds 32 bytes, invalid data.");
        return false; // avoid buffer overflow
    }
    // dbgSerial.print("SMBus block data length: ");
    // dbgSerial.println(buf[3]);




    for (uint8_t i = 0; i < buf[3]; i++) {
        if (Wire.available()) {
            buf[i + 4] = Wire.read();
        }
        else {
            dbgSerial.println("Block data read error, not enough data available.");
            return false; 
        }
    }

    uint8_t PECcheck = Compute_CRC8(buf, buf[3] + 4); // Calculate PEC for the received data

    if (Wire.available()) {
        uint8_t pec = Wire.read();
        if(PECcheck == pec) {
            for (uint8_t i = 0; i < buf[3]; i++) {
                dataVal[i] = buf[i + 4];
            }
        }
        return (PECcheck == pec); // Compare calculated PEC with received PEC
    }
    dbgSerial.println("Block data read error, no PEC byte received.");
    return false; // If PEC does not match, return false
}


bool writeMACommand(uint16_t regAddress){
    //for simplicity, we create an array to hold all the byte we will be sending
    uint8_t byteArr[5] = {
        (uint8_t)(BQ4050addr<<1),           // 
        BLOCK_ACCESS_CMD,                   // 
        0x02,                               // 
        (uint8_t)(regAddress & 0xFF),       // 
        (uint8_t)((regAddress >> 8) & 0xFF) // 
    };
    //This array is then sent to the CRC checker. The CRC includes all bytes sent.
    //The arrSize var is static, so there are always 5 bytes to be sent
    uint8_t PECcheck = Compute_CRC8( byteArr, sizeof(byteArr) );

    Wire.beginTransmission(BQ4050addr);
    //MAC access
    Wire.write( BLOCK_ACCESS_CMD );
    //number of bytes in to be sent. In when sending a command, this is always 2
    Wire.write( 0x02 );
    // delay(1);
    //little endian of address
    Wire.write( regAddress&0xFF );
    Wire.write((regAddress>>8)&0xFF );
    //send CRC at the end
    Wire.write( PECcheck );
    uint8_t ack = Wire.endTransmission();
    return (ack == 0); // Return true if transmission was successful
}


bool bq4050_read_hw_version() {
    uint8_t data[2] = {0,};// 2 bytes for hardware version + 1 byte for data length + 1 byte for PEC
    if (!writeMACommand(MAC_CMD_HW_VER)) {
        dbgSerial.println("Write MAC CMD HW failed!");
        return false;
    }
    delay(18);
    if (!readBQ4050BlockData(MAC_CMD_HW_VER, data, sizeof(data))) {
        dbgSerial.println("Read MAC CMD HW failed!");
        return false;
    }

    for(uint8_t i = 0; i < sizeof(data); i++) {
        if (data[i] < 0x10) {
            dbgSerial.print("0");
        }
        dbgSerial.print(data[i], HEX);
        if (i < sizeof(data) - 1) {
            dbgSerial.print(" ");
        }
    }
    dbgSerial.println();



    return true;
}


bool bq4050_read_fw_version() {
    uint8_t data[11 + 2] = {0,}; // 11 bytes for firmware version + 1 byte for data length + 1 byte for PEC
    if (!writeMACommand(MAC_CMD_FW_VER)) {
        dbgSerial.println("Write MAC CMD FW failed!");
        return false;
    }
    delay(18);
    if (!readBQ4050BlockData(MAC_CMD_FW_VER, data, sizeof(data))) {
        dbgSerial.println("Read MAC CMD FW failed!");
        return false;
    }

    for(uint8_t i = 0; i < sizeof(data); i++) {
        if (data[i] < 0x10) {
            dbgSerial.print("0");
        }
        dbgSerial.print(data[i], HEX);
        if (i < sizeof(data) - 1) {
            dbgSerial.print(" ");
        }
    }
    dbgSerial.println();

    return true;
}

bool bq4050_read_cell_temp(DAStatus2_t *temp) {
    uint8_t data[14 + 2] = {0,};// 14 bytes for cell temperatures + 1 bytes for data length + 1 byte for PEC
    if (!writeMACommand(MAC_CMD_DA_STATUS2)) {
        dbgSerial.println("Write cell temp failed!");
        return false;
    }
    delay(18);
    if (!readBQ4050BlockData(MAC_CMD_DA_STATUS2, data, sizeof(data))) {
        dbgSerial.println("Read cell temp  failed!");
        return false;
    }

    // for(uint8_t i = 0; i < sizeof(data); i++) {
    //     if (data[i] < 0x10) {
    //         dbgSerial.print("0");
    //     }
    //     dbgSerial.print(data[i], HEX);
    //     if (i < sizeof(data) - 1) {
    //         dbgSerial.print(" ");
    //     }
    // }
    // dbgSerial.println();

    *temp = *(DAStatus2_t*)(data + 2); // Copy the data into the temp structure

    return true;
}

bool bq4050_fet_control(){
    if(writeMACommand(MAC_CMD_FET_CONTROL)) {
        delay(500); // Wait for the device to process the command
        return true; // Return true if command was sent successfully
    }
    return false; // Return false if there was an error sending the command
}


bool bq4050_reset(){
    if(writeMACommand(MAC_CMD_DEV_RESET)) {
        delay(500); // Wait for the device to reset
        return true; // Return true if reset command was sent successfully
    }
    return false; // Return false if there was an error sending the reset command
}


void setup() {
#if 1
    comSerial.begin(115200); 
    time_t timeout = millis();
    while (!comSerial){
        if ((millis() - timeout) < 10000){
        delay(100);
        }
    }
#endif
    dbgSerial.begin(115200);        // For debugging, if needed
    Wire.begin(); 
    CalculateTable_CRC8();
}



void loop() {
    String input = "";
    static uint32_t cnt = 0; 
    cnt++;

    input = ""; // Reset input for new command
    if(listenString(input, '\n')) {
        // dbgSerial.print("Received command: ");
        // dbgSerial.println(input);
        meshsolar_cmd_t cmd_t = {0,}; // Initialize command structure
        bool res = parseJsonCommand(input.c_str(), &cmd_t);
        if (res) {
            // printMeshsolarCmd(&g_bat_cmd);
            /*  add some func call back here base on cmd sector */
            if (0 == strcmp(cmd_t.command, "config")) {
                g_bat_cmd = cmd_t; // Update global command structure with new config
                dbgSerial.println("Configuring Battery cmd Received.");
            }
            else if (0 == strcmp(cmd_t.command, "switch")) {
                bq4050_fet_control();
                dbgSerial.println("FET Toggle Command Received.");
            }
            else if (0 == strcmp(cmd_t.command, "reset")) {
                bq4050_reset();
                dbgSerial.println("Resetting BQ4050...");
            }
            else{
                dbgSerial.print("Unknown command: ");
                dbgSerial.println(cmd_t.command);
            }
        } else {
            dbgSerial.println("Failed to parse command");
        }
    }


#if 0
    if(0 == cnt % 1000) {
        // uint16_t value = 0;
        // if(bq4050_rd_word_with_pec(BQ4050_REG_VOLT, &value)) g_bat_sta.total_voltage = value / 1000.0f; 
        // dbgSerial.print("Battery Total Voltage: ");
        // dbgSerial.print(g_bat_sta.total_voltage, 2);

        // bq4050_read_hw_version();
        // bq4050_read_fw_version();

        DAStatus2_t temp = {0,};
        bq4050_read_cell_temp(&temp);
        dbgSerial.print("int temp: ");
        dbgSerial.println(temp.int_temp/10.0f - 273.15f);

        dbgSerial.print("ts1 temp: ");
        dbgSerial.println(temp.ts1_temp/10.0f - 273.15f);

        dbgSerial.print("ts2 temp: ");
        dbgSerial.println(temp.ts2_temp/10.0f - 273.15f);

        dbgSerial.print("ts3 temp: ");
        dbgSerial.println(temp.ts3_temp/10.0f - 273.15f);

        dbgSerial.print("ts4 temp: ");
        dbgSerial.println(temp.ts4_temp/10.0f - 273.15f);

        dbgSerial.print("Cell Temp: ");
        dbgSerial.println(temp.cell1_temp/10.0f - 273.15f);

        dbgSerial.print("fet temp: ");
        dbgSerial.println(temp.fet_temp/10.0f - 273.15f);
    }
#endif

#if 1
    if(0 == cnt % 1000) {
        g_bat_sta.total_voltage  = bq4050_rd_word(BQ4050_REG_VOLT);
        g_bat_sta.charge_current = (int16_t)bq4050_rd_word(BQ4050_REG_CURRENT);
        g_bat_sta.soc_gauge      = bq4050_rd_word(BQ4050_REG_RSOC); 

        delay(10); 
        //update battery cell count
        g_bat_sta.cell_count = g_bat_cmd.battery.cell_number; // Use the cell number from the command

        DAStatus2_t temperature = {0,};
        bq4050_read_cell_temp(&temperature);

        g_bat_sta.cells[0].temperature = (g_bat_sta.cell_count >= 1) ? temperature.ts1_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[1].temperature = (g_bat_sta.cell_count >= 2) ? temperature.ts2_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[2].temperature = (g_bat_sta.cell_count >= 3) ? temperature.ts3_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[3].temperature = (g_bat_sta.cell_count >= 4) ? temperature.ts4_temp / 10.0f - 273.15f : 0.0f;

        // update battery cell voltage
        for (uint8_t i = 0; i < g_bat_sta.cell_count; i++) {
            g_bat_sta.cells[i].cell_num = i + 1;
            // Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
            uint8_t cell_reg = BQ4050_CELL1_VOLTAGE - i;
            g_bat_sta.cells[i].voltage = bq4050_rd_word(cell_reg); 
            delay(10); 
        }
        
        //update battery learned capacity
        // if(bq4050_rd_word_with_pec(BQ4050_REG_FCC, &value)) g_bat_sta.learned_capacity = value/1000.0f; 
        g_bat_sta.learned_capacity = bq4050_rd_word(BQ4050_REG_FCC) / 1000.0f; 
        delay(10); 

        dbgSerial.println("================================");        
        dbgSerial.print("Battery Total Voltage: ");
        dbgSerial.print(g_bat_sta.total_voltage, 3);
        dbgSerial.println(" V");

        for (uint8_t i = 0; i < g_bat_sta.cell_count; i++) {
            dbgSerial.print("Cell ");
            dbgSerial.print(g_bat_sta.cells[i].cell_num);
            dbgSerial.print(" Voltage: ");
            dbgSerial.print(g_bat_sta.cells[i].voltage, 0);
            dbgSerial.println(" V");
        }

        dbgSerial.print("Learned Capacity: ");
        dbgSerial.print(g_bat_sta.learned_capacity, 3);
        dbgSerial.println(" Ah");

        dbgSerial.print("Charge Current: ");
        dbgSerial.print(g_bat_sta.charge_current);
        dbgSerial.println(" mA");

        dbgSerial.print("State of Charge: ");
        dbgSerial.print(g_bat_sta.soc_gauge);
        dbgSerial.println("%");
    }
#endif


#if 1
    if(0 == cnt % 1500){
        strlcpy(g_bat_sta.command, "status", sizeof(g_bat_sta.command));
        String json = "";
        meshsolarStatusToJson(&g_bat_sta, json);
        // dbgSerial.print("JSON Status: ");
        // dbgSerial.println(json);
        comSerial.println(json);
    }
#endif

    delay(1);
}
