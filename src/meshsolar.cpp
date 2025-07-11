#include "bq4050.h"
#include "meshsolar.h"
#include "logger.h"

MeshSolar::MeshSolar(){
    this->_bq4050 = nullptr; // Initialize pointer to null
    memset(&this->sta, 0, sizeof(this->sta)); // Initialize status structure to zero
    memset(&this->cmd, 0, sizeof(this->cmd)); // Initialize command structure to zero
    this->cmd.basic.cell_number = 4; // Default to 4 cells
    this->cmd.basic.design_capacity = 3200; // Default design capacity in m
    this->cmd.basic.discharge_cutoff_voltage = 2800; // Default cutoff voltage in mV
    strlcpy(this->cmd.basic.type, "lifepo4", sizeof(this->cmd.basic.type)); // Default battery type
    this->cmd.basic.protection.charge_high_temp_c         = 60; // Default high temperature threshold
    this->cmd.basic.protection.charge_low_temp_c          = -10; // Default low temperature threshold
    this->cmd.basic.protection.discharge_high_temp_c      = 60; // Default discharge
    this->cmd.basic.protection.discharge_low_temp_c       = -10; // Default discharge low temperature threshold
    this->cmd.basic.protection.enabled                    = true; // Default temperature protection enabled
    strlcpy(this->cmd.command, "config", sizeof(this->cmd.command)); // Default command type
    this->cmd.fet_en.enable = false; // Default FET enable status
    this->cmd.sync.times    = 1; // Default sync times
}

MeshSolar::~MeshSolar(){
    if (this->_bq4050) {
        this->_bq4050->~BQ4050(); // Call destructor to clean up
        this->_bq4050 = nullptr;  // Set pointer to null after cleanup
    }
}

void MeshSolar::begin(BQ4050 *device) {
    this->_bq4050 = device;
}

bool MeshSolar::get_realtime_bat_status(){
    bool res = true;
    bq4050_reg_t reg = {0,0};             // Initialize register structure
    bq4050_block_t block = {0,0,nullptr}; // Initialize block structure
    // /********************************************** get pack total voltage *********************************************/
    // reg.addr = BQ4050_REG_VOLT; // Register address for total voltage
    // res &= this->_bq4050->read_reg_word(&reg);
    // this->sta.total_voltage  = (res) ? reg.value : this->sta.total_voltage; // Convert from mV to V
    // delay(10); 
    /************************************************ get charge current ***********************************************/
    reg.addr = BQ4050_REG_CURRENT; // Register address for charge current
    res &= (int16_t)this->_bq4050->read_reg_word(&reg);
    this->sta.charge_current = (res) ? reg.value : this->sta.charge_current; // Convert from mA to A
    delay(10); 
    LOG_L("Charge current: %d mA", this->sta.charge_current); // Log charge current
    /*************************************************** get soc gauge ************************************************/
    reg.addr = BQ4050_REG_RSOC; // Register address for state of charge
    res &= this->_bq4050->read_reg_word(&reg); 
    this->sta.soc_gauge = (res) ? reg.value : this->sta.soc_gauge; // Read state of charge
    delay(10); 
    LOG_L("State of charge: %d %%", this->sta.soc_gauge); // Log state of charge
    /*************************************************** get bat cells ************************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration again
    block.pvalue[0] = (block.pvalue[0] & 0b00000011);        // Mask to get only the last 2 bits for DA configuration
    block.pvalue[0] = (block.pvalue[0] > 3) ? 3 : block.pvalue[0];        // Ensure DA value is within valid range (0-3)
    this->sta.cell_count = block.pvalue[0] + 1; // Set cell count based on DA configuration (0-3 corresponds to 1-4 cells)
    LOG_L("Cell count: %d", this->sta.cell_count); // Log cell count
    /**************************************************** get cell temp ***********************************************/
    DAStatus2_t da2 = {0,};
    block.cmd = MAC_CMD_DA_STATUS2; // Command to read cell temperatures
    block.len = 14;                 // 14 bytes for cell temperatures
    this->_bq4050->read_mac_block(&block); // Read the data block from the BQ4050
    memcpy(&da2, block.pvalue, sizeof(DAStatus2_t)); // Copy the data into the da2 structure
    delay(10); 

    this->sta.cells[0].temperature =  da2.ts1_temp / 10.0f - 273.15f;// Convert from Kelvin to Celsius
    this->sta.cells[1].temperature =  da2.ts2_temp / 10.0f - 273.15f;
    this->sta.cells[2].temperature =  da2.ts3_temp / 10.0f - 273.15f;
    this->sta.cells[3].temperature =  da2.ts4_temp / 10.0f - 273.15f;
    for(int i = 0; i < this->sta.cell_count; i++) {
        LOG_L("Cell %d temperature: %.2f °C", i + 1, this->sta.cells[i].temperature); // Log cell temperatures
    }
    /**************************************************** get cell voltage *********************************************/
    DAStatus1_t da1 = {0,};
    block.cmd = MAC_CMD_DA_STATUS1; // Command to read cell temperatures
    block.len = 32;                 // 14 bytes for cell temperatures
    this->_bq4050->read_mac_block(&block); // Read the data block from the BQ4050
    memcpy(&da1, block.pvalue, sizeof(DAStatus1_t)); // Copy the data into the da1 structure
    delay(10); 
    this->sta.cells[0].cell_num = 1;
    this->sta.cells[0].voltage  = da1.cell_1_voltage; 
    this->sta.cells[1].cell_num = 2;
    this->sta.cells[1].voltage  = da1.cell_2_voltage; 
    this->sta.cells[2].cell_num = 3;
    this->sta.cells[2].voltage  = da1.cell_3_voltage; 
    this->sta.cells[3].cell_num = 4;
    this->sta.cells[3].voltage  = da1.cell_4_voltage; 
    this->sta.total_voltage = da1.bat_voltage; // Use bat pin voltage as total voltage
    for(int i = 0; i < 4 ; i++) {
        LOG_L("Cell %d voltage: %.2f V", this->sta.cells[i].cell_num, this->sta.cells[i].voltage / 1000.0f); // Log cell voltages
    }
    LOG_L("Total voltage: %.2f V", this->sta.total_voltage / 1000.0f); // Log total voltage
    /**************************************************** get charge voltage ********************************************/
    this->sta.pack_voltage = da1.pack_voltage;    // Use pack voltage as charge voltage
    LOG_L("Charge voltage: %d mV", this->sta.charge_voltage); // Log charge voltage
    /**************************************************** get full charge capacity **************************************/
    reg.addr = BQ4050_REG_FCC; 
    res  &= this->_bq4050->read_reg_word(&reg);
    this->sta.learned_capacity = (res) ? reg.value : this->sta.learned_capacity; 
    delay(10); 
    LOG_L("Learned capacity: %.2f Ah", this->sta.learned_capacity / 1000.0f); // Log learned capacity in Ah
    /**************************************************** get fet enable state ******************************************/
    block.cmd = MAC_CMD_MANUFACTURER_STATUS;        // Command to read manufacturer status
    block.len = 2;                 
    res &= this->_bq4050->read_mac_block(&block); 
    this->sta.fet_enable = (res) ? (*(uint16_t*)block.pvalue & 0x0010) != 0 : this->sta.fet_enable; 
    /**************************************************** get protection status **************************************/
    SafetyStatus_t safety_status = {0,};
    block.cmd = MAC_CMD_SAFETY_STATUS; // Command to read safety status
    block.len = 4;                     // Length of the data block to read
    res &= this->_bq4050->read_mac_block(&block); // Read the data block from the BQ4050
    memcpy(&safety_status, block.pvalue, sizeof(SafetyStatus_t)); // Copy the data into the safety_status structure
    
    // Convert 4-byte SafetyStatus to hex string (8 characters + null terminator)
    // Format as big-endian hex string: MSB first 
    snprintf(this->sta.protection_sta, sizeof(this->sta.protection_sta), "%08X", (unsigned int)safety_status.bytes);
    LOG_L("Protection status: %s", this->sta.protection_sta); // Log protection status in hex format

    return res; // Return true to indicate status update was successful
}

