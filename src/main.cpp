#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "meshsolar.h"
#include "SoftwareWire.h"
#include "bq4050.h"
#include "logger.h"

#define comSerial           Serial
#define SDA_PIN             33
#define SCL_PIN             32

SoftwareWire Wire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
BQ4050       bq4050(false); 
MeshSolar    meshsolar;   // Create an instance of MeshSolar

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
        strlcpy(cmd->basic.type, battery["type"] | "", sizeof(cmd->basic.type));
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
        cmd->fet_en.enable = doc["fet_en"] | false;
    } 
    else if (strcmp(cmd->command, "advance") == 0) {
        if(doc["battery"].isNull() || doc["cedv"].isNull()) {
            LOG_E("Missing 'battery' or 'cedv' field for 'advance' command");
            return false;
        }
        JsonObject battery = doc["battery"];
        JsonObject cedv = doc["cedv"];
        if (!battery.containsKey("cuv") ||
            !battery.containsKey("eoc") ||
            !battery.containsKey("eoc_protect") ||
            !cedv.containsKey("cedv0") ||
            !cedv.containsKey("cedv1") ||
            !cedv.containsKey("cedv2") ||
            !cedv.containsKey("discharge_cedv0") ||
            !cedv.containsKey("discharge_cedv10") ||
            !cedv.containsKey("discharge_cedv20") ||
            !cedv.containsKey("discharge_cedv30") ||
            !cedv.containsKey("discharge_cedv40") ||
            !cedv.containsKey("discharge_cedv50") ||
            !cedv.containsKey("discharge_cedv60") ||
            !cedv.containsKey("discharge_cedv70") ||
            !cedv.containsKey("discharge_cedv80") ||
            !cedv.containsKey("discharge_cedv90") ||
            !cedv.containsKey("discharge_cedv100")) {
                LOG_E("Missing fields in 'battery' or 'cedv'");
                return false;
        }
        cmd->advance.battery.cuv            = battery["cuv"] | 0;
        cmd->advance.battery.eoc            = battery["eoc"] | 0;
        cmd->advance.battery.eoc_protect    = battery["eoc_protect"] | 0;
        cmd->advance.cedv.cedv0             = cedv["cedv0"] | 0;
        cmd->advance.cedv.cedv1             = cedv["cedv1"] | 0;
        cmd->advance.cedv.cedv2             = cedv["cedv2"] | 0;
        cmd->advance.cedv.discharge_cedv0   = cedv["discharge_cedv0"]  | 0;
        cmd->advance.cedv.discharge_cedv10  = cedv["discharge_cedv10"] | 0;
        cmd->advance.cedv.discharge_cedv20  = cedv["discharge_cedv20"] | 0;
        cmd->advance.cedv.discharge_cedv30  = cedv["discharge_cedv30"] | 0;
        cmd->advance.cedv.discharge_cedv40  = cedv["discharge_cedv40"] | 0;
        cmd->advance.cedv.discharge_cedv50  = cedv["discharge_cedv50"] | 0;
        cmd->advance.cedv.discharge_cedv60  = cedv["discharge_cedv60"] | 0;
        cmd->advance.cedv.discharge_cedv70  = cedv["discharge_cedv70"] | 0;
        cmd->advance.cedv.discharge_cedv80  = cedv["discharge_cedv80"] | 0;
        cmd->advance.cedv.discharge_cedv90  = cedv["discharge_cedv90"] | 0;
        cmd->advance.cedv.discharge_cedv100 = cedv["discharge_cedv100"]| 0;
    } 
    else if (strcmp(cmd->command, "reset") == 0) {

    } 
    else if (strcmp(cmd->command, "sync") == 0) {
        if (!doc.containsKey("times")) {
            LOG_E("Missing 'times' field for 'sync' command");
            return false;
        }
        cmd->sync.times = doc["times"] | 1; // Default to 1 if not specified
        if (cmd->sync.times < 1 || cmd->sync.times > 10) {
            LOG_E("'times' must be between 1 and 10");
            cmd->sync.times = 10; // Reset to default if out of range
            return false;
        }
    } 
    else if (strcmp(cmd->command, "status") == 0) {
        // No additional fields required for status command
    }
    else {
        LOG_E("Unknown command '%s'", cmd->command);
        return false;
    }

    return true;
}


