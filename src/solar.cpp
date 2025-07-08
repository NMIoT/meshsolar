#include "bq4050.h"
#include "solar.h"
#include "logger.h"

MeshSolar::MeshSolar(){
    this->_bq4050 = nullptr; // Initialize pointer to null
    memset(&this->sta, 0, sizeof(this->sta)); // Initialize status structure to zero
    memset(&this->cmd, 0, sizeof(this->cmd)); // Initialize command structure to zero
    this->cmd.basic.cell_number = 4; // Default to 4 cells
    this->cmd.basic.design_capacity = 3200; // Default design capacity in m
    this->cmd.basic.discharge_cutoff_voltage = 2800; // Default cutoff voltage in mV
    strlcpy(this->cmd.basic.bat_type, "lifepo4", sizeof(this->cmd.basic.bat_type)); // Default battery type
    this->cmd.basic.protection.charge_high_temp_c         = 60; // Default high temperature threshold
    this->cmd.basic.protection.charge_low_temp_c          = -10; // Default low temperature threshold
    this->cmd.basic.protection.discharge_high_temp_c      = 60; // Default discharge
    this->cmd.basic.protection.discharge_low_temp_c       = -10; // Default discharge low temperature threshold
    this->cmd.basic.protection.enabled                    = true; // Default temperature protection enabled
    strlcpy(this->cmd.command, "config", sizeof(this->cmd.command)); // Default command type
    this->cmd.fet_en = false; // Default FET switch state
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

bool MeshSolar::get_bat_realtime_status(){
    bool res = false;
    bq4050_reg_t reg = {0,0};             // Initialize register structure
    bq4050_block_t block = {0,0,nullptr}; // Initialize block structure
    /********************************************** get pack total voltage *********************************************/
    reg.addr = BQ4050_REG_VOLT; // Register address for total voltage
    res = this->_bq4050->read_reg_word(&reg);
    this->sta.total_voltage  = (res) ? reg.value : this->sta.total_voltage; // Convert from mV to V
    delay(10); 
    /************************************************ get charge current ***********************************************/
    reg.addr = BQ4050_REG_CURRENT; // Register address for charge current
    res = (int16_t)this->_bq4050->read_reg_word(&reg);
    this->sta.charge_current = (res) ? reg.value : this->sta.charge_current; // Convert from mA to A
    delay(10); 
    /*************************************************** get soc gauge ************************************************/
    reg.addr = BQ4050_REG_RSOC; // Register address for state of charge
    res = this->_bq4050->read_reg_word(&reg); 
    this->sta.soc_gauge = (res) ? reg.value : this->sta.soc_gauge; // Read state of charge
    delay(10); 
    /*************************************************** get bat cells ************************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration again
    block.pvalue[0] = (block.pvalue[0] & 0b00000011);        // Mask to get only the last 2 bits for DA configuration
    block.pvalue[0] = (block.pvalue[0] > 3) ? 3 : block.pvalue[0];        // Ensure DA value is within valid range (0-3)
    this->sta.cell_count = block.pvalue[0] + 1; // Set cell count based on DA configuration (0-3 corresponds to 1-4 cells)
    /**************************************************** get cell temp ***********************************************/
    DAStatus2_t temperature = {0,};
    block.cmd = MAC_CMD_DA_STATUS2; // Command to read cell temperatures
    block.len = 14;                 // 14 bytes for cell temperatures

    this->_bq4050->read_mac_block(&block); // Read the data block from the BQ4050
    memcpy(&temperature, block.pvalue, sizeof(DAStatus2_t)); // Copy the data into the temperature structure
    delay(10); 

    this->sta.cells[0].temperature = (this->sta.cell_count >= 1) ? temperature.ts1_temp / 10.0f - 273.15f : 0.0f;// Convert from Kelvin to Celsius
    this->sta.cells[1].temperature = (this->sta.cell_count >= 2) ? temperature.ts2_temp / 10.0f - 273.15f : 0.0f;
    this->sta.cells[2].temperature = (this->sta.cell_count >= 3) ? temperature.ts3_temp / 10.0f - 273.15f : 0.0f;
    this->sta.cells[3].temperature = (this->sta.cell_count >= 4) ? temperature.ts4_temp / 10.0f - 273.15f : 0.0f;

    /**************************************************** get cell voltage *********************************************/
    for (uint8_t i = 0; i < this->sta.cell_count; i++) {
        this->sta.cells[i].cell_num = i + 1;
        reg.addr = BQ4050_CELL1_VOLTAGE - i; // Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
        res = this->_bq4050->read_reg_word(&reg); 
        this->sta.cells[i].voltage = (res) ? reg.value : this->sta.cells[i].voltage; // Convert from mV to V
        delay(10); 
    }

    /**************************************************** get learned capacity ********************************************/
    reg.addr = BQ4050_REG_FCC; 
    res  = this->_bq4050->read_reg_word(&reg);
    this->sta.learned_capacity = (res) ? reg.value : this->sta.learned_capacity; 

    delay(10); 
    return true; // Return true to indicate status update was successful
}

bool MeshSolar::get_bat_realtime_config(){
    bq4050_block_t block= {
        .cmd = DF_CMD_SBS_DATA_CHEMISTRY,
        .len = 5,
        .pvalue =nullptr,
        .type = STRING
    };
    /*****************************************   bat type   *************************************/
    this->_bq4050->read_dataflash_block(&block); 
    if(0 == strcasecmp((const char *)block.pvalue, "lfe4")) {
        strlcpy(this->cmd.basic.bat_type, "lifepo4", sizeof(this->cmd.basic.bat_type));
    }
    else if(0 == strcasecmp((const char *)block.pvalue, "lion")) {
        strlcpy(this->cmd.basic.bat_type, "liion", sizeof(this->cmd.basic.bat_type));
    }
    else if(0 == strcasecmp((const char *)block.pvalue, "lipo")) {
        strlcpy(this->cmd.basic.bat_type, "lipo", sizeof(this->cmd.basic.bat_type));
    }
    else {
        return false; // Unknown battery type, return false
    }
    /*****************************************  cell count  *************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration again
    block.pvalue[0] = (block.pvalue[0] & 0b00000011);        // Mask to get only the last 2 bits for DA configuration
    block.pvalue[0] = (block.pvalue[0] > 3) ? 3 : block.pvalue[0];        // Ensure DA value is within valid range (0-3)
    this->sync_rsp.basic.cell_number = block.pvalue[0] + 1; // Set cell count based on DA configuration (0-3 corresponds to 1-4 cells)
    /*****************************************  design capacity  *************************************/
    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH; // Command to access design capacity
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read design capacity
    this->sync_rsp.basic.design_capacity = (block.pvalue[0] << 8) | block.pvalue[1]; // Combine bytes to get capacity in mAh
    /*****************************************  cutoff voltage  *************************************/
    block.cmd = DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR; // Command to access cutoff voltage
    block.len = 2; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER
    this->_bq4050->read_dataflash_block(&block); // Read cutoff voltage
    this->sync_rsp.basic.discharge_cutoff_voltage = (block.pvalue[0] << 8) | block.pvalue[1]; // Combine bytes to get voltage in mV
    /*****************************************  temperature protection  *************************************/


    return true; // Return false to indicate configuration update was successful
}

bool MeshSolar::bat_basic_type_setting_update(){ 
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

    const char *bat_type = nullptr;
    // Set voltage parameters based on battery type with temperature compensation
    if(0 == strcasecmp(this->cmd.basic.bat_type, "lifepo4")) {
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
        bat_type = "lfe4"; // Set battery type to LiFePO4 , bq4050 uses 4 character to store chemistry name
    }
    else if((0 == strcasecmp(this->cmd.basic.bat_type, "lipo"))) {
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
        bat_type = "lipo"; // Set battery type to LiPo , bq4050 uses 4 character to store chemistry name
    }
    else if((0 == strcasecmp(this->cmd.basic.bat_type, "liion"))) {
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
        bat_type = "lion"; // Set battery type to Li-ion , bq4050 uses 4 character to store chemistry name
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
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY,  config.cov_recovery.low_temp,     "DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY  "},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY,  config.cov_recovery.std_temp,     "DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY  "},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY, config.cov_recovery.high_temp,    "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY "},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY,  config.cov_recovery.rec_temp,     "DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY  "},
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
            LOG_W("%s set to: %d mV - OK", name, read_value);
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
    block.pvalue[0] = strlen(bat_type); // Set the first byte to the length of the battery type string
    strlcpy((char *)block.pvalue + 1, bat_type, block.len); // Copy the battery type string into the block value
    this->_bq4050->write_dataflash_block(block); // Write the battery type
    free(block.pvalue); // Free the allocated memory for block.pvalue
    block.pvalue = nullptr; // Set pointer to null after freeing memory


    delay(100); // Ensure the write is complete before reading
    bq4050_block_t ret = {0, 0, nullptr, STRING}; // Reset block structure for reading
    ret.cmd = block.cmd; // Command to access battery chemistry
    ret.len = block.len; // Length of the data block to read
    ret.type = block.type; // Set block type to STRING
    this->_bq4050->read_dataflash_block(&ret); // Read the battery type back from data flash
    if(0 == strcasecmp((const char *)(ret.pvalue), bat_type)) {
        LOG_W("DF_CMD_SBS_DATA_CHEMISTRY after: %s - OK", (const char *)(ret.pvalue)); // Log success
    }
    else {
        LOG_E("DF_CMD_SBS_DATA_CHEMISTRY after: %s - ERROR", (const char *)(ret.pvalue)); // Log error
        res = false; // If the read value does not match, set result to false
    }

    return res;
}