bool MeshSolar::get_basic_bat_realtime_setting(){
    bq4050_block_t block= {
        .cmd = DF_CMD_SBS_DATA_CHEMISTRY,
        .len = 5,
        .pvalue =nullptr,
        .type = STRING
    };
    /*****************************************   bat type   *************************************/
    memset(this->sync_rsp.basic.type, 0, sizeof(this->sync_rsp.basic.type)); // Clear the battery type string
    this->_bq4050->read_dataflash_block(&block); 
    if(0 == strcasecmp((const char *)block.pvalue, "LFE4")) {
        strlcpy(this->sync_rsp.basic.type, "lifepo4", sizeof(this->sync_rsp.basic.type)); // Copy battery type to sync response structure
    }
    else if(0 == strcasecmp((const char *)block.pvalue, "LION")) {
        strlcpy(this->sync_rsp.basic.type, "liion", sizeof(this->sync_rsp.basic.type)); // Copy battery type to sync response structure
    }
    else if(0 == strcasecmp((const char *)block.pvalue, "LIPO")) {
        strlcpy(this->sync_rsp.basic.type, "lipo", sizeof(this->sync_rsp.basic.type)); // Copy battery type to sync response structure
    }
    else {
        LOG_E("Unknown battery type from BQ4050: %s", (const char *)block.pvalue);
        return false; // Unknown battery type, return false
    }
    /*****************************************  cell count  *************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration again
    block.pvalue[0] = (block.pvalue[0] & 0b00000011);        // Mask to get only the last 2 bits for DA configuration
    block.pvalue[0] = (block.pvalue[0] > 3) ? 3 : block.pvalue[0];  // Ensure DA value is within valid range (0-3)
    this->sync_rsp.basic.cell_number = block.pvalue[0] + 1;   // Set cell count based on DA configuration (0-3 corresponds to 1-4 cells)
    /*****************************************  design capacity  *************************************/
    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH; // Command to access design capacity
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read design capacity
    this->sync_rsp.basic.design_capacity = (block.pvalue[1] << 8) | block.pvalue[0]; // Combine bytes to get capacity in mAh
    /*****************************************  cutoff voltage  *************************************/
    block.cmd = DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR; // Command to access cutoff voltage
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read cutoff voltage
    this->sync_rsp.basic.discharge_cutoff_voltage = (block.pvalue[1] << 8) | block.pvalue[0]; // Combine bytes to get voltage in mV
    /*****************************************  temperature protection charge high temp  *************************************/
    block.cmd = DF_CMD_PROTECTIONS_OTC_THR; // Command to access temperature protection thresholds
    block.len = 2;
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read temperature protection thresholds
    int16_t raw_temp = (int16_t)((block.pvalue[1] << 8) | block.pvalue[0]); // Read as signed 16-bit value
    this->sync_rsp.basic.protection.charge_high_temp_c = raw_temp / 10.0f; // Convert from 0.1°C to Celsius
    /*****************************************  temperature protection charge low temp  *************************************/
    block.cmd = DF_CMD_PROTECTIONS_UTC_THR; // Command to access charge low temperature threshold
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read charge low temperature threshold
    raw_temp = (int16_t)((block.pvalue[1] << 8) | block.pvalue[0]); // Read as signed 16-bit value
    this->sync_rsp.basic.protection.charge_low_temp_c = raw_temp / 10.0f; // Convert from 0.1°C to Celsius
    /*****************************************  temperature protection discharge high temp  *************************************/
    block.cmd = DF_CMD_PROTECTIONS_OTD_THR; // Command to access discharge high temperature threshold
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read discharge high temperature threshold
    raw_temp = (int16_t)((block.pvalue[1] << 8) | block.pvalue[0]); // Read as signed 16-bit value
    this->sync_rsp.basic.protection.discharge_high_temp_c = raw_temp / 10.0f; // Convert from 0.1°C to Celsius
    /*****************************************  temperature protection discharge low temp  *************************************/
    block.cmd = DF_CMD_PROTECTIONS_UTD_THR; // Command to access discharge low temperature threshold
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read discharge low temperature threshold
    raw_temp = (int16_t)((block.pvalue[1] << 8) | block.pvalue[0]); // Read as signed 16-bit value
    this->sync_rsp.basic.protection.discharge_low_temp_c = raw_temp / 10.0f; // Convert from 0.1°C to Celsius
    /*****************************************  temperature protection enabled  *************************************/
    // Read temperature protection enable status from both Protection Enable registers
    // Temperature protection is considered enabled only if all required bits are set in both registers
    
    bool protection_b_enabled = false, protection_d_enabled = false;
    
    // Read Protection Enable B register (controls OTC: bit 5, OTD: bit 4)
    block.cmd = DF_CMD_SETTINGS_PROTECTIONS_ENABLE_B;
    block.len = 1;
    block.type = NUMBER;
    if (this->_bq4050->read_dataflash_block(&block)) {
        const uint8_t PROTECTION_B_TEMP_MASK = 0b00110000; // Bits 4 and 5 (OTD and OTC)
        protection_b_enabled = ((block.pvalue[0] & PROTECTION_B_TEMP_MASK) == PROTECTION_B_TEMP_MASK);
        LOG_D("Protection Enable B: 0x%02X, temp bits enabled: %s", block.pvalue[0], protection_b_enabled ? "Yes" : "No");
    } else {
        LOG_E("Failed to read Protection Enable B register");
    }
    
    // Read Protection Enable D register (controls UTC: bit 3, UTD: bit 2)
    block.cmd = DF_CMD_SETTINGS_PROTECTIONS_ENABLE_D;
    block.len = 1;
    block.type = NUMBER;
    if (this->_bq4050->read_dataflash_block(&block)) {
        const uint8_t PROTECTION_D_TEMP_MASK = 0b00001100; // Bits 2 and 3 (UTD and UTC)
        protection_d_enabled = ((block.pvalue[0] & PROTECTION_D_TEMP_MASK) == PROTECTION_D_TEMP_MASK);
        LOG_D("Protection Enable D: 0x%02X, temp bits enabled: %s", block.pvalue[0], protection_d_enabled ? "Yes" : "No");
    } else {
        LOG_E("Failed to read Protection Enable D register");
    }
    // Temperature protection is enabled only if both registers have the required bits set
    this->sync_rsp.basic.protection.enabled = protection_b_enabled && protection_d_enabled;
    LOG_D("Temperature protection overall status: %s", this->sync_rsp.basic.protection.enabled ? "ENABLED" : "DISABLED");

    return true; // Return false to indicate configuration update was successful
}

