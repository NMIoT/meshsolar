#include "meshSolarApp.h"

#define MESHSOLAR_VERSION  "v1.0"

/*
 * ============================================================================
 * PORTING GUIDE - Critical Configuration Points
 * ============================================================================
 * 
 * 1. SERIAL PORT CONFIGURATION:
 *    - comSerial: Primary communication port for JSON command/response
 *    - Serial2: Debug logging port (115200 baud)
 *    - Modify serial port assignments based on target platform:
 *      * ESP32: Serial, Serial1, Serial2
 *      * Arduino Uno: Serial, SoftwareSerial
 *      * STM32: Serial, Serial1, Serial2, etc.
 * 
 * 2. I2C PIN CONFIGURATION:
 *    - SDA_PIN: I2C data line connected to BQ4050 SDA
 *    - SCL_PIN: I2C clock line connected to BQ4050 SCL
 *    - Default pins are for nRF52840, modify for your hardware
 * 
 * 3. PLATFORM-SPECIFIC REQUIREMENTS:
 *    - nRF52840: Uses g_ADigitalPinMap[] for pin mapping
 *    - Other platforms: Use direct pin numbers
 *    - Some platforms may require different I2C libraries
 * 
 * 4. GLOBAL OBJECT INITIALIZATION ORDER (CRITICAL!):
 *    Wire -> bq4050 -> meshsolar
 *    This order must be maintained for proper initialization
 * 
 * 5. MEMORY REQUIREMENTS:
 *    - JSON buffer: 1024 bytes for command parsing
 *    - Status buffer: 512 bytes for status/config serialization
 *    - Ensure sufficient RAM on target platform
 * 
 * 6. TIMING CONSIDERATIONS:
 *    - Main loop runs every 1ms
 *    - Status updates every 1000 iterations (1 second)
 *    - BQ4050 I2C operations require delay(10-100ms) between calls
 * 
 * 7. DEPENDENCIES:
 *    - ArduinoJson library (version 6.x)
 *    - Platform-specific USB/Serial libraries
 *    - Custom logger.h for debugging output
 */

// Serial port definitions - MODIFY FOR YOUR PLATFORM
#define comSerial           Serial      // Primary communication port
// Platform examples:
// ESP32: Serial, Serial1, Serial2
// Arduino: Serial, SoftwareSerial
// STM32: Serial, Serial1, etc.

// I2C pin definitions - MODIFY FOR YOUR HARDWARE
#define SDA_PIN                         33          // I2C data line pin
#define SCL_PIN                         32          // I2C clock line pin
#define RGB_LED_PIN                     47          // RGB LED data line pin
#define EMERGENCY_SHUTDOWN_PIN          35          // Emergency shutdown pin
// Common pin assignments:
// ESP32: GPIO 21 (SDA), GPIO 22 (SCL)
// Arduino Uno: A4 (SDA), A5 (SCL)
// nRF52840: Any GPIO pin

// Global object declarations - INITIALIZATION ORDER IS CRITICAL!
// For nRF52840: Uses g_ADigitalPinMap[] for pin mapping
// For other platforms: Use direct pin numbers like SoftwareWire Wire(SDA_PIN, SCL_PIN);
SoftwareWire      SoftWire( g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
Adafruit_NeoPixel strip(1, g_ADigitalPinMap[RGB_LED_PIN], NEO_GRBW + NEO_KHZ800);
BQ4050       bq4050;               // BQ4050 instance
MeshSolar    meshsolar;            // Main MeshSolar controller object   

/*
 * ============================================================================
 * UTILITY FUNCTIONS - Required for system operation
 * ============================================================================
 */

/**
 * @brief Listen for incoming string data on serial port
 * @param input Reference to string buffer for received data
 * @param terminator Character that indicates end of message (default: '\n')
 * @return true if complete message received, false otherwise
 * 
 * PORTING NOTES:
 * - Uses comSerial.available() and comSerial.read()
 * - Ensure your platform's Serial implementation supports these methods
 * - For non-blocking operation, this function should be called frequently
 */
static bool listenString(String& input, char terminator = '\n') {
    while (comSerial.available() > 0) {
        char c = comSerial.read();
        if (c == terminator) return true;   
         else input += c;                   
    }
    return false; 
}

/**
 * @brief Parse incoming JSON command and populate command structure
 * @param json JSON string to parse
 * @param cmd Pointer to command structure to populate
 * @return true if parsing successful, false otherwise
 * 
 * SUPPORTED COMMANDS:
 * - "config": Basic battery configuration
 * - "advance": Advanced battery settings
 * - "switch": FET control
 * - "reset": Battery gauge reset
 * - "sync": Synchronize settings
 * - "status": Get current status (no parameters)
 * 
 * PORTING NOTES:
 * - Requires ArduinoJson library (version 6.x)
 * - Uses StaticJsonDocument<1024> - ensure sufficient RAM
 * - All string operations use safe strlcpy() for buffer protection
 */
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
    else if (strcmp(cmd->command, "renew") == 0) {
        // No additional fields required for renew command
    }
    else {
        LOG_E("Unknown command '%s'", cmd->command);
        return false;
    }

    return true;
}