size_t meshsolarStatusToJson(const meshsolar_status_t* status, String& output) {
    output = "";
    StaticJsonDocument<512> doc;
    doc["command"]          = "status";
    doc["soc_gauge"]        = status->soc_gauge;
    doc["charge_current"]   = status->charge_current;
    doc["total_voltage"]    = String(status->total_voltage/1000.0f, 3);
    doc["learned_capacity"] = String(status->learned_capacity /1000.0f, 3);
    doc["pack_voltage"]     = String(status->pack_voltage);
    doc["fet_enable"]       = status->fet_enable;
    doc["protection_sta"]   = status->protection_sta;

    JsonArray cells = doc.createNestedArray("cells");
    for (int i = 0; i < 4; ++i) {
        JsonObject cell     = cells.createNestedObject();
        cell["cell_num"]    = status->cells[i].cell_num;
        cell["temperature"] = status->cells[i].temperature;
        cell["voltage"]     = status->cells[i].voltage/1000.0f;
    }

    output = "";
    return serializeJson(doc, output);
}


size_t meshsolarBasicConfigToJson(const basic_config_t *basic, String& output) {
    output = "";
    StaticJsonDocument<512> doc;
    doc["command"] = "config";
    doc["battery"]["type"] = String(basic->type);
    doc["battery"]["cell_number"] = basic->cell_number;
    doc["battery"]["design_capacity"] = basic->design_capacity;
    doc["battery"]["cutoff_voltage"] = basic->discharge_cutoff_voltage;

    JsonObject protection = doc.createNestedObject("temperature_protection");
    protection["discharge_high_temp_c"] = basic->protection.discharge_high_temp_c;
    protection["discharge_low_temp_c"]  = basic->protection.discharge_low_temp_c;
    protection["charge_high_temp_c"]    = basic->protection.charge_high_temp_c;
    protection["charge_low_temp_c"]     = basic->protection.charge_low_temp_c;
    protection["temp_enabled"]          = basic->protection.enabled;

    output = "";
    return serializeJson(doc, output);
}