bool MeshSolar::get_advance_bat_realtime_setting(){
    bq4050_block_t block = {0, 0, nullptr, NUMBER};
    
    /*
     * Read advanced battery configuration from BQ4050
     * 
     * This function reads the advanced configuration parameters that correspond
     * to the settings configured in update_advance_bat_battery_setting() and
     * update_advance_bat_cedv_setting() functions.
     */
    
    /*****************************************  CUV (Cell Under Voltage) Protection  *************************************/
    block.cmd = DF_CMD_PROTECTIONS_CUV_THR;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read CUV threshold");
        return false;
    }
    this->sync_rsp.advance.battery.cuv = (block.pvalue[1] << 8) | block.pvalue[0];
    LOG_D("CUV threshold: %d mV", this->sync_rsp.advance.battery.cuv);
    
    /*****************************************  EOC (End of Charge) Voltage  *************************************/
    // Read from standard temperature charge voltage (represents EOC setting)
    block.cmd = DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read EOC voltage");
        return false;
    }
    this->sync_rsp.advance.battery.eoc = (block.pvalue[1] << 8) | block.pvalue[0];
    LOG_L("EOC voltage: %d mV", this->sync_rsp.advance.battery.eoc);
    
    /*****************************************  EOC Protection Voltage  *************************************/
    // Read from standard temperature COV threshold (represents EOC protection setting)
    block.cmd = DF_CMD_PROTECTIONS_COV_STD_TEMP_THR;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read EOC protection voltage");
        return false;
    }
    this->sync_rsp.advance.battery.eoc_protect = (block.pvalue[1] << 8) | block.pvalue[0];
    LOG_L("EOC protection voltage: %d mV", this->sync_rsp.advance.battery.eoc_protect);
    
    /*****************************************  CEDV Fixed Values  *************************************/
    // Read CEDV fixed EDV0
    block.cmd = DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read CEDV0");
        return false;
    }
    this->sync_rsp.advance.cedv.cedv0 = (block.pvalue[1] << 8) | block.pvalue[0];
    
    // Read CEDV fixed EDV1
    block.cmd = DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read CEDV1");
        return false;
    }
    this->sync_rsp.advance.cedv.cedv1 = (block.pvalue[1] << 8) | block.pvalue[0];
    
    // Read CEDV fixed EDV2
    block.cmd = DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2;
    block.len = 2;
    block.type = NUMBER;
    if (!this->_bq4050->read_dataflash_block(&block)) {
        LOG_E("Failed to read CEDV2");
        return false;
    }
    this->sync_rsp.advance.cedv.cedv2 = (block.pvalue[1] << 8) | block.pvalue[0];
    
    /*****************************************  CEDV Discharge Profile Values  *************************************/
    // Configuration table for CEDV discharge profile readings
    struct cedv_read_entry_t {
        uint16_t cmd;
        int* target_field;
        const char* name;
    };
    
    cedv_read_entry_t cedv_readings[] = {
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0,   &this->sync_rsp.advance.cedv.discharge_cedv0,   "Discharge CEDV 0%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10,  &this->sync_rsp.advance.cedv.discharge_cedv10,  "Discharge CEDV 10%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20,  &this->sync_rsp.advance.cedv.discharge_cedv20,  "Discharge CEDV 20%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30,  &this->sync_rsp.advance.cedv.discharge_cedv30,  "Discharge CEDV 30%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40,  &this->sync_rsp.advance.cedv.discharge_cedv40,  "Discharge CEDV 40%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50,  &this->sync_rsp.advance.cedv.discharge_cedv50,  "Discharge CEDV 50%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60,  &this->sync_rsp.advance.cedv.discharge_cedv60,  "Discharge CEDV 60%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70,  &this->sync_rsp.advance.cedv.discharge_cedv70,  "Discharge CEDV 70%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80,  &this->sync_rsp.advance.cedv.discharge_cedv80,  "Discharge CEDV 80%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90,  &this->sync_rsp.advance.cedv.discharge_cedv90,  "Discharge CEDV 90%"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100, &this->sync_rsp.advance.cedv.discharge_cedv100, "Discharge CEDV 100%"}
    };
    
    // Helper lambda to read CEDV values
    auto read_cedv_value = [&](uint16_t cmd, int* target, const char* name) -> bool {
        block.cmd = cmd;
        block.len = 2;
        block.type = NUMBER;
        if (!this->_bq4050->read_dataflash_block(&block)) {
            LOG_E("Failed to read %s", name);
            return false;
        }
        *target = (block.pvalue[1] << 8) | block.pvalue[0];
        delay(50); // Add a small delay to avoid flooding the bus
        return true;
    };
    
    // Read all CEDV discharge profile values
    for (auto& entry : cedv_readings) {
        if (!read_cedv_value(entry.cmd, entry.target_field, entry.name)) {
            return false;
        }
    }
    return true;
}

