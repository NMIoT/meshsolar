#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
// #include "SoftwareWire.h"
#include "bq4050.h"



#define SDA_PIN             33
#define SCL_PIN             32

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
BQ4050 bq4050(&Wire, BQ4050ADDR); // Create BQ4050 instance with SoftwareWire

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




// static void printMeshsolarCmd(const meshsolar_cmd_t* cmd) {
//     if (strcmp(cmd->command, "config") == 0) {
//         dbgSerial.println("Battery Config:");
//         dbgSerial.print("       Type: "); dbgSerial.println(cmd->battery.type);
//         dbgSerial.print("       Cell Number: "); dbgSerial.println(cmd->battery.cell_number);
//         dbgSerial.print("       Design Capacity: "); dbgSerial.println(cmd->battery.design_capacity);
//         dbgSerial.print("       Cutoff Voltage: "); dbgSerial.println(cmd->battery.cutoff_voltage);

//         dbgSerial.println("Temperature Protection:");
//         dbgSerial.print("       High Temp (C): "); dbgSerial.println(cmd->temperature_protection.high_temp_c);
//         dbgSerial.print("       High Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.high_temp_enabled ? "true" : "false");
//         dbgSerial.print("       Low Temp (C): "); dbgSerial.println(cmd->temperature_protection.low_temp_c);
//         dbgSerial.print("       Low Temp Enabled: "); dbgSerial.println(cmd->temperature_protection.low_temp_enabled ? "true" : "false");
//     }
//     else if (strcmp(cmd->command, "switch") == 0) {
//         dbgSerial.print("FET Switch: ");
//         dbgSerial.println(cmd->fet_en ? "ON" : "OFF");
//     }
//     else if (strcmp(cmd->command, "reset") == 0) {
//         dbgSerial.println("Device Reset Command Received");
//     }
//     else{
//         dbgSerial.print("Unknown command: ");
//         dbgSerial.println(cmd->command);
//     }
// }

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











// uint16_t bq4050_rd_word(uint8_t reg){
//     Wire.beginTransmission(BQ4050addr);
//     Wire.write(reg);
//     Wire.endTransmission();//send stop signal
//     delay(1); // Wait for the device to process the command
//     Wire.requestFrom(BQ4050addr, 2);//send start signal
//     if (2 == Wire.available()){
//         uint8_t lsb = Wire.read();
//         uint8_t msb = Wire.read();
//         return (msb << 8) | lsb;
//     }
//     return 0xFFFF;
// }

// bool bq4050_rd_word_with_pec(uint8_t reg, uint16_t *value){
//     uint8_t byteArr[] = {
//         (uint8_t)(BQ4050addr <<1 ), // slave write address, lsb = 0 
//         reg,                      
//     };
//     uint8_t PECcheck = Compute_CRC8(byteArr, sizeof(byteArr));

//     Wire.beginTransmission(BQ4050addr);
//     Wire.write(reg);
//     Wire.write(PECcheck);
//     Wire.endTransmission();//send stop signal
//     delay(5);              // Wait for the device to process the command
//     Wire.requestFrom(BQ4050addr, 3);//send start signal
//     if (3 != Wire.available()) return false; // Ensure we have 3 bytes to read

//     uint8_t lsb = Wire.read();
//     uint8_t msb = Wire.read();
//     PECcheck    = Wire.read();

//     uint8_t retArr[] = {
//         (uint8_t)(BQ4050addr<<1) + 1, // slave write address, lsb = 1 
//         lsb, 
//         msb,                      
//     };

//     // dbgSerial.print("PEC received: 0x");
//     // dbgSerial.println(PECcheck, HEX);
//     // dbgSerial.print("PEC calculated: 0x");
//     // dbgSerial.println(Compute_CRC8(retArr, sizeof(retArr)), HEX);




//     if(PECcheck == Compute_CRC8(retArr, sizeof(retArr))){
//         *value = (msb << 8) | lsb;
//         return true; // Return true if PEC matches
//     }
//     return false;
// }