/*
 * ============================================================================
 * JSON SERIALIZATION FUNCTIONS - Data output formatting (snake_case naming)
 * ============================================================================
 */

/**
 * @brief Convert battery status to JSON format
 * @param status Pointer to battery status structure
 * @param output Reference to output string
 * @return Size of serialized JSON
 * 
 * OUTPUT FORMAT:
 * {
 *   "command": "status",
 *   "soc_gauge": 85,
 *   "charge_current": -1200,
 *   "total_voltage": "12.345",
 *   "learned_capacity": "3.200",
 *   "pack_voltage": "12345",
 *   "fet_enable": true,
 *   "protection_sta": "CUV,COV",
 *   "emergency_shutdown": false,
 *   "cells": [
 *     {"cell_num": 1, "temperature": 25.123, "voltage": 3.234},
 *     ...
 *   ]
 * }
 * 
 * PORTING NOTES:
 * - Uses StaticJsonDocument<512> - ensure sufficient RAM
 * - Voltage/temperature values rounded to 3 decimal places
 * - Always outputs 4 cells regardless of actual cell count
 * - Function name follows snake_case convention for better readability
 */
size_t meshsolar_status_to_json(const meshsolar_status_t* status, String& output) {
    output = "";
    StaticJsonDocument<512> doc;
    doc["command"]          = "status";
    doc["soc_gauge"]        = status->soc_gauge;
    doc["charge_current"]   = status->charge_current;
    doc["total_voltage"]    = String(status->total_voltage/1000.0f, 3);
    doc["learned_capacity"] = String(status->learned_capacity /1000.0f, 3);
    doc["pack_voltage"]     = String(status->pack_voltage);
    doc["fet_enable"]       = status->fet_enable;
    doc["protection_sta"]   = String(status->protection_sta) + String((status->emergency_shutdown) ? ",EMSHUT" : "");
    // doc["emergency_shutdown"] = status->emergency_shutdown;
    
    JsonArray cells = doc.createNestedArray("cells");
    for (int i = 0; i < 4; ++i) {
        JsonObject cell     = cells.createNestedObject();
        cell["cell_num"]    = status->cells[i].cell_num;
        cell["temperature"] = round(status->cells[i].temperature * 1000)/1000.0f; // Round to 3 decimal places
        cell["voltage"]     = round((status->cells[i].voltage / 1000.0f) * 1000) / 1000.0f; // Round to 3 decimal places
    }
    output = "";
    return serializeJson(doc, output);
}


/**
 * @brief Convert basic battery configuration to JSON format
 * @param basic Pointer to basic configuration structure
 * @param output Reference to output string
 * @return Size of serialized JSON
 * 
 * FUNCTION: meshsolar_basic_config_to_json
 * - Serializes basic battery settings into JSON format
 * - Includes battery type, cell count, capacity, and temperature protection
 * - Used for configuration synchronization and validation
 */