bool MeshSolar::update_basic_bat_type_setting(){ 
    bool res = true;

    /*
     * Temperature-compensated voltage configuration for battery charging and protection
     * 
     * Why different voltages for different temperatures?
     * 1. Low temperature (-20°C to 0°C): 
     *    - Battery internal resistance increases, requiring lower voltages
     *    - Risk of lithium plating if charged too aggressively
     *    - Conservative voltages prevent damage and improve safety
     * 
     * 2. Standard temperature (0°C to 45°C):
     *    - Optimal operating range with nominal voltages
     *    - Best balance of charging speed and battery life
     * 
     * 3. High temperature (45°C to 60°C):
     *    - Reduced voltages prevent thermal runaway
     *    - Lower thresholds protect against accelerated aging
     *    - Critical for safety in hot environments
     * 
     * 4. Recovery temperature:
     *    - Intermediate voltages for transitioning between states
     *    - Helps prevent oscillation between protection modes
     */

    // Define temperature-specific voltage configuration
    struct temp_voltage_t {
        uint16_t low_temp;     // Low temperature (-20°C to 0°C) - Conservative voltages
        uint16_t std_temp;     // Standard temperature (0°C to 45°C) - Nominal voltages
        uint16_t high_temp;    // High temperature (45°C to 60°C) - Reduced for safety
        uint16_t rec_temp;     // Recovery/recommended temperature - Transition values
    };

    // Define complete battery voltage configuration
    struct battery_voltage_config_t {
        temp_voltage_t charge_voltage;   // Charge voltages for different temperatures
        temp_voltage_t cov_threshold;    // Charge over-voltage thresholds
        temp_voltage_t cov_recovery;     // Charge over-voltage recovery voltages
    } config;

    // Configuration table: {command, value, description}
    struct config_entry_t {
        uint16_t cmd;
        uint16_t value;
        const char* name;
    };

    const char *type = nullptr;
    // Set voltage parameters based on battery type with temperature compensation
    if(0 == strcasecmp(this->cmd.basic.type, "lifepo4")) {
        // LiFePO4 (Lithium Iron Phosphate) characteristics:
        // - More stable across temperatures than Li-ion
        // - Lower nominal voltage (3.2V vs 3.7V for Li-ion)
        // - Very flat discharge curve
        // - Inherently safer chemistry but still needs temperature protection
        config = {
            .charge_voltage = {3600, 3600, 3600, 3600},  // Conservative at temp extremes
            .cov_threshold  = {3750, 3750, 3750, 3750},  // Safety margins, esp. at extremes
            .cov_recovery   = {3600, 3600, 3600, 3600}   // Match charge voltages for stability
        };
        type = "LFE4"; // Set battery type to LiFePO4 , bq4050 uses 4 character to store chemistry name
    }
    else if((0 == strcasecmp(this->cmd.basic.type, "lipo"))) {
        // Li-ion/LiPo (Lithium Cobalt/Polymer) characteristics:
        // - Higher energy density but more temperature-sensitive
        // - Higher nominal voltage (3.7V) and charge voltage (4.2V)
        // - More aggressive temperature derating required for safety
        // - Risk of thermal runaway at high temperatures
        config = {
            .charge_voltage = {4200, 4200, 4200, 4200},  // Significant reduction at extremes
            .cov_threshold  = {4300, 4300, 4300, 4300},  // Conservative safety margins
            .cov_recovery   = {4100, 4100, 4100, 4100}   // Lower recovery for safety
        };
        type = "LIPO"; // Set battery type to LiPo , bq4050 uses 4 character to store chemistry name
    }
    else if((0 == strcasecmp(this->cmd.basic.type, "liion"))) {
        // Li-ion/LiPo (Lithium Cobalt/Polymer) characteristics:
        // - Higher energy density but more temperature-sensitive
        // - Higher nominal voltage (3.7V) and charge voltage (4.2V)
        // - More aggressive temperature derating required for safety
        // - Risk of thermal runaway at high temperatures
        config = {
            .charge_voltage = {4200, 4200, 4200, 4200},  // Significant reduction at extremes
            .cov_threshold  = {4300, 4300, 4300, 4300},  // Conservative safety margins
            .cov_recovery   = {4100, 4100, 4100, 4100}   // Lower recovery for safety
        };
        type = "LION"; // Set battery type to Li-ion , bq4050 uses 4 character to store chemistry name
    }
    else {
        // dbgSerial.println("Unknown battery type, exit!!!!!!!");
        LOG_E("Unknown battery type, exit!!!!!!!");
        return false;
    }

    config_entry_t configurations[] = {
        // Advanced charge algorithm voltages
        {DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL,  config.charge_voltage.low_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL "},
        {DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL,  config.charge_voltage.std_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL "},
        {DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL, config.charge_voltage.high_temp, "DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL"},
        {DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL,  config.charge_voltage.rec_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL "},
        
        // Protection COV thresholds
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR,  config.cov_threshold.low_temp,             "DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR           "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_THR,  config.cov_threshold.std_temp,             "DF_CMD_PROTECTIONS_COV_STD_TEMP_THR           "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR, config.cov_threshold.high_temp,            "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR          "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_THR,  config.cov_threshold.rec_temp,             "DF_CMD_PROTECTIONS_COV_REC_TEMP_THR           "},
        
        // Protection COV recovery
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY,  config.cov_recovery.low_temp,         "DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY      "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY,  config.cov_recovery.std_temp,         "DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY      "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY, config.cov_recovery.high_temp,        "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY     "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY,  config.cov_recovery.rec_temp,         "DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY      "},
    };


    // Helper lambda function to write and verify voltage setting
    auto write_and_verify = [&](uint16_t cmd, uint16_t value, const char* name) -> bool {
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};

        // Write the value
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to read back %s", name);
            return false;
        }

        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        if(value == read_value) {
            LOG_I("%s set to: %d mV - OK", name, read_value);
            return true; // Return true if the value matches
        }
        else{
            LOG_E("%s set to: %d mV - ERROR (expected %d mV)", name, read_value, value);
            return false; // Return false if the value does not match
        }

        return (value == read_value);
    };


    // Apply all configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify(cfg.cmd, cfg.value, cfg.name);
    }

    /*****************************************   bat type   *************************************/
    bq4050_block_t block = {0, 0, nullptr, STRING}; // Declare and initialize block structure
    block.cmd = DF_CMD_SBS_DATA_CHEMISTRY; // Command to access battery chemistry
    block.len = 5; // Length of the data block to read
    block.type = STRING; // Set block type to STRING
    block.pvalue = (uint8_t*)malloc(block.len + 1); // Allocate memory for the value
    if (block.pvalue == nullptr) {
        LOG_E("Memory allocation failed for block.pvalue");
        free(block.pvalue); // Free memory if allocation fails
        return false; // Return false if memory allocation fails
    }
    memset(block.pvalue, 0, block.len); // Initialize the value to zero
    block.pvalue[0] = strlen(type); // Set the first byte to the length of the battery type string
    strlcpy((char *)block.pvalue + 1, type, block.len); // Copy the battery type string into the block value
    this->_bq4050->write_dataflash_block(block); // Write the battery type
    free(block.pvalue); // Free the allocated memory for block.pvalue
    block.pvalue = nullptr; // Set pointer to null after freeing memory


    delay(100); // Ensure the write is complete before reading
    bq4050_block_t ret = {0, 0, nullptr, STRING}; // Reset block structure for reading
    ret.cmd = block.cmd; // Command to access battery chemistry
    ret.len = block.len; // Length of the data block to read
    ret.type = block.type; // Set block type to STRING
    this->_bq4050->read_dataflash_block(&ret); // Read the battery type back from data flash
    if(0 == strcasecmp((const char *)(ret.pvalue), type)) {
        LOG_I("DF_CMD_SBS_DATA_CHEMISTRY set to: %s - OK", (const char *)(ret.pvalue)); // Log success
    }
    else {
        LOG_E("DF_CMD_SBS_DATA_CHEMISTRY set to: %s - ERROR", (const char *)(ret.pvalue)); // Log error
        res = false; // If the read value does not match, set result to false
    }

    return res;
}

