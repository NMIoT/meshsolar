#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "solar.h"
#include "SoftwareWire.h"
#include "bq4050.h"
#include "logger.h"

#define comSerial           Serial
#define SDA_PIN             33
#define SCL_PIN             32

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
BQ4050       bq4050(false); 
MeshSolar    meshsolar;   // Create an instance of MeshSolar


// {"command":"config","battery":{"type":"liion","cell_number":2,"design_capacity":3001,"cutoff_voltage":2551},"temperature_protection":{"discharge_high_temp_c":61,"discharge_low_temp_c":1,"charge_high_temp_c":41,"charge_low_temp_c":1,"temp_enabled":false}}

// {"command":"advance","battery":{"cuv":2701,"eoc":4201,"eoc_protect":4351},"cedv":{"cedv0":2561,"cedv1":2571,"cedv2":2581,"discharge_cedv0":4151,"discharge_cedv10":4051,"discharge_cedv20":4001,"discharge_cedv30":3901,"discharge_cedv40":3851,"discharge_cedv50":3801,"discharge_cedv60":3651,"discharge_cedv70":3551,"discharge_cedv80":3501,"discharge_cedv90":3301,"discharge_cedv100":2561}}

// {"command":"status","soc_gauge": 50,"charge_current": 500,"total_voltage": 12.5,"learned_capacity": 6.6,"cells": [{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7},{ "cell_num": 1, "temperature": 26.5, "voltage": 3.7}]}

// {"command":"sync","times":3}

// {"command":"reset"}


static bool listenString(String& input, char terminator = '\n') {
    while (comSerial.available() > 0) {
        char c = comSerial.read();
        if (c == terminator) return true;   
         else input += c;                   
    }
    return false; 
}


static bool parseJsonCommand(const char* json, meshsolar_config_t* cmd) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        LOG_E("Failed to parse JSON: %s", error.c_str());
        return false;
    }

    // clear the command structure
    memset(cmd, 0, sizeof(meshsolar_config_t));

    if (!doc.containsKey("command")) {
        // dbgSerial.println("Missing 'command' field");
        LOG_E("Missing 'command' field");
        return false;
    }
    strlcpy(cmd->command, doc["command"] | "", sizeof(cmd->command));

    if (strcmp(cmd->command, "config") == 0) {
        if (!doc.containsKey("battery") || !doc.containsKey("temperature_protection")) {
            // dbgSerial.println("Missing 'battery' or 'temperature_protection' field for 'config' command");
            LOG_E("Missing 'battery' or 'temperature_protection' field for 'config' command");
            return false;
        }
        JsonObject battery = doc["battery"];
        JsonObject tp = doc["temperature_protection"];
        if (!battery.containsKey("type") ||
            !battery.containsKey("cell_number") ||
            !battery.containsKey("design_capacity") ||
            !battery.containsKey("cutoff_voltage") ||
            !tp.containsKey("charge_high_temp_c") ||
            !tp.containsKey("charge_low_temp_c") ||
            !tp.containsKey("discharge_high_temp_c") ||
            !tp.containsKey("discharge_low_temp_c") ||
            !tp.containsKey("temp_enabled")) {
            LOG_E("Missing fields in 'battery' or 'temperature_protection'");
            return false;
        }
        strlcpy(cmd->basic.bat_type, battery["type"] | "", sizeof(cmd->basic.bat_type));
        cmd->basic.cell_number = battery["cell_number"] | 0;
        cmd->basic.design_capacity = battery["design_capacity"] | 0;
        cmd->basic.discharge_cutoff_voltage = battery["cutoff_voltage"] | 0;

        cmd->basic.protection.charge_high_temp_c = tp["charge_high_temp_c"] | 0;
        cmd->basic.protection.charge_low_temp_c = tp["charge_low_temp_c"] | 0;
        cmd->basic.protection.discharge_high_temp_c = tp["discharge_high_temp_c"] | 0;
        cmd->basic.protection.discharge_low_temp_c = tp["discharge_low_temp_c"] | 0;
        cmd->basic.protection.enabled = tp["temp_enabled"] | false;
    } 
    else if (strcmp(cmd->command, "switch") == 0) {
        if (!doc.containsKey("fet_en")) {
            LOG_E("Missing 'fet_en' field for 'switch' command");
            return false;
        }
        cmd->fet_en = doc["fet_en"] | false;
    } 
    else if (strcmp(cmd->command, "reset") == 0) {

    } 
    else if (strcmp(cmd->command, "sync") == 0) {

    }
    else {
        // dbgSerial.println("Unknown command");
        LOG_E("Unknown command");
        return false;
    }





    return true;
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


