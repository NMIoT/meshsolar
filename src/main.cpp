#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include "SoftwareWire.h"
#include "bq4050.h"


#define SDA_PIN             33
#define SCL_PIN             32

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);

BQ4050 bq4050(false); 

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
    bq4050.begin(&Wire, BQ4050ADDR); // Initialize BQ4050 with SoftwareWire
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
                g_bat_cmd = cmd_t;         // Update global command structure with new config
                uint8_t cells_bits = 0b10; // Default to 0 for 1 cell

                if(cmd_t.battery.cell_number == 1)      cells_bits = 0b00;
                else if(cmd_t.battery.cell_number == 2) cells_bits = 0b01;
                else if(cmd_t.battery.cell_number == 3) cells_bits = 0b10;
                else if(cmd_t.battery.cell_number == 4) cells_bits = 0b11;

                uint8_t da = 0;
                bq4050.rd_df_da_configuration(&da, 1); // Read current DA configuration
                dbgSerial.print("DA Configuration before: 0x");
                dbgSerial.println(da, HEX);

                delay(100); // Ensure the read is complete before modifying


                // Clear the last 2 bits first, then set new cell count bits
                da &= 0b11111100; // Clear bits 0 and 1 (last 2 bits)
                da |= cells_bits;  // Set new cell count bits
                bq4050.wd_df_block(DF_CMD_DA_CONFIGURATION, &da, 1);

                delay(100); // Ensure the read is complete before modifying

                da = 0;
                bq4050.rd_df_da_configuration(&da, 1); // Read current DA configuration again
                dbgSerial.print("DA Configuration after: 0x");
                dbgSerial.println(da, HEX);

            }
            else if (0 == strcmp(cmd_t.command, "switch")) {
                bq4050.fet_toggle();
                dbgSerial.println("FET Toggle Command Received.");
            }
            else if (0 == strcmp(cmd_t.command, "reset")) {
                bq4050.reset();
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
        uint8_t ret = 0;
        bq4050.rd_df_da_configuration(&ret, 1);
        dbgSerial.print("DA Configuration: ");
        dbgSerial.print(ret, HEX);
        dbgSerial.println();

        if(ret == 0x37) {
            uint8_t df = 0x35;
            bq4050.wd_df_block(DF_CMD_DA_CONFIGURATION, &df, 1);
        } else {
            uint8_t df = 0x37;
            bq4050.wd_df_block(DF_CMD_DA_CONFIGURATION, &df, 1);
        }



        // uint8_t df = 0x37;
        // bq4050.wd_df_block(DF_CMD_DA_CONFIGURATION, &df, 1);


        // bq4050.rd_hw_version(df, 2);
        // dbgSerial.print(" Version: ");
        // for (uint8_t i = 0; i < 2; i++) {
        //     if(df[i] < 0x10) dbgSerial.print("0");
        //     dbgSerial.print(df[i], HEX);
        //     dbgSerial.print(" ");
        // }
        // dbgSerial.println();










        // uint16_t value = 0;
        // if(rd_reg_word_with_pec(BQ4050_REG_VOLT, &value)) g_bat_sta.total_voltage = value / 1000.0f; 
        // dbgSerial.print("Battery Total Voltage: ");
        // dbgSerial.print(g_bat_sta.total_voltage, 2);

        // bq4050_read_hw_version();
        // rd_fw_version();

        // DAStatus2_t temp = {0,};
        // rd_cell_temp(&temp);
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
        g_bat_sta.total_voltage  = bq4050.rd_reg_word(BQ4050_REG_VOLT);
        g_bat_sta.charge_current = (int16_t)bq4050.rd_reg_word(BQ4050_REG_CURRENT);
        g_bat_sta.soc_gauge      = bq4050.rd_reg_word(BQ4050_REG_RSOC); 

        delay(10); 
        //update battery cell count
        g_bat_sta.cell_count = g_bat_cmd.battery.cell_number; // Use the cell number from the command

        DAStatus2_t temperature = {0,};
        uint8_t data[14] = {0,};// 14 bytes for cell temperatures
        bq4050.rd_cell_temp(data, sizeof(data));
        memcpy(&temperature, data, sizeof(DAStatus2_t)); // Copy the data into the temperature structure


        g_bat_sta.cells[0].temperature = (g_bat_sta.cell_count >= 1) ? temperature.ts1_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[1].temperature = (g_bat_sta.cell_count >= 2) ? temperature.ts2_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[2].temperature = (g_bat_sta.cell_count >= 3) ? temperature.ts3_temp / 10.0f - 273.15f : 0.0f;
        g_bat_sta.cells[3].temperature = (g_bat_sta.cell_count >= 4) ? temperature.ts4_temp / 10.0f - 273.15f : 0.0f;

        // update battery cell voltage
        for (uint8_t i = 0; i < g_bat_sta.cell_count; i++) {
            g_bat_sta.cells[i].cell_num = i + 1;
            // Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
            uint8_t cell_reg = BQ4050_CELL1_VOLTAGE - i;
            g_bat_sta.cells[i].voltage = bq4050.rd_reg_word(cell_reg); 
            delay(10); 
        }
        
        //update battery learned capacity
        // if(rd_reg_word_with_pec(BQ4050_REG_FCC, &value)) g_bat_sta.learned_capacity = value/1000.0f; 
        g_bat_sta.learned_capacity = bq4050.rd_reg_word(BQ4050_REG_FCC) / 1000.0f; 
        delay(10); 

        // dbgSerial.println("================================");        
        // dbgSerial.print("Battery Total Voltage: ");
        // dbgSerial.print(g_bat_sta.total_voltage, 3);
        // dbgSerial.println(" V");

        // for (uint8_t i = 0; i < g_bat_sta.cell_count; i++) {
        //     dbgSerial.print("Cell ");
        //     dbgSerial.print(g_bat_sta.cells[i].cell_num);
        //     dbgSerial.print(" Voltage: ");
        //     dbgSerial.print(g_bat_sta.cells[i].voltage, 0);
        //     dbgSerial.println(" V");
        // }

        // dbgSerial.print("Learned Capacity: ");
        // dbgSerial.print(g_bat_sta.learned_capacity, 3);
        // dbgSerial.println(" Ah");

        // dbgSerial.print("Charge Current: ");
        // dbgSerial.print(g_bat_sta.charge_current);
        // dbgSerial.println(" mA");

        // dbgSerial.print("State of Charge: ");
        // dbgSerial.print(g_bat_sta.soc_gauge);
        // dbgSerial.println("%");
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