// void bq4050_wd_word(uint8_t reg, uint16_t value){
//   Wire.beginTransmission(BQ4050addr);
//   Wire.write(reg);
//   Wire.write(value & 0xFF);
//   Wire.write(value >> 8);
//   uint8_t result = Wire.endTransmission();
//   delay(10); // Wait for the device to process the command
//   if (result != 0)
//   {
//     comSerial.print("Write to register 0x");
//     comSerial.print(reg, HEX);
//     comSerial.println(" failed.");
//   }
// }






// bool readBQ4050BlockData(uint16_t regAddress, uint8_t *dataVal, uint8_t arrLen ) {
//     uint8_t buf[32+2] = {
//         BQ4050addr<<1,     
//         BLOCK_ACCESS_CMD, 
//         (BQ4050addr<<1) + 1,
//     };

//     Wire.beginTransmission(BQ4050addr);
//     Wire.write(BLOCK_ACCESS_CMD);
//     Wire.endTransmission(false); 
//     Wire.requestFrom(BQ4050addr, arrLen + 2); // send start signal
//     uint32_t start = millis();
//     while (Wire.available() < 1) {
//         if (millis() - start > 500){
//             dbgSerial.println("SMBus read timeout!");
//             return false; 
//         }
//     }

//     // data len, pec not included
//     buf[3] = Wire.read(); // data length
//     if (buf[3] > 32 + 2) {
//         dbgSerial.print("Block data length exceeds 32 bytes, invalid data length : ");
//         dbgSerial.println(buf[3]);
//         return false; // avoid buffer overflow
//     }
//     dbgSerial.print("SMBus block data length: ");
//     dbgSerial.println(buf[3]);
//     dbgSerial.println(Wire.available());
    

//     for (uint8_t i = 0; i < buf[3] - 1; i++) {
//         if (Wire.available()) {
//             buf[i + 4] = Wire.read();
//         }
//         else {
//             dbgSerial.println("Block data read error, not enough data available.");
//             return false; 
//         }
//     }

//     // uint8_t PECcheck = Compute_CRC8(buf, buf[3] + 4); // Calculate PEC for the received data

//     // if (Wire.available()) {
//     //     uint8_t pec = Wire.read();
//     //     if(PECcheck == pec) {
//     //         for (uint8_t i = 0; i < buf[3]; i++) {
//     //             dataVal[i] = buf[i + 4];
//     //         }
//     //     }
//     //     return (PECcheck == pec); // Compare calculated PEC with received PEC
//     // }
//     // dbgSerial.println("Block data read error, no PEC uint8_t received.");
//     // return false; // If PEC does not match, return false



//     for (uint8_t i = 0; i < buf[3]; i++) {
//         dataVal[i] = buf[i + 4];
//     }

//     return true; 
// }


// bool writeMACommand(uint16_t regAddress){
//     //for simplicity, we create an array to hold all the uint8_t we will be sending
//     uint8_t byteArr[5] = {
//         (uint8_t)(BQ4050addr<<1),           // 
//         BLOCK_ACCESS_CMD,                   // 
//         0x02,                               // 
//         (uint8_t)(regAddress & 0xFF),       // 
//         (uint8_t)((regAddress >> 8) & 0xFF) // 
//     };
//     //This array is then sent to the CRC checker. The CRC includes all bytes sent.
//     //The arrSize var is static, so there are always 5 bytes to be sent
//     uint8_t PECcheck = Compute_CRC8( byteArr, sizeof(byteArr) );

//     Wire.beginTransmission(BQ4050addr);
//     //MAC access
//     Wire.write( BLOCK_ACCESS_CMD );
//     //number of bytes in to be sent. In when sending a command, this is always 2
//     Wire.write( 0x02 );
//     // delay(1);
//     //little endian of address
//     Wire.write( regAddress&0xFF );
//     Wire.write((regAddress>>8)&0xFF );
//     //send CRC at the end
//     Wire.write( PECcheck );
//     uint8_t ack = Wire.endTransmission();
//     return (ack == 0); // Return true if transmission was successful
// }


// bool bq4050_read_hw_version() {
//     uint8_t data[2] = {0,};// 2 bytes for hardware version + 1 uint8_t for data length + 1 uint8_t for PEC
//     if (!writeMACommand(MAC_CMD_HW_VER)) {
//         dbgSerial.println("Write MAC CMD HW failed!");
//         return false;
//     }
//     delay(18);
//     if (!readBQ4050BlockData(MAC_CMD_HW_VER, data, sizeof(data))) {
//         dbgSerial.println("Read MAC CMD HW failed!");
//         return false;
//     }