bool MeshSolar::update_basic_bat_model_setting() {

    return false;
}

bool MeshSolar::update_basic_bat_cells_setting() {
    bool res = true;

    // Get cell voltage based on battery type
    uint16_t cell_voltage_mv = 0;
    if(0 == strcasecmp(this->cmd.basic.type, "lifepo4")) {
        cell_voltage_mv = 3600;
    }
    else if((0 == strcasecmp(this->cmd.basic.type, "liion")) || (0 == strcasecmp(this->cmd.basic.type, "lipo"))){
        cell_voltage_mv = 4200;
    }
    else {
        LOG_E("Unknown battery type, exit!!!!!!!");
        return false;
    }

    /******************************************Configure DA Configuration (Cell Count)**************************************/ 
    {
        bq4050_block_t block = {DF_CMD_DA_CONFIGURATION, 1, nullptr, NUMBER};
        
        // Read current DA configuration
        if (!this->_bq4050->read_dataflash_block(&block)) {
            LOG_E("Failed to read DA configuration");
            return false;
        }
        delay(100);

        // Calculate cell count bits (0-3 for 1-4 cells)
        uint8_t cells_bits = (this->cmd.basic.cell_number > 4) ? 3 : (this->cmd.basic.cell_number - 1);
        
        // Clear the last 2 bits first, then set new cell count bits
        block.pvalue[0] &= 0b11111100; // Clear bits 0 and 1
        block.pvalue[0] |= cells_bits;  // Set new cell count bits
        
        // Write the modified configuration
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write DA configuration");
            return false;
        }
        delay(100);

        // Read back and verify
        bq4050_block_t ret = {DF_CMD_DA_CONFIGURATION, 1, nullptr, NUMBER};
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to verify DA configuration");
            return false;
        }

        LOG_L("DF_CMD_DA_CONFIGURATION after: 0x%02X", ret.pvalue[0]);
        res &= (ret.pvalue[0] == block.pvalue[0]);
    }

    /*********************************************************Configure Design Voltage***************************************/
    {
        uint16_t total_voltage = this->cmd.basic.cell_number * cell_voltage_mv;
        bq4050_block_t block = {DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV, 2, (uint8_t*)&total_voltage, NUMBER};
        
        // Write design voltage
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write design voltage");
            return false;
        }
        delay(100);

        // Read back and verify
        bq4050_block_t ret = {DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV, 2, nullptr, NUMBER};
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to verify design voltage");
            return false;
        }

        uint16_t read_voltage = ret.pvalue[0] | (ret.pvalue[1] << 8);
        LOG_I("DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV after: %d mV", read_voltage);
        res &= (read_voltage == total_voltage);
    }

    return res; 
}

bool MeshSolar::update_basic_bat_design_capacity_setting(){
    // Get cell voltage based on battery type
    float cell_voltage = 0.0f;
    bool res = true;

    if(0 == strcasecmp(this->cmd.basic.type, "lifepo4")) {
        cell_voltage = 3.6f;
    }
    else if((0 == strcasecmp(this->cmd.basic.type, "liion")) || (0 == strcasecmp(this->cmd.basic.type, "lipo"))){
        cell_voltage = 4.2f;
    }
    else {
        LOG_E("Unknown battery type, exit!!!!!!!");
        return false;
    }

    /*******************************************************Design Capacity mAh*******************************************/
    {
        uint16_t capacity = this->cmd.basic.design_capacity;
        bq4050_block_t block = {DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH, 2, (uint8_t*)(&capacity), NUMBER};
        this->_bq4050->write_dataflash_block(block);
        delay(100);

        bq4050_block_t ret = {DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH, 2, nullptr, NUMBER};
        this->_bq4050->read_dataflash_block(&ret);
        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        LOG_I("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH after: %d mAh", read_value);
        res &= (capacity == read_value);
    }

    /*******************************************************Design Capacity cWh******************************************/
    {
        uint16_t capacity = static_cast<uint16_t>(this->cmd.basic.cell_number * cell_voltage * this->cmd.basic.design_capacity / 10.0f);
        bq4050_block_t block = {DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH, 2, (uint8_t*)(&capacity), NUMBER};
        this->_bq4050->write_dataflash_block(block);
        delay(100);

        bq4050_block_t ret = {DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH, 2, nullptr, NUMBER};
        this->_bq4050->read_dataflash_block(&ret);
        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        LOG_I("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH after: %d cWh", read_value);
        res &= (capacity == read_value);
    }

    /*******************************************************Learned Full Charge Capacity mAh*****************************/
    {
        uint16_t capacity = this->cmd.basic.design_capacity;
        bq4050_block_t block = {DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY, 2, (uint8_t*)(&capacity), NUMBER};
        this->_bq4050->write_dataflash_block(block);
        delay(100);

        bq4050_block_t ret = {DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY, 2, nullptr, NUMBER};
        this->_bq4050->read_dataflash_block(&ret);
        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        LOG_I("DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY after: %d mAh", read_value);
        res &= (capacity == read_value);
    }

    return res;
}