size_t meshsolarAdvanceConfigToJson(const advance_config_t *config, String& output) {
    output = "";
    StaticJsonDocument<512> doc;
    doc["command"] = "advance";

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


size_t meshsolarCmdRspToJson(bool status, String& output) {
    output = "";
    StaticJsonDocument<64> doc;
    doc["command"] = "rsp";
    doc["status"] = status;
    output = "";
    // Serialize the JSON document to the output string
    return serializeJson(doc, output);
}

void setup() {
    Serial2.begin(115200);              // For debugging, if needed
    comSerial.begin(115200);            // Initialize the main serial port for communication
    bq4050.begin(&Wire, BQ4050ADDR);    // Initialize BQ4050 with SoftwareWire instance
    meshsolar.begin(&bq4050);           // Initialize MeshSolar with bq4050 instance
    LOG_I("MeshSolar initialized successfully");
}



// {"command":"config","battery":{"type":"liion","cell_number":2,"design_capacity":3001,"cutoff_voltage":2551},"temperature_protection":{"discharge_high_temp_c":61,"discharge_low_temp_c":1,"charge_high_temp_c":41,"charge_low_temp_c":1,"temp_enabled":false}}

// {"command":"advance","battery":{"cuv":2701,"eoc":4201,"eoc_protect":4351},"cedv":{"cedv0":2561,"cedv1":2571,"cedv2":2581,"discharge_cedv0":4151,"discharge_cedv10":4051,"discharge_cedv20":4001,"discharge_cedv30":3901,"discharge_cedv40":3851,"discharge_cedv50":3801,"discharge_cedv60":3651,"discharge_cedv70":3551,"discharge_cedv80":3501,"discharge_cedv90":3301,"discharge_cedv100":2561}}

// {"command":"status","soc_gauge":0,"charge_current":0,"total_voltage":"15.688","learned_capacity":"3.237","pack_voltage":"12821","fet_enable":true,"protection_sta":"00000001","cells":[{"cell_num":1,"temperature":-53.44998932,"voltage":3.931000233},{"cell_num":2,"temperature":-53.44998932,"voltage":3.929000139},{"cell_num":3,"temperature":-53.44998932,"voltage":3.934000254}]}

// {"command":"sync","times":3}

// {"command":"rsp","status":true}

// {"command":"reset"}


void loop() {
    static uint32_t cnt = 0; 
    String json = "";
    cnt++;

    if(listenString(json, '\n')) {
        LOG_D("%s", json.c_str());
        bool res = parseJsonCommand(json.c_str(), &meshsolar.cmd); // Parse the command from input
        if (res) {
            // printMeshsolarCmd(&g_bat_cmd);
            /*  add some func call back here base on cmd sector */
            if (0 == strcmp(meshsolar.cmd.command, "config")) {
                bool results[5] = {false};
                log_i("\r\n");
                LOG_W("Updating basic battery configuration...");

                // Execute all configuration methods first
                results[0] = meshsolar.update_basic_bat_type_setting();
                results[1] = meshsolar.update_basic_bat_cells_setting();
                results[2] = meshsolar.update_basic_bat_design_capacity_setting();
                results[3] = meshsolar.update_basic_bat_discharge_cutoff_voltage_setting();
                results[4] = meshsolar.update_basic_bat_temp_protection_setting();
                
                // Print the results table after all executions
                log_i("\r\n");
                log_i("\r\n");
                LOG_I("+------------------------------------------------------+");
                LOG_I("|       Basic Battery Configuration Update             |");
                LOG_I("+------------------------------------------------------+");
                LOG_I("| Setting                      | Status                |");
                LOG_I("+------------------------------+-----------------------+");
                LOG_I("| Battery Type                 | %-21s |", results[0] ? "Success" : "Failed");
                LOG_I("| Battery Cells                | %-21s |", results[1] ? "Success" : "Failed");
                LOG_I("| Design Capacity              | %-21s |", results[2] ? "Success" : "Failed");
                LOG_I("| Discharge Cutoff Voltage     | %-21s |", results[3] ? "Success" : "Failed");
                LOG_I("| Temperature Protection       | %-21s |", results[4] ? "Success" : "Failed");
                LOG_I("+------------------------------+-----------------------+");
                
                // Check if all configurations succeeded
                bool allSuccess = true;
                for (int i = 0; i < 5; i++) {
                    if (!results[i]) {
                        allSuccess = false;
                        break;
                    }
                }
                if (allSuccess) {
                    LOG_I("| Overall Status: Configuration completed successfully |");
                } else {
                    LOG_I("| Overall Status: Some configurations failed          |");
                }
                LOG_I("+------------------------------------------------------+");

                //sync the basic battery configuration immediately
                meshsolar.get_basic_bat_realtime_setting();     
                meshsolarBasicConfigToJson(&meshsolar.sync_rsp.basic, json); // Get the basic battery settings
                comSerial.println(json); // Send the configuration back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Basic configuration sync completed");

                // Respond with the updated basic configuration
                meshsolarCmdRspToJson(allSuccess, json); // Create a response JSON
                comSerial.println(json); // Send the response back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Basic configuration response sent");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "advance")) {
                bool results[2] = {false};
                log_i("\r\n");
                LOG_W("Updating advanced battery configuration...");
                
                // Execute all configuration methods first
                results[0] = meshsolar.update_advance_bat_battery_setting();
                results[1] = meshsolar.update_advance_bat_cedv_setting();
                
                // Print the results table after all executions
                LOG_I("+------------------------------------------------------+");
                LOG_I("|      Advanced Battery Configuration Update           |");
                LOG_I("+------------------------------------------------------+");
                LOG_I("| Setting                      | Status                |");
                LOG_I("+------------------------------+-----------------------+");
                LOG_I("| Advanced Battery Settings    | %-21s |", results[0] ? "Success" : "Failed");
                LOG_I("| CEDV Settings                | %-21s |", results[1] ? "Success" : "Failed");
                LOG_I("+------------------------------+-----------------------+");
                
                // Check if all configurations succeeded
                bool allSuccess = true;
                for (int i = 0; i < 2; i++) {
                    if (!results[i]) {
                        allSuccess = false;
                        break;
                    }
                }
                if (allSuccess) {
                    LOG_I("| Overall Status: Configuration completed successfully |");
                } else {
                    LOG_I("| Overall Status: Some configurations failed          |");
                }
                LOG_I("+------------------------------------------------------+");

                //respond with the updated advanced configuration
                meshsolar.get_advance_bat_realtime_setting();
                meshsolarAdvanceConfigToJson(&meshsolar.sync_rsp.advance, json); // Get the advanced battery settings
                comSerial.println(json); // Send the configuration back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Advanced configuration sync");

                // Respond with the updated basic configuration
                meshsolarCmdRspToJson(allSuccess, json); // Create a response JSON
                comSerial.println(json); // Send the response back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Advance configuration response sent");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "switch")) {
                meshsolar.toggle_fet(); 
                LOG_I("FET Toggle...");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "reset")) {
                meshsolar.reset_bat_gauge();      
                LOG_I("Resetting BQ4050...");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "sync")) {
                size_t len = 0;
                for(uint8_t i = 0; i < meshsolar.cmd.sync.times; i++) {
                    len = meshsolarBasicConfigToJson(&meshsolar.sync_rsp.basic, json); // Get the basic battery settings
                    if(len > 0) {
                        comSerial.println(json); // Send the configuration back to the serial port
                        delay(10); // Small delay to avoid flooding the serial output
                        LOG_D("%s", json.c_str());
                    }

                    len = meshsolarAdvanceConfigToJson(&meshsolar.sync_rsp.advance, json); // Get the advanced battery settings
                    if(len > 0) {
                        comSerial.println(json); // Send the configuration back to the serial port
                        delay(10); // Small delay to avoid flooding the serial output
                        LOG_D("%s", json.c_str());
                    }
                }
                LOG_I("Sync data sent %d times.", meshsolar.cmd.sync.times);
            }
            else{
                LOG_E("Unknown command: %s", meshsolar.cmd.command);
            }
        } else {
            LOG_E("Failed to parse command");
        }
    }