//     for(uint8_t i = 0; i < sizeof(data); i++) {
//         if (data[i] < 0x10) {
//             dbgSerial.print("0");
//         }
//         dbgSerial.print(data[i], HEX);
//         if (i < sizeof(data) - 1) {
//             dbgSerial.print(" ");
//         }
//     }
//     dbgSerial.println();



//     return true;
// }


// bool bq4050_read_fw_version() {
//     uint8_t data[11 + 2] = {0,}; // 11 bytes for firmware version + 1 uint8_t for data length + 1 uint8_t for PEC
//     if (!writeMACommand(MAC_CMD_FW_VER)) {
//         dbgSerial.println("Write MAC CMD FW failed!");
//         return false;
//     }
//     delay(18);
//     if (!readBQ4050BlockData(MAC_CMD_FW_VER, data, sizeof(data))) {
//         dbgSerial.println("Read MAC CMD FW failed!");
//         return false;
//     }

//     for(uint8_t i = 0; i < sizeof(data); i++) {
//         if (data[i] < 0x10) {
//             dbgSerial.print("0");
//         }
//         dbgSerial.print(data[i], HEX);
//         if (i < sizeof(data) - 1) {
//             dbgSerial.print(" ");
//         }
//     }
//     dbgSerial.println();

//     return true;
// }

// bool bq4050_read_cell_temp(DAStatus2_t *temp) {
//     uint8_t data[14 + 2] = {0,};// 14 bytes for cell temperatures + 1 bytes for data length + 1 uint8_t for PEC
//     if (!writeMACommand(MAC_CMD_DA_STATUS2)) {
//         dbgSerial.println("Write cell temp failed!");
//         return false;
//     }
//     delay(18);
//     if (!readBQ4050BlockData(MAC_CMD_DA_STATUS2, data, sizeof(data))) {
//         dbgSerial.println("Read cell temp  failed!");
//         return false;
//     }

//     // for(uint8_t i = 0; i < sizeof(data); i++) {
//     //     if (data[i] < 0x10) {
//     //         dbgSerial.print("0");
//     //     }
//     //     dbgSerial.print(data[i], HEX);
//     //     if (i < sizeof(data) - 1) {
//     //         dbgSerial.print(" ");
//     //     }
//     // }
//     // dbgSerial.println();

//     *temp = *(DAStatus2_t*)(data + 2); // Copy the data into the temp structure

//     return true;
// }

// bool bq4050_fet_control(){
//     if(writeMACommand(MAC_CMD_FET_CONTROL)) {
//         delay(500); // Wait for the device to process the command
//         return true; // Return true if command was sent successfully
//     }
//     return false; // Return false if there was an error sending the command
// }


// bool bq4050_reset(){
//     if(writeMACommand(MAC_CMD_DEV_RESET)) {
//         delay(500); // Wait for the device to reset
//         return true; // Return true if reset command was sent successfully
//     }
//     return false; // Return false if there was an error sending the reset command
// }


// bool bq4050_df_dev_name_read(uint8_t *df, uint8_t len) {

//     uint8_t data[32] = {0,};
//     if (!writeMACommand(DF_CMD_DEVICE_NAME)) {
//         dbgSerial.println("Write df cmd failed!");
//         return false;
//     }
//     delay(18);
//     if (!readBQ4050BlockData(DF_CMD_DEVICE_NAME, data, sizeof(data))) {
//         dbgSerial.println("Read df cmd  failed!");
//         return false;
//     }

//     // for(uint8_t i = 0; i < sizeof(data); i++) {
//     //     if (data[i] < 0x10) {
//     //         dbgSerial.print("0");
//     //     }
//     //     dbgSerial.print(data[i], HEX);
//     //     if (i < sizeof(data) - 1) {
//     //         dbgSerial.print(" ");
//     //     }
//     // }
//     // dbgSerial.println();

//     memcpy(df, data, len); // Copy the data into the df array