bool MeshSolar::bat_basic_model_setting_update() {

    return false;
}

bool MeshSolar::bat_basic_cells_setting_update() {
    bool res = true; // Initialize result variable
    uint8_t cells_bits = 0b10;
    bq4050_block_t block = {0, 0, nullptr, NUMBER}, ret = {0, 0, nullptr, NUMBER}; // Initialize block structures
    /*************************************** setting—>configuration—>DA configuration—>CC1\CC0 ******************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration
    delay(100); // Ensure the read is complete before modifying
    cells_bits = (cells_bits > 3) ? 3 : this->cmd.basic.cell_number - 1; // Convert cell count to bits (0-3 for 1-4 cells)

    // Clear the last 2 bits first, then set new cell count bits
    block.pvalue[0] &= 0b11111100; // Clear bits 0 and 1 (last 2 bits)
    block.pvalue[0] |= cells_bits;  // Set new cell count bits
    block.cmd = block.cmd; // Command to access DA configuration
    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the read is complete before modifying

    memset(&ret, 0, sizeof(ret)); // Reset block structure
    ret.cmd = block.cmd; // Command to access DA configuration
    ret.len = 1; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret); // Read current DA configuration again
    // dbgSerial.print("DF_CMD_DA_CONFIGURATION after: 0x");
    // dbgSerial.println(ret.pvalue[0], HEX);
    LOG_W("DF_CMD_DA_CONFIGURATION after: 0x%02X", ret.pvalue[0]); // Log the read DA configuration
    res &= (ret.pvalue[0] == block.pvalue[0]);
    /********************************************** Gauging—>Design—>Design Voltage *****************************************************/
    uint16_t voltage = 0; // Initialize voltage variable
    if(0 == strcasecmp(this->cmd.basic.bat_type, "lifepo4")) {
        voltage = 3600;
    }
    else if((0 == strcasecmp(this->cmd.basic.bat_type, "liion")) || (0 == strcasecmp(this->cmd.basic.bat_type, "lipo"))){
        voltage = 4200;
    }
    else {
        // dbgSerial.println("Unknown battery type, exit!!!!!!!");
        LOG_E("Unknown battery type, exit!!!!!!!");
        return false;
    }
    voltage = this->cmd.basic.cell_number * voltage; // Set design voltage based on cell count

    // Clear the last 2 bits first, then set new cell count bits
    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV; // Command to access DA configuration
    block.pvalue = (uint8_t*)&voltage; // Pointer to the design voltage value
    block.len = 2; // Length of the data block to read
    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the read is complete before modifying

    memset(&ret, 0, sizeof(ret)); // Reset block structure
    ret.cmd = block.cmd; // Command to access DA configuration
    ret.len = 2; // Length of the data block to read

    this->_bq4050->read_dataflash_block(&ret); // Read current DA configuration again
    // dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV after: 0x");
    // dbgSerial.println(ret.pvalue[0], HEX);
    LOG_W("DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV after: 0x%02X", ret.pvalue[0]); // Log the read design voltage
    res &= (*(uint16_t*)(ret.pvalue) == voltage); // Verify the voltage setting


    return res; 
}