bool MeshSolar::update_basic_bat_discharge_cutoff_voltage_setting(){
    bool res = true;
    
    /*
     * Discharge cutoff voltage configuration for battery protection and gas gauging
     * 
     * Voltage levels explained:
     * - FD/TD Set: Final/Terminate Discharge voltage (lowest safe voltage)
     * - FD/TD Clear: Recovery voltage (voltage to resume normal operation)
     * - EDV0/1/2: End of Discharge Voltage levels (stepped discharge warnings)
     * - CUV Threshold: Cell Under Voltage protection (hardware cutoff)
     * - CUV Recovery: Recovery voltage to re-enable after CUV protection
     * 
     * These settings work together to provide multi-level protection during discharge.
     */

    // Define base voltages from configuration
    uint16_t cutoff_base = this->cmd.basic.discharge_cutoff_voltage;

    // Configuration table: {command, voltage_offset, description}
    struct cutoff_config_entry_t {
        uint16_t cmd;
        int16_t  offest;  // Offset from base cutoff voltage (can be negative)
        const char* name;
    };


    // Helper lambda function to write and verify voltage setting
    auto write_and_verify = [&](uint16_t cmd, uint16_t value, const char* name) -> bool {
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};

        // Write the value
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to read back %s", name);
            return false;
        }

        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        if (value == read_value) {
            LOG_I("%s set to: %d mV - OK", name, read_value); // Log success
            return true;
        } else {
            LOG_E("%s set to: %d mV - ERROR (expected %d mV)", name, read_value, value); // Log error
            return false;
        }
    };
    cutoff_config_entry_t configurations[] = {
        // Gas Gauging - Final Discharge (FD) thresholds
        {DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR,   0,    "FD Set Voltage Threshold                      "},      // Lowest discharge voltage
        {DF_CMD_GAS_GAUGE_FD_CLEAR_VOLTAGE_THR, 100,  "FD Clear Voltage Threshold                    "},    // Recovery voltage (+100mV)
        
        // Gas Gauging - Terminate Discharge (TD) thresholds  
        {DF_CMD_GAS_GAUGE_TD_SET_VOLTAGE_THR,   0,    "TD Set Voltage Threshold                      "},      // Same as FD set
        {DF_CMD_GAS_GAUGE_TD_CLEAR_VOLTAGE_THR, 100,  "TD Clear Voltage Threshold                    "},    // Recovery voltage (+100mV)
        
        // Gas Gauging - End Discharge Voltage (EDV) stepped warnings
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0,  0,    "CEDV Fixed EDV0                               "},              // First warning level
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1,  20,   "CEDV Fixed EDV1                               "},              // Second warning (+20mV)
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2,  30,   "CEDV Fixed EDV2                               "},              // Third warning (+30mV)
        
        // Protection - Cell Under Voltage (CUV) hardware protection
        {DF_CMD_PROTECTIONS_CUV_THR,           -50,   "CUV Protection Threshold                      "},      // Hardware cutoff (-50mV for safety margin)
        {DF_CMD_PROTECTIONS_CUV_RECOVERY,      100,   "CUV Recovery Voltage                          "}           // Recovery to re-enable (+100mV)
    };

    // Apply all discharge cutoff configurations
    for(auto& cfg : configurations) {
        uint16_t target_voltage = cutoff_base + cfg.offest;
        res &= write_and_verify(cfg.cmd, target_voltage, cfg.name);
    }

    return res;
}