size_t meshsolarBasicConfigToJson(const basic_config_t *config, String& output) {
    StaticJsonDocument<512> doc;
    doc["command"] = "config";
    doc["battery"]["bat_type"] = config->bat_type;
    doc["battery"]["cell_number"] = config->cell_number;
    doc["battery"]["design_capacity"] = config->design_capacity;
    doc["battery"]["cutoff_voltage"] = config->discharge_cutoff_voltage;

    JsonObject protection = doc.createNestedObject("temperature_protection");
    protection["discharge_high_temp_c"] = config->protection.discharge_high_temp_c;
    protection["discharge_low_temp_c"] = config->protection.discharge_low_temp_c;
    protection["charge_high_temp_c"] = config->protection.charge_high_temp_c;
    protection["charge_low_temp_c"] = config->protection.charge_low_temp_c;
    protection["temp_enabled"] = config->protection.enabled;

    output = "";
    return serializeJson(doc, output);
}


size_t meshsolarAdvanceConfigToJson(const advance_config_t *config, String& output) {
    StaticJsonDocument<512> doc;
    doc["command"] = config->command;

    JsonObject battery = doc.createNestedObject("battery");
    battery["cuv"] = config->battery.cuv;
    battery["eoc"] = config->battery.eoc;
    battery["eoc_protect"] = config->battery.eoc_protect;

    JsonObject cedv = doc.createNestedObject("cedv");
    cedv["cedv0"] = config->cedv.cedv0;
    cedv["cedv1"] = config->cedv.cedv1;
    cedv["cedv2"] = config->cedv.cedv2;
    cedv["discharge_cedv0"] = config->cedv.discharge_cedv0;
    cedv["discharge_cedv10"] = config->cedv.discharge_cedv10;
    cedv["discharge_cedv20"] = config->cedv.discharge_cedv20;
    cedv["discharge_cedv30"] = config->cedv.discharge_cedv30;
    cedv["discharge_cedv40"] = config->cedv.discharge_cedv40;
    cedv["discharge_cedv50"] = config->cedv.discharge_cedv50;
    cedv["discharge_cedv60"] = config->cedv.discharge_cedv60;
    cedv["discharge_cedv70"] = config->cedv.discharge_cedv70;
    cedv["discharge_cedv80"] = config->cedv.discharge_cedv80;
    cedv["discharge_cedv90"] = config->cedv.discharge_cedv90;
    cedv["discharge_cedv100"] = config->cedv.discharge_cedv100;

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
    Serial2.begin(115200);              // For debugging, if needed
    bq4050.begin(&Wire, BQ4050ADDR);    // Initialize BQ4050 with SoftwareWire instance
    meshsolar.begin(&bq4050);           // Initialize MeshSolar with bq4050 instance
    LOG_I("MeshSolar initialized successfully");
}