bool MeshSolar::bat_basic_design_capacity_setting_update(){
    bq4050_block_t block = {0,0, nullptr, NUMBER}, ret = {0,0, nullptr, NUMBER}; // Initialize block structures
    bool res = true; // Initialize result variable
    /*************************************** Gas Gauging—>Design—>Design Capacity mAh ******************************************/
    uint16_t capacity = this->cmd.basic.design_capacity; // Set the design capacity value

    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH; // Command to access design capacity in mAh
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    // dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH after: ");
    // dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    // dbgSerial.println(" mAh");
    LOG_W("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH after: %d mAh", ret.pvalue[0] | (ret.pvalue[1] << 8)); // Log the read design capacity

    res &= (capacity == *(uint16_t*)(ret.pvalue));
    /*************************************** Gas Gauging—>Design—>Design Capacity cWh ******************************************/
    float voltage = 0; // Initialize voltage variable
    if(0 == strcasecmp(this->cmd.basic.bat_type, "lifepo4")) {
        voltage = 3.6f;
    }
    else if((0 == strcasecmp(this->cmd.basic.bat_type, "liion")) || (0 == strcasecmp(this->cmd.basic.bat_type, "lipo"))){
        voltage = 4.2f;
    }
    else {
        // dbgSerial.println("Unknown battery type, exit!!!!!!!");
        LOG_E("Unknown battery type, exit!!!!!!!");
        return false;
    }
    capacity = this->cmd.basic.design_capacity; // Set the design capacity value
    capacity = (uint16_t)(this->cmd.basic.cell_number * voltage * capacity / 10.0f); // Convert mAh to cWh (assuming 4.2V per cell)

    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH; // Command to access design capacity in cWh
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    // dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH after: ");
    // dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    // dbgSerial.println(" cWh");
    LOG_W("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH after: %d cWh", ret.pvalue[0] | (ret.pvalue[1] << 8)); // Log the read design capacity in cWh

    res &= (capacity == *(uint16_t*)(ret.pvalue));
    /*************************************** Gas Gauging—>State—>Learned Full Charge Capacity mAh ******************************************/
    capacity = this->cmd.basic.design_capacity; // Set the design capacity value

    block.cmd = DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY;
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    LOG_W("DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY after: %d mAh", ret.pvalue[0] | (ret.pvalue[1] << 8)); // Log the read learned capacity
    res &= (capacity == *(uint16_t*)(ret.pvalue));

    return res;
}