size_t meshsolar_basic_config_to_json(const basic_config_t *basic, String& output) {
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


/**
 * @brief Convert advanced battery configuration to JSON format
 * @param config Pointer to advanced configuration structure  
 * @param output Reference to output string
 * @return Size of serialized JSON
 * 
 * FUNCTION: meshsolar_advance_config_to_json
 * - Serializes advanced battery settings including CEDV curves
 * - Contains cutoff voltages and discharge curve data points
 * - Critical for battery gauge calibration and performance tuning
 */
size_t meshsolar_advance_config_to_json(const advance_config_t *config, String& output) {
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


/**
 * @brief Create standardized command response JSON
 * @param status Boolean indicating command success/failure
 * @param output Reference to output string  
 * @return Size of serialized JSON
 * 
 * FUNCTION: meshsolar_cmd_rsp_to_json
 * - Generates consistent response format for all commands
 * - Simple true/false status indication
 * - Used for acknowledgment of configuration changes
 */
size_t meshsolar_cmd_rsp_to_json(bool status, String& output) {
    output = "";
    StaticJsonDocument<64> doc;
    doc["command"] = "rsp";
    doc["status"] = status;
    output = "";
    // Serialize the JSON document to the output string
    return serializeJson(doc, output);
}

/*
 * ============================================================================
 * MAIN PROGRAM FUNCTIONS
 * ============================================================================
 */

/**
 * @brief System initialization function
 * 
 * INITIALIZATION SEQUENCE (CRITICAL ORDER):
 * 1. Debug serial port (Serial2) - Optional, for logging
 * 2. Communication serial port (comSerial) - Required for JSON commands
 * 3. BQ4050 I2C interface initialization
 * 4. MeshSolar controller initialization
 * 
 * PORTING CHECKLIST:
 * □ Verify serial port assignments match your hardware
 * □ Confirm baud rates are supported by your platform
 * □ Check I2C pins are correctly mapped
 * □ Ensure BQ4050ADDR (0x0B) matches your hardware configuration
 * □ Verify sufficient power supply for BQ4050 operation
 * 
 * COMMON ISSUES:
 * - I2C communication failures: Check pin connections and pull-up resistors
 * - Serial port conflicts: Ensure ports don't conflict with programming interface
 * - Power issues: BQ4050 requires stable 3.3V supply
 */
static volatile SemaphoreHandle_t xMutex;

void meshSolarStart(void)
{

    pinMode(EMERGENCY_SHUTDOWN_PIN, OUTPUT);    // Set emergency shutdown pin as output

    digitalWrite(EMERGENCY_SHUTDOWN_PIN, HIGH); // Set emergency shutdown pin to HIGH (disabled)

    // Initialize debug serial port (optional)
    // MODIFY: Change to your platform's debug port or comment out if not needed
    // Serial2.begin(115200);
    
    // Initialize main communication serial port (REQUIRED)
    // MODIFY: Change to your platform's primary serial port
    comSerial.begin(115200);            
    
    // Initialize BQ4050 with I2C interface (REQUIRED)
    // VERIFY: Ensure Wire object is properly configured for your platform
    bq4050.begin(&SoftWire, BQ4050ADDR);    
    
    // Initialize MeshSolar controller (REQUIRED)
    meshsolar.begin(&bq4050);           
    
    // INITIALIZE NeoPixel strip object (REQUIRED)
    // strip.begin();     

    // Turn OFF all pixels ASAP
    // strip.show();       
    
    // Set brightness to 100% (0-255 range)
    // strip.setBrightness(100);

    // Set all pixels to black (off)
    // for (int i = 0; i < strip.numPixels(); i++) {
    //     strip.setPixelColor(i, strip.Color(0, 0, 0, 0)); // Set pixel to black
    // }
    xMutex = xSemaphoreCreateRecursiveMutex( );

    LOG_I("MeshSolar %s initialized successfully", MESHSOLAR_VERSION);
}

#define TRY_WRITE(RETRY_NUM, RETRY_INTERVAL, RESULT_VAR, EXPR) \
    do { \
        int _try_num; \
        for (_try_num = 0; _try_num < (RETRY_NUM); _try_num++) { \
            if ((RESULT_VAR = (EXPR))) { \
                break; \
            } \
            delay(RETRY_INTERVAL); \
        } \
    } while (0)

#define WRITE_TRY_NUM 1
#define WRITE_TRY_INTERVAL 100
#define READ_TRY_NUM 1
#define READ_TRY_INTERVAL 100

int meshSolarCmdHandle(const char *cmd)
{
    int result = 0;
    bool results[5] = {false};

    if((cmd==NULL) ||(xSemaphoreTake(xMutex, 100)!=pdTRUE))
    {
        return -1;
    }
    if (0 == strncmp(cmd,"{\"command\":\"renew\"}", strlen("{\"command\":\"renew\"}"))) 
    {
        bool res;
        TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[0], meshsolar.get_realtime_bat_status());
        TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[1], meshsolar.get_basic_bat_realtime_setting());
        TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[2], meshsolar.get_advance_bat_realtime_setting());
        xSemaphoreGive(xMutex);
        return 0;
    }
    String json = cmd;
    LOG_D(" JSON: %s", json.c_str());
    if(json.length() >  6) {
        bool res = parseJsonCommand(json.c_str(), &meshsolar.cmd);
        if (res) {
            /*
             * COMMAND HANDLERS
             * Each command type has specific processing requirements:
             * 
             * "config": Updates basic battery configuration (type, cells, capacity, etc.)
             * "advance": Updates advanced settings (CEDV, protection thresholds)
             * "switch": Controls FET enable/disable
             * "reset": Resets battery gauge learning data
             * "sync": Sends current configuration data multiple times
             * 
             * PORTING NOTES:
             * - All configuration changes are immediately written to BQ4050
             * - Operations may take 100-500ms due to I2C flash writes
             * - Responses are sent immediately after completion
             * - Consider implementing timeout mechanisms for production use
             */
            if (0 == strcmp(meshsolar.cmd.command, "config")) {
                log_i("\r\n");
                LOG_W("Updating basic battery configuration...");

                // Execute all configuration methods first

                TRY_WRITE(WRITE_TRY_NUM, WRITE_TRY_INTERVAL, results[0], meshsolar.update_basic_bat_type_setting());
                TRY_WRITE(WRITE_TRY_NUM, WRITE_TRY_INTERVAL, results[1], meshsolar.update_basic_bat_cells_setting());
                TRY_WRITE(WRITE_TRY_NUM, WRITE_TRY_INTERVAL, results[2], meshsolar.update_basic_bat_design_capacity_setting());
                TRY_WRITE(WRITE_TRY_NUM, WRITE_TRY_INTERVAL, results[3], meshsolar.update_basic_bat_discharge_cutoff_voltage_setting());
                TRY_WRITE(WRITE_TRY_NUM, WRITE_TRY_INTERVAL, results[4], meshsolar.update_basic_bat_temp_protection_setting());
                
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

                //sync the basic battery configuration immediately
                meshsolar.get_basic_bat_realtime_setting();
                meshsolar_basic_config_to_json(&meshsolar.sync_rsp.basic, json); // Get the basic battery settings
                comSerial.println(json); // Send the configuration back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Basic configuration sync completed");

                bool allSuccess = results[0] && results[1] && results[2] && results[3] && results[4];
                // Respond with the updated basic configuration
                meshsolar_cmd_rsp_to_json(allSuccess, json); // Create a response JSON
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
                
                //respond with the updated advanced configuration
                meshsolar.get_advance_bat_realtime_setting();
                meshsolar_advance_config_to_json(&meshsolar.sync_rsp.advance, json); // Get the advanced battery settings
                comSerial.println(json); // Send the configuration back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Advanced configuration sync");

                // Respond with the updated advanced configuration
                bool allSuccess = results[0] && results[1];
                meshsolar_cmd_rsp_to_json(allSuccess, json); // Create a response JSON
                comSerial.println(json); // Send the response back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Advanced configuration response sent");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "switch")) {
                res = meshsolar.toggle_fet(); 
                LOG_I("FET Toggle...");

                // Respond with the FET toggle result
                meshsolar_cmd_rsp_to_json(res, json); // Create a response JSON
                comSerial.println(json); // Send the response back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("FET toggle response sent");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "reset")) {
                res =  meshsolar.reset_bat_gauge();      
                LOG_I("Resetting BQ4050...");

                // Respond with the reset result
                meshsolar_cmd_rsp_to_json(res, json); // Create a response JSON
                comSerial.println(json); // Send the response back to the serial port
                delay(10); // Small delay to avoid flooding the serial output
                LOG_I("Reset response sent");
            }
            else if (0 == strcmp(meshsolar.cmd.command, "sync")) {
                size_t len = 0;
                TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[0], meshsolar.get_realtime_bat_status());
                TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[1], meshsolar.get_basic_bat_realtime_setting());
                TRY_WRITE(READ_TRY_NUM, READ_TRY_INTERVAL, results[2], meshsolar.get_advance_bat_realtime_setting());    
                len = meshsolar_status_to_json(&meshsolar.sta, json);
                if(len > 0) {
                    comSerial.println(json); // Send the configuration back to the serial port
                    delay(10); // Small delay to avoid flooding the serial output
                    LOG_D("%s", json.c_str());
                }
                for(uint8_t i = 0; i < meshsolar.cmd.sync.times; i++) {
                    len = meshsolar_basic_config_to_json(&meshsolar.sync_rsp.basic, json); // Get the basic battery settings
                    if(len > 0) {
                        comSerial.println(json); // Send the configuration back to the serial port
                        delay(10); // Small delay to avoid flooding the serial output
                        LOG_D("%s", json.c_str());
                    }

                    len = meshsolar_advance_config_to_json(&meshsolar.sync_rsp.advance, json); // Get the advanced battery settings
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
                result = -3;
            }
        } else {
            LOG_E("Failed to parse command");
            result = -2;
        }
    }
    else
    {
        LOG_E("The length is too short, the command is invalid.");
        result = -1;
    }
    xSemaphoreGive(xMutex);
    return result;
}


    /**
     * Battery state of charge, from 0 to 100 or -1 for unknown
     */