bool MeshSolar::update_basic_bat_temp_protection_setting() {
    bool res = true;
    
    /*
     * Temperature protection configuration for battery safety
     * 
     * Temperature thresholds explained:
     * - OTC (Over Temperature Charge): High temperature protection during charging
     * - OTD (Over Temperature Discharge): High temperature protection during discharge  
     * - UTC (Under Temperature Charge): Low temperature protection during charging
     * - UTD (Under Temperature Discharge): Low temperature protection during discharge
     * - Recovery: Temperature to resume normal operation after protection
     * 
     * Each protection has threshold and recovery values to prevent oscillation
     */

    // Define temperature protection structure
    struct temp_protection_t {
        int16_t otc_threshold;    // Over Temperature Charge threshold (0.1°C units)
        int16_t otc_recovery;     // Over Temperature Charge recovery (0.1°C units)
        int16_t utc_threshold;    // Under Temperature Charge threshold (0.1°C units)
        int16_t utc_recovery;     // Under Temperature Charge recovery (0.1°C units)
        int16_t otd_threshold;    // Over Temperature Discharge threshold (0.1°C units)
        int16_t otd_recovery;     // Over Temperature Discharge recovery (0.1°C units)
        int16_t utd_threshold;    // Under Temperature Discharge threshold (0.1°C units)
        int16_t utd_recovery;     // Under Temperature Discharge recovery (0.1°C units)
    } config;

    // Configuration table: {command, value, description}
    struct config_entry_t {
        uint16_t cmd;
        int16_t value;
        const char* name;
    };

    // Get temperature values from configuration and validate
    float charge_high    = this->cmd.basic.protection.charge_high_temp_c;
    float charge_low     = this->cmd.basic.protection.charge_low_temp_c;
    float discharge_high = this->cmd.basic.protection.discharge_high_temp_c;
    float discharge_low  = this->cmd.basic.protection.discharge_low_temp_c;

    // Validate temperature ranges (basic sanity check)
    if (charge_low >= charge_high || discharge_low >= discharge_high) {
        LOG_E("ERROR: Invalid temperature ranges in configuration");
        LOG_E("  Charge: %.1f°C to %.1f°C", charge_low, charge_high);
        LOG_E("  Discharge: %.1f°C to %.1f°C", discharge_low, discharge_high);
        return false;
    }

    // Set temperature protection parameters with hysteresis
    // Convert to BQ4050 format (0.1°C units) and apply 5°C hysteresis
    config = {
        .otc_threshold = (int16_t)(charge_high * 10),           // Charge over temperature threshold
        .otc_recovery  = (int16_t)((charge_high - 5) * 10),     // 5°C lower for recovery
        .utc_threshold = (int16_t)(charge_low * 10),            // Charge under temperature threshold
        .utc_recovery  = (int16_t)((charge_low + 5) * 10),      // 5°C higher for recovery
        .otd_threshold = (int16_t)(discharge_high * 10),        // Discharge over temperature threshold
        .otd_recovery  = (int16_t)((discharge_high - 5) * 10),  // 5°C lower for recovery
        .utd_threshold = (int16_t)(discharge_low * 10),         // Discharge under temperature threshold
        .utd_recovery  = (int16_t)((discharge_low + 5) * 10)    // 5°C higher for recovery
    };

    config_entry_t configurations[] = {
        // Charge temperature protections
        {DF_CMD_PROTECTIONS_OTC_THR,      config.otc_threshold, "DF_CMD_PROTECTIONS_OTC_THR                    "},
        {DF_CMD_PROTECTIONS_OTC_RECOVERY, config.otc_recovery,  "DF_CMD_PROTECTIONS_OTC_RECOVERY               "},
        {DF_CMD_PROTECTIONS_UTC_THR,      config.utc_threshold, "DF_CMD_PROTECTIONS_UTC_THR                    "},
        {DF_CMD_PROTECTIONS_UTC_RECOVERY, config.utc_recovery,  "DF_CMD_PROTECTIONS_UTC_RECOVERY               "},
        
        // Discharge temperature protections
        {DF_CMD_PROTECTIONS_OTD_THR,      config.otd_threshold, "DF_CMD_PROTECTIONS_OTD_THR                    "},
        {DF_CMD_PROTECTIONS_OTD_RECOVERY, config.otd_recovery,  "DF_CMD_PROTECTIONS_OTD_RECOVERY               "},
        {DF_CMD_PROTECTIONS_UTD_THR,      config.utd_threshold, "DF_CMD_PROTECTIONS_UTD_THR                    "},
        {DF_CMD_PROTECTIONS_UTD_RECOVERY, config.utd_recovery,  "DF_CMD_PROTECTIONS_UTD_RECOVERY               "}
    };

    // Helper lambda function to write and verify temperature setting
    auto write_and_verify = [&](uint16_t cmd, int16_t value, const char* name) -> bool {
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};

        // Write the value
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to read back %s", name);
            return false;
        }

        int16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        float temp_celsius = read_value / 10.0f;  // Convert from 0.1°C units to °C
        if (value == read_value) {
            LOG_I("%s set to: %.1f°C - OK", name, temp_celsius); // Log success
            return true;
        } else {
            float expected_celsius = value / 10.0f;  // Convert expected value to °C
            LOG_E("%s set to: %.1f°C - ERROR (expected %.1f°C)", name, temp_celsius, expected_celsius); // Log error
            return false;
        }
    };

    // Apply all temperature protection configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify(cfg.cmd, cfg.value, cfg.name);
    }

    /****************************************** protection enable/disable ******************************************/
    // Configure temperature protection enable/disable for both registers:
    // Protection Enable B - controls charge temperature protections:
    //   Bit 4: OTD (Over Temperature Discharge) protection enable
    //   Bit 5: OTC (Over Temperature Charge) protection enable
    // Protection Enable D - controls discharge temperature protections:
    //   Bit 2: UTD (Under Temperature Discharge) protection enable  
    //   Bit 3: UTC (Under Temperature Charge) protection enable
    
    // Helper lambda to configure protection enable bits
    auto configure_protection_register = [&](uint16_t cmd, uint8_t bit_mask, 
                                             const char* reg_name, const char* bit_desc) -> bool {
        bq4050_block_t block = {cmd, 1, nullptr, NUMBER};
        
        // Read current register value
        if (!this->_bq4050->read_dataflash_block(&block)) {
            // dbgSerial.print("Failed to read ");
            // dbgSerial.println(reg_name);
            LOG_E("Failed to read %s", reg_name);
            return false;
        }
        
        uint8_t register_value = block.pvalue[0];
        
        if (this->cmd.basic.protection.enabled) {
            // Enable temperature protection: set specified bits
            register_value |= bit_mask;
            LOG_I("Temperature protection enabled in %s (%s set)", reg_name, bit_desc);
        } else {
            // Disable temperature protection: clear specified bits
            register_value &= ~bit_mask;
            LOG_I("Temperature protection disabled in %s (%s cleared)", reg_name, bit_desc);
        }
        
        // Write the modified register value back to the device
        block.pvalue[0] = register_value;
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", reg_name);
            return false;
        }
        delay(100);
        
        // Read back and verify the setting
        bq4050_block_t verify_block = {cmd, 1, nullptr, NUMBER};
        if (!this->_bq4050->read_dataflash_block(&verify_block)) {
            LOG_E("Failed to verify %s", reg_name);
            return false;
        }
        
        uint8_t verified_value = verify_block.pvalue[0];
        LOG_I("%s after: 0x%02X", reg_name, verified_value); // Log the register value
        
        // Check if bits are set correctly
        bool bits_match_expected = ((verified_value & bit_mask) != 0) == this->cmd.basic.protection.enabled;
        
        if (bits_match_expected) {
            LOG_I("%s temperature protection bits verified - OK", reg_name); // Log success
            return true;
        } else {
            LOG_E("%s temperature protection bits verification failed - ERROR", reg_name); // Log error
            LOG_E("  Expected: %s, Actual bits (%s): %s", 
                  this->cmd.basic.protection.enabled ? "enabled" : "disabled",
                  bit_desc,
                  (verified_value & bit_mask) != 0 ? "set" : "clear");

            return false;
        }
    };
    
    // Configure Protection Enable B register (OTC: bit 5, OTD: bit 4)
    const uint8_t PROTECTION_B_TEMP_MASK = 0b00110000; // Bits 4 and 5 (OTD and OTC)
    res &= configure_protection_register(DF_CMD_SETTINGS_PROTECTIONS_ENABLE_B, 
                                        PROTECTION_B_TEMP_MASK,
                                        "Protection Enable B", 
                                        "bits 4,5 OTD/OTC");
    
    // Configure Protection Enable D register (UTC: bit 3, UTD: bit 2)  
    const uint8_t PROTECTION_D_TEMP_MASK = 0b00001100; // Bits 2 and 3 (UTD and UTC)
    res &= configure_protection_register(DF_CMD_SETTINGS_PROTECTIONS_ENABLE_D,
                                        PROTECTION_D_TEMP_MASK,
                                        "Protection Enable D",
                                        "bits 2,3 UTD/UTC");


    return res;
}