bool MeshSolar::bat_basic_discharge_cutoff_voltage_setting_update(){
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
            LOG_W("%s set to: %d mV - OK", name, read_value); // Log success
            return true;
        } else {
            LOG_E("%s set to: %d mV - ERROR (expected %d mV)", name, read_value, value); // Log error
            return false;
        }
    };

    cutoff_config_entry_t configurations[] = {
        // Gas Gauging - Final Discharge (FD) thresholds
        {DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR,   0,    "FD Set Voltage Threshold  "},      // Lowest discharge voltage
        {DF_CMD_GAS_GAUGE_FD_CLEAR_VOLTAGE_THR, 100,  "FD Clear Voltage Threshold"},    // Recovery voltage (+100mV)
        
        // Gas Gauging - Terminate Discharge (TD) thresholds  
        {DF_CMD_GAS_GAUGE_TD_SET_VOLTAGE_THR,   0,    "TD Set Voltage Threshold  "},      // Same as FD set
        {DF_CMD_GAS_GAUGE_TD_CLEAR_VOLTAGE_THR, 100,  "TD Clear Voltage Threshold"},    // Recovery voltage (+100mV)
        
        // Gas Gauging - End Discharge Voltage (EDV) stepped warnings
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0,  0,    "CEDV Fixed EDV0           "},              // First warning level
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1,  20,   "CEDV Fixed EDV1           "},              // Second warning (+20mV)
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2,  30,   "CEDV Fixed EDV2           "},              // Third warning (+30mV)
        
        // Protection - Cell Under Voltage (CUV) hardware protection
        {DF_CMD_PROTECTIONS_CUV_THR,           -50,   "CUV Protection Threshold  "},      // Hardware cutoff (-50mV for safety margin)
        {DF_CMD_PROTECTIONS_CUV_RECOVERY,      100,   "CUV Recovery Voltage      "}           // Recovery to re-enable (+100mV)
    };

    // Apply all discharge cutoff configurations
    for(auto& cfg : configurations) {
        uint16_t target_voltage = cutoff_base + cfg.offest;
        res &= write_and_verify(cfg.cmd, target_voltage, cfg.name);
    }

    return res;
}