uint32_t lastRenewTime = 0;
#define RENEW_INTERVAL 10000
    int meshSolarGetBatteryPercent()  {
        if((millis()-lastRenewTime) > RENEW_INTERVAL)
        {
            meshSolarCmdHandle("{\"command\":\"renew\"}");
            lastRenewTime = millis();
        }
        return (int)meshsolar.sta.soc_gauge; 
    }

    /**
     * The raw voltage of the battery in millivolts, or NAN if unknown
     */
     uint16_t meshSolarGetBattVoltage()  { 
        if((millis()-lastRenewTime) > RENEW_INTERVAL)
        {
            meshSolarCmdHandle("{\"command\":\"renew\"}");
            lastRenewTime = millis();
        }
        return (uint16_t)meshsolar.sta.total_voltage;
    }

    /**
     * return true if there is a battery installed in this unit
     */
     bool meshSolarIsBatteryConnect()  {
        return true;
    }
    /**
     * return true if there is an external power source detected
     */
     bool meshSolarIsVbusIn()  {
        if((millis()-lastRenewTime) > RENEW_INTERVAL)
        {
            meshSolarCmdHandle("{\"command\":\"renew\"}");
            lastRenewTime = millis();
        }
        return (meshsolar.sta.charge_current>0)? true:false;
    }
    /**
     * return true if the battery is currently charging
     */
     bool meshSolarIsCharging()  {
        if((millis()-lastRenewTime) > RENEW_INTERVAL)
        {
            meshSolarCmdHandle("{\"command\":\"renew\"}");
            lastRenewTime = millis();
        }
        return (meshsolar.sta.charge_current>0)? true:false;
    }