void loop() {
    String input = "";
    static uint32_t cnt = 0; 
    cnt++;

    input = ""; // Reset input for new command
    if(listenString(input, '\n')) {
        // LOG_I("%s", input.c_str());
        bool res = parseJsonCommand(input.c_str(), &meshsolar.cmd); // Parse the command from input
        if (res) {
            // printMeshsolarCmd(&g_bat_cmd);
            /*  add some func call back here base on cmd sector */
            if (0 == strcmp(meshsolar.cmd.command, "config")) {
                bool res = false;

                res = meshsolar.bat_type_setting_update();
                LOG_I(">>>>>>>>    bat_type_setting_update result: %s", res ? "Success" : "Failed");

                res = meshsolar.bat_cells_setting_update();
                LOG_I(">>>>>>>>    bat_cells_setting_update result: %s", res ? "Success" : "Failed");

                res = meshsolar.bat_design_capacity_setting_update();
                LOG_I(">>>>>>>>    bat_design_capacity_setting_update result: %s", res ? "Success" : "Failed");

                res = meshsolar.bat_discharge_cutoff_voltage_setting_update();
                LOG_I(">>>>>>>>    bat_discharge_cutoff_voltage_setting_update result: %s", res ? "Success" : "Failed");

                res = meshsolar.bat_temp_protection_setting_update();
                LOG_I(">>>>>>>>    bat_temp_protection_setting_update result: %s", res ? "Success" : "Failed");


            }
            else if (0 == strcmp(meshsolar.cmd.command, "switch")) {
                meshsolar.bat_fet_toggle(); // Call the callback function for FET control
                // dbgSerial.println("FET Toggle Command Received.");
                LOG_I("FET Toggle Command Received.");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "reset")) {
                meshsolar.bat_reset();      // Call the callback function for reset
                // dbgSerial.println("Resetting BQ4050...");
                LOG_I("Resetting BQ4050...");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "sync")) {
                // String json = "";
                // size_t len = meshsolarBasicConfigToJson(&meshsolar.cmd.basic, json);
                // if(len > 0) {
                //     comSerial.println(json); // Send the configuration back to the serial port
                //     dbgSerial.println(json); // Send the configuration back to the serial port
                // }

                // len = meshsolarAdvanceConfigToJson(&meshsolar.cmd.advance, json);
                // if(len > 0) {
                //     comSerial.println(json); // Send the configuration back to the serial port
                //     dbgSerial.println(json); // Send the configuration back to the serial port
                // }


                // dbgSerial.println("Sync Command Received.");
                LOG_I("Sync Command Received.");
            }
            else{
                // dbgSerial.print("Unknown command: ");
                LOG_E("Unknown command: %s", meshsolar.cmd.command);
                // dbgSerial.println(meshsolar.cmd.command);
            }
        } else {
            // dbgSerial.println("Failed to parse command");
            LOG_E("Failed to parse command");
        }
    }


#if 0
    if(0 == cnt % 1000) {

        bq4050_block_t block= {
            .cmd = DF_CMD_SBS_DATA_CHEMISTRY,
            .len = 5,
            .pvalue =nullptr,
            .type = STRING
        };
        bq4050.read_dataflash_block(&block); 
        for(uint8_t i = 0; i < block.len; i++) {
            dbgSerial.print((char)block.pvalue[i]); // Print as character
        }

        block.pvalue = (uint8_t *)malloc(block.len); // Allocate memory for the block data
        if (block.pvalue == nullptr) {
            dbgSerial.println("Memory allocation failed for block.pvalue");
            return; // Exit if memory allocation fails
        }

        memset(block.pvalue, 0, block.len); // Initialize the block data to zero
        const char *ddd = "lfe4"; // Custom data to write
        block.pvalue[0] = strlen(ddd); // Set the length of the data
        memcpy(block.pvalue + 1, ddd, strlen(ddd)); // Copy custom data into the block
        bq4050.write_dataflash_block(block); // Write back the block to the data flash


        dbgSerial.println();
    }
#endif

#if 0
    if(0 == cnt % 1000) {
        meshsolar.get_bat_realtime_status(); // Update the battery status
        meshsolar.get_bat_realtime_config(); // Update the battery configuration

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