bool MeshSolar::update_advance_bat_battery_setting() {
    bool res = true;
    /*
     * Advanced battery configuration for BQ4050
     * 
     * This function sets advanced charge algorithm voltages and protection thresholds
     * based on the advance command configuration.
     * 
     * It configures:
     * - CUV (Cell Under Voltage) protection threshold and recovery
     * - Charge voltages for different temperature ranges (EOC - End Of Charge)
     * - Charge over-voltage protection thresholds
     */

    // Configuration table: {command, value, description}
    struct config_entry_t {
        uint16_t cmd;
        uint16_t value;
        const char* name;
    };

    // Define configurations using advance command parameters
    config_entry_t configurations[] = {
        // CUV protection settings
        {DF_CMD_PROTECTIONS_CUV_THR,                          (uint16_t)this->cmd.advance.battery.cuv,                                   "DF_CMD_PROTECTIONS_CUV_THR                         "},
        {DF_CMD_PROTECTIONS_CUV_RECOVERY,                     (uint16_t)(this->cmd.advance.battery.cuv + 100),                           "DF_CMD_PROTECTIONS_CUV_RECOVERY                    "},
        // Advanced charge algorithm - EOC voltages for all temperature ranges
        {DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL,       (uint16_t)this->cmd.advance.battery.eoc,                                   "DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL      "},
        {DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL,       (uint16_t)this->cmd.advance.battery.eoc,                                   "DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL      "},
        {DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL,      (uint16_t)this->cmd.advance.battery.eoc,                                   "DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL     "},
        {DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL,       (uint16_t)this->cmd.advance.battery.eoc,                                   "DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL      "},
        // Charge over-voltage protection thresholds for all temperature ranges
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR                "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_STD_TEMP_THR                "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR,                (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR               "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_REC_TEMP_THR                "},
        // Protection COV thresholds settings
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR                "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_STD_TEMP_THR                "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR,                (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR               "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_THR,                 (uint16_t)this->cmd.advance.battery.eoc_protect,                           "DF_CMD_PROTECTIONS_COV_REC_TEMP_THR                "},
        // Protection COV recovery settings
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY,            (uint16_t)(this->cmd.advance.battery.eoc_protect - 100),                   "DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY           "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY,            (uint16_t)(this->cmd.advance.battery.eoc_protect - 100),                   "DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY           "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY,           (uint16_t)(this->cmd.advance.battery.eoc_protect - 100),                   "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY          "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY,            (uint16_t)(this->cmd.advance.battery.eoc_protect - 100),                   "DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY           "}
    };

    // Helper lambda function to write and verify voltage setting
    auto write_and_verify = [&](uint16_t cmd, uint16_t value, const char* name) -> bool {
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};

        // Write the value
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to read back %s", name);
            return false;
        }

        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        if (value == read_value) {
            LOG_I("%s set to: %d mV - OK", name, read_value);
            return true;
        } else {
            LOG_E("%s set to: %d mV - ERROR (expected %d mV)", name, read_value, value);
            return false;
        }
    };

    // Apply all configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify(cfg.cmd, cfg.value, cfg.name);
    }

    return res;
}

bool MeshSolar::update_advance_bat_cedv_setting(){
    bool res = true; // Initialize result variable

    /*
     * CEDV (Constant Energy Discharge Voltage) configuration for BQ4050
     * 
     * This function sets CEDV parameters based on the advance command configuration.
     * 
     * It configures:
     * - CEDV fixed EDV0/1/2 voltages
     * - CEDV voltage thresholds
     */

    // Configuration table: {command, value, description}
    struct config_entry_t {
        uint16_t cmd;
        uint16_t value;
        const char* name;
    };

    // Define configurations using advance command parameters
    config_entry_t configurations[] = {
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0,       (uint16_t)this->cmd.advance.cedv.cedv0,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0               "},
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1,       (uint16_t)this->cmd.advance.cedv.cedv1,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1               "},
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2,       (uint16_t)this->cmd.advance.cedv.cedv2,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2               "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0,   (uint16_t)this->cmd.advance.cedv.discharge_cedv0,   "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0           "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10,  (uint16_t)this->cmd.advance.cedv.discharge_cedv10,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20,  (uint16_t)this->cmd.advance.cedv.discharge_cedv20,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30,  (uint16_t)this->cmd.advance.cedv.discharge_cedv30,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40,  (uint16_t)this->cmd.advance.cedv.discharge_cedv40,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50,  (uint16_t)this->cmd.advance.cedv.discharge_cedv50,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60,  (uint16_t)this->cmd.advance.cedv.discharge_cedv60,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70,  (uint16_t)this->cmd.advance.cedv.discharge_cedv70,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80,  (uint16_t)this->cmd.advance.cedv.discharge_cedv80,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90,  (uint16_t)this->cmd.advance.cedv.discharge_cedv90,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90          "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100, (uint16_t)this->cmd.advance.cedv.discharge_cedv100, "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100         "},
    };


    // Helper lambda function to write and verify voltage setting
    auto write_and_verify = [&](uint16_t cmd, uint16_t value, const char* name) -> bool {
        // Create a block structure for writing and reading data flash  
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};
        // Write the value to data flash
        if (!this->_bq4050->write_dataflash_block(block)) {
            LOG_E("Failed to write %s", name);
            return false;
        }
        delay(100); // Ensure the write is complete before reading
        // Read back the value from data flash
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            LOG_E("Failed to read back %s", name);
            return false;
        }
        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8); // Combine two bytes into a single value
        if (value == read_value) {
            LOG_I("%s set to: %d mV - OK", name, read_value); // Log success
            return true;
        } else {
            LOG_E("%s set to: %d mV - ERROR (expected %d mV)", name, read_value, value); // Log error
            return false;
        }
    };
    // Apply all configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify(cfg.cmd, cfg.value, cfg.name);
    }
    return res; // Return the result of all configurations
}

bool MeshSolar::toggle_fet(){
    return this->_bq4050->fet_toggle(); // Call the BQ4050 method to toggle FETs
}


bool MeshSolar::reset_bat_gauge() {
    return this->_bq4050->reset(); // Call the BQ4050 method to reset the device
}