/*
 * ============================================================================
 * PORTING CHECKLIST - Verify these items for successful port
 * ============================================================================
 * 
 * HARDWARE REQUIREMENTS:
 * □ MCU with sufficient RAM (>8KB recommended)
 * □ I2C interface capability
 * □ At least 2 serial ports (1 for commands, 1 for debug)
 * □ Stable 3.3V power supply for BQ4050
 * □ I2C pull-up resistors (4.7kΩ typical)
 * 
 * SOFTWARE REQUIREMENTS:
 * □ Arduino framework or compatible environment
 * □ ArduinoJson library (version 6.x)
 * □ Platform-specific I2C library
 * □ Serial/USB communication support
 * 
 * CONFIGURATION CHECKLIST:
 * □ Serial port assignments match your hardware
 * □ I2C pin definitions are correct
 * □ Baud rates are supported by your platform
 * □ BQ4050 I2C address (0x0B) is accessible
 * □ Pin mapping functions work on your platform
 * 
 * TESTING PROCEDURE:
 * 1. Verify serial communication (send/receive test strings)
 * 2. Test I2C communication (read BQ4050 firmware version)
 * 3. Send basic JSON commands and verify responses
 * 4. Monitor status updates for reasonable values
 * 5. Test all command types (config, advance, switch, reset, sync)
 * 
 * COMMON TROUBLESHOOTING:
 * - "I2C timeout": Check wiring, pull-ups, power supply
 * - "JSON parse error": Verify command format and buffer sizes
 * - "No serial response": Check port assignments and baud rates
 * - "Invalid battery data": Verify BQ4050 configuration and connections
 * 
 * PERFORMANCE OPTIMIZATION:
 * - Reduce status update frequency for battery-powered applications
 * - Implement command queuing for high-frequency operations
 * - Use hardware I2C instead of software I2C if available
 * - Consider DMA for serial operations on supported platforms
 */
