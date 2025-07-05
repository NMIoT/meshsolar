#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include "SoftwareWire.h"
#include "bq4050.h"

#define comSerial           Serial
#define SDA_PIN             33
#define SCL_PIN             32

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
BQ4050       bq4050(false); 
MeshSolar    meshsolar;   // Create an instance of MeshSolar


// {"command":"config","battery":{"type":"LiFePO4","cell_number":4,"design_capacity":3200,"cutoff_voltage":2800},"temperature_protection":{"high_temp_c":60,"high_temp_enabled":true,"low_temp_c":-10,"low_temp_enabled":true}}

// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}

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
    doc["learned_capacity"] = String(status->learned_capacity /1000.0f, 3);

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
    dbgSerial.begin(115200);            // For debugging, if needed
    bq4050.begin(&Wire, BQ4050ADDR);    // Initialize BQ4050 with SoftwareWire instance
    meshsolar.begin(&bq4050);           // Initialize MeshSolar with bq4050 instance
}



void loop() {
    String input = "";
    static uint32_t cnt = 0; 
    cnt++;

    input = ""; // Reset input for new command
    if(listenString(input, '\n')) {
        // dbgSerial.print("Received command: ");
        // dbgSerial.println(input);
        bool res = parseJsonCommand(input.c_str(), &meshsolar.cmd); // Parse the command from input
        if (res) {
            // printMeshsolarCmd(&g_bat_cmd);
            /*  add some func call back here base on cmd sector */
            if (0 == strcmp(meshsolar.cmd.command, "config")) {
                bool res = false;

                res = meshsolar.bat_type_setting_update();
                dbgSerial.print(">>>>>>> bat_type_setting_update result: ");
                dbgSerial.println(res ? "Success" : "Failed");

                res = meshsolar.bat_cells_setting_update();
                dbgSerial.print(">>>>>>> bat_cells_setting_update result: ");
                dbgSerial.println(res ? "Success" : "Failed");

                res = meshsolar.bat_design_capacity_setting_update();
                dbgSerial.print(">>>>>>> bat_design_capacity_setting_update result: ");
                dbgSerial.println(res ? "Success" : "Failed");

                res = meshsolar.bat_discharge_cutoff_voltage_setting_update();
                dbgSerial.print(">>>>>>> bat_discharge_cutoff_voltage_setting_update result: ");
                dbgSerial.println(res ? "Success" : "Failed");

            }
            else if (0 == strcmp(meshsolar.cmd.command, "switch")) {
                meshsolar.bat_fet_toggle(); // Call the callback function for FET control
                dbgSerial.println("FET Toggle Command Received.");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "reset")) {
                meshsolar.bat_reset();      // Call the callback function for reset
                dbgSerial.println("Resetting BQ4050...");
            }
            else{
                dbgSerial.print("Unknown command: ");
                dbgSerial.println(meshsolar.cmd.command);
            }
        } else {
            dbgSerial.println("Failed to parse command");
        }
    }


#if 0
    if(0 == cnt % 1000) {

        bq4050_block_t block= {
            .cmd = DF_CMD_LEARNED_CAPACITY,
            .len = 2,
            .pvalue =nullptr,
            .type = NUMBER
        };
        // bq4050.read_mac_block(&block); 
        // for(uint8_t i = 0; i < block.len; i++) {
        //     if(block.pvalue[i] < 0x10) dbgSerial.print("0");
        //     dbgSerial.print(block.pvalue[i], HEX);
        //     dbgSerial.print(" ");
        // }

        bq4050.read_dataflash_block(&block); 
        dbgSerial.print("DF_CMD_LEARNED_CAPACITY: ");
        for(uint8_t i = 0; i < block.len; i++) {
            // if(0 == block.pvalue[i]) break; // Stop at null terminator
            // dbgSerial.print((char)block.pvalue[i]); // Print as character
            if(block.pvalue[i] < 0x10) dbgSerial.print("0");
            dbgSerial.print(block.pvalue[i], HEX);
        }
        dbgSerial.println();
    }
#endif

#if 1
    if(0 == cnt % 1000) {
        meshsolar.get_bat_status(); // Update the battery status
        
        dbgSerial.println("================================");        
        dbgSerial.print("Battery Total Voltage: ");
        dbgSerial.print(meshsolar.sta.total_voltage, 0);
        dbgSerial.println(" mV");

        for (uint8_t i = 0; i < meshsolar.sta.cell_count; i++) {
            dbgSerial.print("Cell ");
            dbgSerial.print(meshsolar.sta.cells[i].cell_num);
            dbgSerial.print(" Voltage: ");
            dbgSerial.print(meshsolar.sta.cells[i].voltage, 0);
            dbgSerial.println(" mV");
        }

        dbgSerial.print("Learned Capacity: ");
        dbgSerial.print(meshsolar.sta.learned_capacity, 0);
        dbgSerial.println(" mAh");

        dbgSerial.print("Charge Current: ");
        dbgSerial.print(meshsolar.sta.charge_current);
        dbgSerial.println(" mA");

        dbgSerial.print("State of Charge: ");
        dbgSerial.print(meshsolar.sta.soc_gauge);
        dbgSerial.println("%");
    }
#endif


#if 1
    if(0 == cnt % 1500){
        strlcpy(meshsolar.sta.command, "status", sizeof(meshsolar.sta.command));
        String json = "";
        meshsolarStatusToJson(&meshsolar.sta, json);
        // dbgSerial.print("JSON Status: ");
        // dbgSerial.println(json);
        comSerial.println(json);
    }
#endif

    delay(1);
}