bool MeshSolar::bat_basic_temp_protection_setting_update() {
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
    int16_t charge_high    = this->cmd.basic.protection.charge_high_temp_c;
    int16_t charge_low     = this->cmd.basic.protection.charge_low_temp_c;
    int16_t discharge_high = this->cmd.basic.protection.discharge_high_temp_c;
    int16_t discharge_low  = this->cmd.basic.protection.discharge_low_temp_c;

    // Validate temperature ranges (basic sanity check)
    if (charge_low >= charge_high || discharge_low >= discharge_high) {
        LOG_E("ERROR: Invalid temperature ranges in configuration");
        LOG_E("  Charge: %d°C to %d°C", charge_low, charge_high);
        LOG_E("  Discharge: %d°C to %d°C", discharge_low, discharge_high);
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
        {DF_CMD_PROTECTIONS_OTC_THR,      config.otc_threshold, "DF_CMD_PROTECTIONS_OTC_THR     "},
        {DF_CMD_PROTECTIONS_OTC_RECOVERY, config.otc_recovery,  "DF_CMD_PROTECTIONS_OTC_RECOVERY"},
        {DF_CMD_PROTECTIONS_UTC_THR,      config.utc_threshold, "DF_CMD_PROTECTIONS_UTC_THR     "},
        {DF_CMD_PROTECTIONS_UTC_RECOVERY, config.utc_recovery,  "DF_CMD_PROTECTIONS_UTC_RECOVERY"},
        
        // Discharge temperature protections
        {DF_CMD_PROTECTIONS_OTD_THR,      config.otd_threshold, "DF_CMD_PROTECTIONS_OTD_THR     "},
        {DF_CMD_PROTECTIONS_OTD_RECOVERY, config.otd_recovery,  "DF_CMD_PROTECTIONS_OTD_RECOVERY"},
        {DF_CMD_PROTECTIONS_UTD_THR,      config.utd_threshold, "DF_CMD_PROTECTIONS_UTD_THR     "},
        {DF_CMD_PROTECTIONS_UTD_RECOVERY, config.utd_recovery,  "DF_CMD_PROTECTIONS_UTD_RECOVERY"}
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
            LOG_W("%s set to: %.1f°C - OK", name, temp_celsius); // Log success
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
            LOG_W("Temperature protection enabled in %s (%s set)", reg_name, bit_desc);
        } else {
            // Disable temperature protection: clear specified bits
            register_value &= ~bit_mask;
            LOG_W("Temperature protection disabled in %s (%s cleared)", reg_name, bit_desc);
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
        // Convert to binary string for logging (since %b is not standard)
        char binary_str[9];
        for (int i = 7; i >= 0; i--) {
            binary_str[7-i] = (verified_value & (1 << i)) ? '1' : '0';
        }
        binary_str[8] = '\0';
        LOG_W("%s register: 0b%s (0x%02X)", reg_name, binary_str, verified_value);
        
        // Check if bits are set correctly
        bool bits_match_expected = ((verified_value & bit_mask) != 0) == this->cmd.basic.protection.enabled;
        
        if (bits_match_expected) {
            LOG_W("%s temperature protection bits verified - OK", reg_name); // Log success
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

bool MeshSolar::bat_advance_battery_config_update() {
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
            LOG_W("%s set to: %d mV - OK", name, read_value);
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

bool MeshSolar::bat_advance_cedv_setting_update(){
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
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0,       (uint16_t)this->cmd.advance.cedv.cedv0,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0     "},
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1,       (uint16_t)this->cmd.advance.cedv.cedv1,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1     "},
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2,       (uint16_t)this->cmd.advance.cedv.cedv2,             "DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2     "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0,   (uint16_t)this->cmd.advance.cedv.discharge_cedv0,   "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0 "},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10,  (uint16_t)this->cmd.advance.cedv.discharge_cedv10,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20,  (uint16_t)this->cmd.advance.cedv.discharge_cedv20,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30,  (uint16_t)this->cmd.advance.cedv.discharge_cedv30,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40,  (uint16_t)this->cmd.advance.cedv.discharge_cedv40,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50,  (uint16_t)this->cmd.advance.cedv.discharge_cedv50,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60,  (uint16_t)this->cmd.advance.cedv.discharge_cedv60,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70,  (uint16_t)this->cmd.advance.cedv.discharge_cedv70,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80,  (uint16_t)this->cmd.advance.cedv.discharge_cedv80,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90,  (uint16_t)this->cmd.advance.cedv.discharge_cedv90,  "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90"},
        {DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100, (uint16_t)this->cmd.advance.cedv.discharge_cedv100, "DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100"},
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
            LOG_W("%s set to: %d mV - OK", name, read_value); // Log success
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

bool MeshSolar::bat_fet_toggle(){
    return this->_bq4050->fet_toggle(); // Call the BQ4050 method to toggle FETs
}


bool MeshSolar::bat_reset() {
    return this->_bq4050->reset(); // Call the BQ4050 method to reset the device
}