//     return true; // Return true if data was read successfully and PEC matches
// }




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
    // CalculateTable_CRC8();
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
                // bq4050_fet_control();
                dbgSerial.println("FET Toggle Command Received.");
            }
            else if (0 == strcmp(cmd_t.command, "reset")) {
                // bq4050_reset();
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
        uint8_t df[21] = {0,}; // Buffer to hold device name data

        bq4050_df_dev_name_read(df , sizeof(df));
        dbgSerial.print("Device Name (ASCII): ");
        // for (uint8_t i = 0; i < sizeof(df); i++) {
        //     if (df[i] == 0) break; // Stop at null terminator
        //     dbgSerial.print((char)df[i]);
        // }
        // dbgSerial.println();
        for(uint8_t i = 0; i < sizeof(df); i++) {
            // if (df[i] == 0) break; // Stop at null terminator
            dbgSerial.print(df[i], HEX);
        }
        dbgSerial.println();





        // uint16_t value = 0;
        // if(bq4050_rd_word_with_pec(BQ4050_REG_VOLT, &value)) g_bat_sta.total_voltage = value / 1000.0f; 
        // dbgSerial.print("Battery Total Voltage: ");
        // dbgSerial.print(g_bat_sta.total_voltage, 2);

        // bq4050_read_hw_version();
        // bq4050_read_fw_version();

        // DAStatus2_t temp = {0,};
        // bq4050_read_cell_temp(&temp);
        // dbgSerial.print("int temp: ");
        // dbgSerial.println(temp.int_temp/10.0f - 273.15f);

        // dbgSerial.print("ts1 temp: ");
        // dbgSerial.println(temp.ts1_temp/10.0f - 273.15f);

        // dbgSerial.print("ts2 temp: ");
        // dbgSerial.println(temp.ts2_temp/10.0f - 273.15f);

        // dbgSerial.print("ts3 temp: ");
        // dbgSerial.println(temp.ts3_temp/10.0f - 273.15f);

        // dbgSerial.print("ts4 temp: ");
        // dbgSerial.println(temp.ts4_temp/10.0f - 273.15f);

        // dbgSerial.print("Cell Temp: ");
        // dbgSerial.println(temp.cell1_temp/10.0f - 273.15f);

        // dbgSerial.print("fet temp: ");
        // dbgSerial.println(temp.fet_temp/10.0f - 273.15f);
    }
#endif

#if 1
    if(0 == cnt % 1000) {
        g_bat_sta.total_voltage  = bq4050.bq4050_rd_word(BQ4050_REG_VOLT);
        g_bat_sta.charge_current = (int16_t)bq4050.bq4050_rd_word(BQ4050_REG_CURRENT);
        g_bat_sta.soc_gauge      = bq4050.bq4050_rd_word(BQ4050_REG_RSOC); 

        delay(10); 
        //update battery cell count
        g_bat_sta.cell_count = g_bat_cmd.battery.cell_number; // Use the cell number from the command

        DAStatus2_t temperature = {0,};
        // bq4050_read_cell_temp(&temperature);

        g_bat_sta.cells[0].temperature = (g_bat_sta.cell_count >= 1) ? temperature.ts1_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[1].temperature = (g_bat_sta.cell_count >= 2) ? temperature.ts2_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[2].temperature = (g_bat_sta.cell_count >= 3) ? temperature.ts3_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[3].temperature = (g_bat_sta.cell_count >= 4) ? temperature.ts4_temp / 10.0f - 273.15f : 0.0f;

        // update battery cell voltage
        for (uint8_t i = 0; i < g_bat_sta.cell_count; i++) {
            g_bat_sta.cells[i].cell_num = i + 1;
            // Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
            uint8_t cell_reg = BQ4050_CELL1_VOLTAGE - i;
            g_bat_sta.cells[i].voltage = bq4050.bq4050_rd_word(cell_reg); 
            delay(10); 
        }
        
        //update battery learned capacity
        // if(bq4050_rd_word_with_pec(BQ4050_REG_FCC, &value)) g_bat_sta.learned_capacity = value/1000.0f; 
        g_bat_sta.learned_capacity = bq4050.bq4050_rd_word(BQ4050_REG_FCC) / 1000.0f; 
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