#if 0 // debugging section
    if(0 == cnt % 1000) {
        bq4050_block_t block = {0,0,nullptr}; // Initialize block structure
        DAStatus1_t da1 = {0,};
        block.cmd = MAC_CMD_DA_STATUS1; // Command to read cell voltage
        block.len = 32;                 

        if(bq4050.read_mac_block(&block)){
            memcpy(&da1, block.pvalue, sizeof(DAStatus1_t)); // Copy the data into the da1 structure
            dbg::hex_print((uint8_t*)&da1, sizeof(DAStatus1_t),"DAStatus1");
        } 
        delay(10); 



    //     DAStatus2_t da2 = {0,};
    //     block.cmd = MAC_CMD_DA_STATUS2; // Command to read cell voltage
    //     block.len = 14;                 

    //    if(bq4050.read_mac_block(&block)){
    //         memcpy(&da2, block.pvalue, sizeof(DAStatus2_t)); // Copy the data into the da1 structure
    //         dbg::hex_print((uint8_t*)&da2, sizeof(DAStatus2_t),"DAStatus2");
    //     } 
    //     delay(10); 

    }
#endif


#if 1
    if(0 == cnt % 1000) {
        meshsolar.get_realtime_bat_status();            // Update the battery status
        meshsolar.get_basic_bat_realtime_setting();     // Update the battery configuration
        meshsolar.get_advance_bat_realtime_setting();   // Update the battery advanced configuration
        LOG_I("================================================");
        LOG_I("Status soc_gauge: %d%%", meshsolar.sta.soc_gauge);
        LOG_I("Status pack_voltage: %d mV", meshsolar.sta.pack_voltage);
        LOG_I("Status charge_current: %d mA", meshsolar.sta.charge_current);
        LOG_I("Status total_voltage: %.0f mV", meshsolar.sta.total_voltage);
        LOG_I("Status learned_capacity: %.0f mAh", meshsolar.sta.learned_capacity);
        LOG_I("Status bat pack: %s", meshsolar.sta.fet_enable ? "On" : "Off");
        LOG_I("Protect Status: %s", meshsolar.sta.protection_sta);
    }
#endif


#if 1
    if(0 == cnt % 1000){
        meshsolarStatusToJson(&meshsolar.sta, json);
        LOG_L("Status JSON: %s", json.c_str());
        comSerial.println(json);
        delay(10); // Small delay to avoid flooding the serial output
    }
#endif

    delay(1);
}
