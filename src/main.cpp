#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include <Wire.h>
#include "bq4050.h"


#define dbgSerial Serial2
#define comSerial Serial


extern byte crctable[256];
extern boolean printResults;

// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}


// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}


static meshsolar_cmd_t bat_cmd = {
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
static meshsolar_status_t bat_sta = {0,};    // Initialize status structure


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
    dbgSerial.print("Command: ");
    dbgSerial.println(cmd->command);

    if (strcmp(cmd->command, "config") == 0) {
        dbgSerial.println("Battery Config:");
        dbgSerial.print("  Type: "); dbgSerial.println(cmd->battery.type);
        dbgSerial.print("  Cell Number: "); dbgSerial.println(cmd->battery.cell_number);
        dbgSerial.print("  Design Capacity: "); dbgSerial.println(cmd->battery.design_capacity);
        dbgSerial.print("  Cutoff Voltage: "); dbgSerial.println(cmd->battery.cutoff_voltage);

        dbgSerial.println("Temperature Protection:");
        dbgSerial.print("  High Temp (C): "); dbgSerial.println(cmd->temperature_protection.high_temp_c);
        dbgSerial.print("  High Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.high_temp_enabled ? "true" : "false");
        dbgSerial.print("  Low Temp (C): "); dbgSerial.println(cmd->temperature_protection.low_temp_c);
        dbgSerial.print("  Low Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.low_temp_enabled ? "true" : "false");
    }
    if (strcmp(cmd->command, "switch") == 0) {
        dbgSerial.print("FET Switch: ");
        dbgSerial.println(cmd->fet_en ? "ON" : "OFF");
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
    delay(5); // Wait for the device to process the command
    Wire.requestFrom(BQ4050addr, 3);//send start signal

    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    PECcheck = Wire.read();

    uint8_t retArr[] = {
        (uint8_t)(BQ4050addr<<1) + 1, // slave write address, lsb = 1 
        lsb, 
        msb,                      
    };

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
    // 1. 发送写地址 + 命令码（BLOCK_ACCESS_CMD）
    Wire.beginTransmission(BQ4050addr);
    Wire.write(BLOCK_ACCESS_CMD);

    bool end = Wire.endTransmission(false); 
    // dbgSerial.print("End Transmission: ");
    // dbgSerial.println(end);

    // 2. 发送读地址 + 请求数据（含字节计数）
    Wire.requestFrom(BQ4050addr, arrLen); // 请求最大可能长度

    // 3. 读取字节计数（第一个字节）
    uint8_t block_len = Wire.read();
    dbgSerial.print("Block Length: ");
    dbgSerial.println(block_len);

    // 4. 读取后续数据（根据block_len）
    for (uint8_t i = 0; i < block_len; i++) {
        dataVal[i] = Wire.read();
        dbgSerial.print(dataVal[i] < 0x10 ? "0" : "");
        dbgSerial.print(dataVal[i], HEX);
        dbgSerial.print(" ");
    }
    dbgSerial.println();

    // 5. 可选：读取PEC校验字节
    if (Wire.available()) {
        uint8_t pec = Wire.read();
        dbgSerial.print("PEC: ");
        dbgSerial.println(pec, HEX);
    }
    return true;
}





bool writeMACommand(uint16_t regAddress){
    //for simplicity, we create an array to hold all the byte we will be sending
    uint8_t byteArr[5] = {
        (uint8_t)(BQ4050addr<<1),           // 设备写地址（8位，最低位为0）
        BLOCK_ACCESS_CMD,                   // 命令码
        0x02,                               // 数据长度
        (uint8_t)(regAddress & 0xFF),       // 低字节
        (uint8_t)((regAddress >> 8) & 0xFF) // 高字节
    };
    //This array is then sent to the CRC checker. The CRC includes all bytes sent.
    //The arrSize var is static, so there are always 5 bytes to be sent
    uint8_t PECcheck = Compute_CRC8( byteArr, sizeof(byteArr) );

    // dbgSerial.print("PEC Check: 0x");
    // dbgSerial.println(PECcheck, HEX);

    Wire.beginTransmission(BQ4050addr);
    //MAC access
    Wire.write( BLOCK_ACCESS_CMD );
    //number of bytes in to be sent. In when sending a command, this is always 2
    Wire.write( 0x02 );
    // delay(10);
    //little endian of address
    Wire.write( regAddress&0xFF );
    Wire.write((regAddress>>8)&0xFF );
    //send CRC at the end
    Wire.write( PECcheck );
    uint8_t ack = Wire.endTransmission();
    return (ack == 0); // Return true if transmission was successful
}


bool readDataReg(byte regAddress, byte *dataVal, uint8_t len ){
  uint8_t buf[16] = {0,};

  //Add in the device address to the buffer
  Wire.beginTransmission( BQ4050addr );
  //Add the one byte register address
  Wire.write( regAddress );
  //Send out buffer and log response
  byte ack = Wire.endTransmission();
  //If data is ackowledged, proceed
  if( ack == 0 ){
    //Request a number of bytes from the device address
    Wire.requestFrom( BQ4050addr , (int16_t) len );
    //If there is data in the in buffer
    if( Wire.available() > 0 ){
      //Cycle through, loading data into array
      for( uint8_t i = 0; i < len; i++ ){
        dataVal[i] = Wire.read();
      }
    }
    return true;
  }else{
    return false; //if I2C comm fails
  }

}


// 读取固件版本号
bool bq4050_read_fw_version() {
    uint8_t data[16] = {0,};
    if (!writeMACommand(MAC_CMD_HW_VER)) {
        dbgSerial.println("Write MAC CMD failed!");
        return false;
    }
    delay(10);
    if (!readBQ4050BlockData(MAC_CMD_HW_VER, data, sizeof(data))) {
        dbgSerial.println("Read MAC CMD  failed!");
        return false;
    }
    return true;
}










void setup() {
#if 0
    comSerial.begin(115200); 
    time_t timeout = millis();
    while (!comSerial){
        if ((millis() - timeout) < 10000){
        delay(100);
        }
    }
#endif

    dbgSerial.begin(115200); // For debugging, if needed
    Wire.setPins(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.begin(); 
    CalculateTable_CRC8();
    // BQ4050.deviceReset(); // Reset the device to ensure it's in a known state
}



void loop() {
    String input = "";
    static uint32_t cnt = 0; 
    cnt++;

    input = ""; // Reset input for new command
    if(listenString(input, '\n')) {
        dbgSerial.print("Received command: ");
        dbgSerial.println(input);
        bool res = parseJsonCommand(input.c_str(), &bat_cmd);
        if (res) {
            printMeshsolarCmd(&bat_cmd);
            /*  add some func call back here base on cmd sector */




        } else {
            dbgSerial.println("Failed to parse command");
        }
    }


#if 1
    if(0 == cnt % 1000) {

        // bq4050_read_fw_version();

        // // uint8_t arr[] = {0x16,0x44,0x02,0x03,0x00};   //pec = 0x53 ok
        // // uint8_t arr[] = {0x16,0x44,0x02,0x02,0x00};   //pec = 0x46 ok
        // uint8_t arr[] = {0x16, 0x0e};  //
        // uint8_t pec = Compute_CRC8(arr, sizeof(arr));
        // dbgSerial.print("PEC Check: 0x");
        // dbgSerial.println(pec, HEX);


        uint16_t value = 0;
        if(bq4050_rd_word_with_pec(BQ4050_REG_VOLT, &value)){
            bat_sta.total_voltage = value; 
        } else {
            dbgSerial.println("Failed to read total voltage");
        }




        dbgSerial.println("================================");        
        dbgSerial.print("Battery Total Voltage: ");
        dbgSerial.print(bat_sta.total_voltage, 1);
        dbgSerial.println(" mV");
        dbgSerial.println(cnt);
    }
#endif

#if 0
    if(0 == cnt % 1000) {
        //update battery total voltage
        bat_sta.total_voltage  = bq4050_rd_word(BQ4050_REG_VOLT) / 1000.0f;
        delay(100);
        //update solar charge current
        bat_sta.charge_current = bq4050_rd_word(BQ4050_REG_CURRENT);
        delay(100);
        //update relative state of charge
        bat_sta.soc_gauge      = bq4050_rd_word(BQ4050_REG_RSOC); // Convert to percentage
        delay(100);
        //update battery cell count
        bat_sta.cell_count = bat_cmd.battery.cell_number; // Use the cell number from the command
        delay(100);

        // update battery cell voltage
        for (uint8_t i = 0; i < bat_cmd.battery.cell_number; i++) {
            bat_sta.cells[i].cell_num = i + 1;
            // Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
            uint8_t cell_reg = BQ4050_CELL1_VOLTAGE - i;
            uint16_t cell_mv = bq4050_rd_word(cell_reg); // raw value in mV
            bat_sta.cells[i].voltage = cell_mv / 1000.0; // convert to V
            delay(100);
        }
        //update battery learned capacity
        bat_sta.learned_capacity = bq4050_rd_word(BQ4050_REG_FCC) / 1000.0; // convert to Ah


        dbgSerial.println("================================");        
        dbgSerial.print("Battery Total Voltage: ");
        dbgSerial.print(bat_sta.total_voltage, 2);
        dbgSerial.println(" V");

        for (uint8_t i = 0; i < bat_cmd.battery.cell_number; i++) {
            dbgSerial.print("Cell ");
            dbgSerial.print(bat_sta.cells[i].cell_num);
            dbgSerial.print(" Voltage: ");
            dbgSerial.print(bat_sta.cells[i].voltage, 2);
            dbgSerial.println(" V");
        }

        dbgSerial.print("Learned Capacity: ");
        dbgSerial.print(bat_sta.learned_capacity, 2);
        dbgSerial.println(" Ah");

        dbgSerial.print("Charge Current: ");
        dbgSerial.print(bat_sta.charge_current);
        dbgSerial.println(" mA");

        dbgSerial.print("Full Charge Capacity: ");
        dbgSerial.print("FCC: ");
        dbgSerial.print(bat_sta.learned_capacity, 2);
        dbgSerial.println(" Ah");
    }
#endif


#if 0
    if(0 == cnt % 1500){
        strlcpy(bat_sta.command, "status", sizeof(bat_sta.command));
        String json = "";
        meshsolarStatusToJson(&bat_sta, json);
        // dbgSerial.print("JSON Status: ");
        // dbgSerial.println(json);
        comSerial.println(json);
    }
#endif

    delay(1);
}




#include <Lorro_BQ4050.h>

//Initialise the device and library
Lorro_BQ4050 BQ4050( BQ4050addr );
//Instantiate the structs
extern Lorro_BQ4050::Regt registers;
uint32_t previousMillis;
uint16_t loopInterval = 1000;















// void loop() {

//   uint32_t currentMillis = millis();

//   if( currentMillis - previousMillis > loopInterval ){
//     previousMillis = currentMillis;

//     // Lorro_BQ4050::Regt registers;
//     BQ4050.readReg( registers.relativeStateOfCharge );
//     dbgSerial.print( "State of charge: " );
//     dbgSerial.print( registers.relativeStateOfCharge.val );
//     dbgSerial.println( "%" );
//     delay( 15 );

//     BQ4050.readReg( registers.voltage );
//     dbgSerial.print( "Pack voltage: " );
//     dbgSerial.print( registers.voltage.val );
//     dbgSerial.println( "mV" );
//     delay( 15 );

//     BQ4050.readReg( registers.cellVoltage1 );
//     dbgSerial.print( "Cell voltage 1: " );
//     dbgSerial.print( registers.cellVoltage1.val );
//     dbgSerial.println( "mV" );
//     delay( 15 );

//     BQ4050.readReg( registers.cellVoltage2 );
//     dbgSerial.print( "Cell voltage 2: " );
//     dbgSerial.print( registers.cellVoltage2.val );
//     dbgSerial.println( "mV" );
//     delay( 15 );

//     BQ4050.readReg( registers.cellVoltage3 );
//     dbgSerial.print( "Cell voltage 3: " );
//     dbgSerial.print( registers.cellVoltage3.val );
//     dbgSerial.println( "mV" );
//     delay( 15 );

//     BQ4050.readReg( registers.cellVoltage4 );
//     dbgSerial.print( "Cell voltage 4: " );
//     dbgSerial.print( registers.cellVoltage4.val );
//     dbgSerial.println( "mV" );
//     delay( 15 );

//     BQ4050.readReg( registers.current );
//     dbgSerial.print( "Current: " );
//     dbgSerial.print( registers.current.val );
//     dbgSerial.println( "mA" );
//     delay( 15 );

//     // BQ4050.getHWversion();



//   }

// }

