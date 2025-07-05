#include "bq4050.h"
#include "solar.h"


MeshSolar::MeshSolar(){
    this->_bq4050 = nullptr; // Initialize pointer to null
    memset(&this->sta, 0, sizeof(this->sta)); // Initialize status structure to zero
    memset(&this->cmd, 0, sizeof(this->cmd)); // Initialize command structure to zero
    this->cmd.battery.cell_number = 4; // Default to 4 cells
    this->cmd.battery.design_capacity = 3200; // Default design capacity in m
    this->cmd.battery.cutoff_voltage = 2800; // Default cutoff voltage in mV
    strlcpy(this->cmd.battery.type, "LiFePO4", sizeof(this->cmd.battery.type)); // Default battery type
    this->cmd.temperature_protection.high_temp_c = 60; // Default high temperature threshold
    this->cmd.temperature_protection.high_temp_enabled = true; // Default high temperature protection enabled
    this->cmd.temperature_protection.low_temp_c = -10; // Default low temperature threshold
    this->cmd.temperature_protection.low_temp_enabled = true; // Default low temperature protection enabled
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

bool MeshSolar::get_bat_status(){
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

bool MeshSolar::bat_type_setting_update(){ 
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

    // Set voltage parameters based on battery type with temperature compensation
    if(0 == strcasecmp(this->cmd.battery.type, "lifepo4")) {
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
    }
    else if((0 == strcasecmp(this->cmd.battery.type, "liion")) || (0 == strcasecmp(this->cmd.battery.type, "lipo"))) {
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
    }
    else {
        dbgSerial.println("Unknown battery type, exit!!!!!!!");
        return false;
    }

    config_entry_t configurations[] = {
        // Advanced charge algorithm voltages
        {DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL,  config.charge_voltage.low_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL"},
        {DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL,  config.charge_voltage.std_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL"},
        {DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL, config.charge_voltage.high_temp, "DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL"},
        {DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL,  config.charge_voltage.rec_temp,  "DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL"},
        
        // Protection COV thresholds
        {DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR,  config.cov_threshold.low_temp,  "DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR"},
        {DF_CMD_PROTECTIONS_COV_STD_TEMP_THR,  config.cov_threshold.std_temp,  "DF_CMD_PROTECTIONS_COV_STD_TEMP_THR"},
        {DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR, config.cov_threshold.high_temp, "DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR"},
        {DF_CMD_PROTECTIONS_COV_REC_TEMP_THR,  config.cov_threshold.rec_temp,  "DF_CMD_PROTECTIONS_COV_REC_TEMP_THR"},
        
        // Protection COV recovery
        {DF_CMD_PROTECTIONS_CUV_RECOVERY_LOW_TEMP_THR,  config.cov_recovery.low_temp,  "DF_CMD_PROTECTIONS_CUV_RECOVERY_LOW_TEMP_THR"},
        {DF_CMD_PROTECTIONS_CUV_RECOVERY_STD_TEMP_THR,  config.cov_recovery.std_temp,  "DF_CMD_PROTECTIONS_CUV_RECOVERY_STD_TEMP_THR"},
        {DF_CMD_PROTECTIONS_CUV_RECOVERY_HIGH_TEMP_THR, config.cov_recovery.high_temp, "DF_CMD_PROTECTIONS_CUV_RECOVERY_HIGH_TEMP_THR"},
        {DF_CMD_PROTECTIONS_CUV_RECOVERY_REC_TEMP_THR,  config.cov_recovery.rec_temp,  "DF_CMD_PROTECTIONS_CUV_RECOVERY_REC_TEMP_THR"}
    };

    // Helper lambda function to write and verify voltage setting
    auto write_and_verify = [&](uint16_t cmd, uint16_t value, const char* name) -> bool {
        bq4050_block_t block = {cmd, 2, (uint8_t*)&value, NUMBER};
        bq4050_block_t ret = {cmd, 2, nullptr, NUMBER};

        // Write the value
        if (!this->_bq4050->write_dataflash_block(block)) {
            dbgSerial.print("Failed to write ");
            dbgSerial.println(name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            dbgSerial.print("Failed to read back ");
            dbgSerial.println(name);
            return false;
        }

        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        dbgSerial.print(name);
        dbgSerial.print(" set to: ");
        dbgSerial.print(read_value);
        dbgSerial.print(" mV");
        
        if (value == read_value) {
            dbgSerial.println(" - OK");
            return true;
        } else {
            dbgSerial.print(" - ERROR (expected ");
            dbgSerial.print(value);
            dbgSerial.println(" mV)");
            return false;
        }

        return (value == read_value);
    };


    // Apply all configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify(cfg.cmd, cfg.value, cfg.name);
    }

    return res;
}

bool MeshSolar::bat_model_setting_update() {

    return false;
}

bool MeshSolar::bat_cells_setting_update() {
    bool res = true; // Initialize result variable
    uint8_t cells_bits = 0b10;
    bq4050_block_t block = {0, 0, nullptr, NUMBER}, ret = {0, 0, nullptr, NUMBER}; // Initialize block structures
    /*************************************** setting—>configuration—>DA configuration—>CC1\CC0 ******************************************/
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration
    delay(100); // Ensure the read is complete before modifying
    cells_bits = (cells_bits > 3) ? 3 : this->cmd.battery.cell_number - 1; // Convert cell count to bits (0-3 for 1-4 cells)

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
    dbgSerial.print("DF_CMD_DA_CONFIGURATION after: 0x");
    dbgSerial.println(ret.pvalue[0], HEX);
    res &= (ret.pvalue[0] == block.pvalue[0]);
    /********************************************** Gauging—>Design—>Design Voltage *****************************************************/
    uint16_t voltage = 0; // Initialize voltage variable
    if(0 == strcasecmp(this->cmd.battery.type, "lifepo4")) {
        voltage = 3600;
    }
    else if((0 == strcasecmp(this->cmd.battery.type, "liion")) || (0 == strcasecmp(this->cmd.battery.type, "lipo"))){
        voltage = 4200;
    }
    else {
        dbgSerial.println("Unknown battery type, exit!!!!!!!");
        return false;
    }
    voltage = this->cmd.battery.cell_number * voltage; // Set design voltage based on cell count

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
    dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV after: 0x");
    dbgSerial.println(ret.pvalue[0], HEX);
    res &= (*(uint16_t*)(ret.pvalue) == voltage); // Verify the voltage setting


    return res; 
}

bool MeshSolar::bat_design_capacity_setting_update(){
    bq4050_block_t block = {0,0, nullptr, NUMBER}, ret = {0,0, nullptr, NUMBER}; // Initialize block structures
    bool res = true; // Initialize result variable
    /*************************************** Gas Gauging—>Design—>Design Capacity mAh ******************************************/
    uint16_t capacity = this->cmd.battery.design_capacity; // Set the design capacity value

    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH; // Command to access design capacity in mAh
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH after: ");
    dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    dbgSerial.println(" mAh");

    res &= (capacity == *(uint16_t*)(ret.pvalue));
    /*************************************** Gas Gauging—>Design—>Design Capacity cWh ******************************************/
    float voltage = 0; // Initialize voltage variable
    if(0 == strcasecmp(this->cmd.battery.type, "lifepo4")) {
        voltage = 3.6f;
    }
    else if((0 == strcasecmp(this->cmd.battery.type, "liion")) || (0 == strcasecmp(this->cmd.battery.type, "lipo"))){
        voltage = 4.2f;
    }
    else {
        dbgSerial.println("Unknown battery type, exit!!!!!!!");
        return false;
    }
    capacity = this->cmd.battery.design_capacity; // Set the design capacity value
    capacity = (uint16_t)(this->cmd.battery.cell_number * voltage * capacity / 10.0f); // Convert mAh to cWh (assuming 4.2V per cell)

    block.cmd = DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH; // Command to access design capacity in cWh
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    dbgSerial.print("DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH after: ");
    dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    dbgSerial.println(" cWh");

    res &= (capacity == *(uint16_t*)(ret.pvalue));
    /*************************************** Gas Gauging—>State—>Learned Full Charge Capacity mAh ******************************************/
    capacity = this->cmd.battery.design_capacity; // Set the design capacity value

    block.cmd = DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY;
    block.len = 2; // Length of the data block to read
    block.pvalue = (uint8_t*)&capacity; // Pointer to the design capacity value

    this->_bq4050->write_dataflash_block(block);
    delay(100); // Ensure the write is complete before reading

    ret.cmd = block.cmd; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    dbgSerial.print("DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY after: ");
    dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    dbgSerial.println(" mAh");
    res &= (capacity == *(uint16_t*)(ret.pvalue));

    return res;
}

bool MeshSolar::bat_charge_cutoff_voltage_setting_update(){
 



    return false; // Return false as this function is not implemented yet
}

bool MeshSolar::bat_discharge_cutoff_voltage_setting_update(){
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
    uint16_t cutoff_base = this->cmd.battery.cutoff_voltage;

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
            dbgSerial.print("Failed to write :");
            dbgSerial.println(name);
            return false;
        }
        delay(100);

        // Read back and verify
        if (!this->_bq4050->read_dataflash_block(&ret)) {
            dbgSerial.print("Failed to read back :");
            dbgSerial.println(name);
            return false;
        }

        uint16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        dbgSerial.print(name);
        dbgSerial.print(" set to: ");
        dbgSerial.print(read_value);
        dbgSerial.print(" mV");
        
        if (value == read_value) {
            dbgSerial.println(" - OK");
            return true;
        } else {
            dbgSerial.print(" - ERROR (expected ");
            dbgSerial.print(value);
            dbgSerial.println(" mV)");
            return false;
        }
    };

    cutoff_config_entry_t configurations[] = {
        // Gas Gauging - Final Discharge (FD) thresholds
        {DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR,   0,    "FD Set Voltage Threshold"},      // Lowest discharge voltage
        {DF_CMD_GAS_GAUGE_FD_CLEAR_VOLTAGE_THR, 100,  "FD Clear Voltage Threshold"},    // Recovery voltage (+100mV)
        
        // Gas Gauging - Terminate Discharge (TD) thresholds  
        {DF_CMD_GAS_GAUGE_TD_SET_VOLTAGE_THR,   0,    "TD Set Voltage Threshold"},      // Same as FD set
        {DF_CMD_GAS_GAUGE_TD_CLEAR_VOLTAGE_THR, 100,  "TD Clear Voltage Threshold"},    // Recovery voltage (+100mV)
        
        // Gas Gauging - End Discharge Voltage (EDV) stepped warnings
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0,  0,    "CEDV Fixed EDV0"},              // First warning level
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1,  20,   "CEDV Fixed EDV1"},              // Second warning (+20mV)
        {DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2,  30,   "CEDV Fixed EDV2"},              // Third warning (+30mV)
        
        // Protection - Cell Under Voltage (CUV) hardware protection
        {DF_CMD_PROTECTIONS_CUV_THR,           -50,   "CUV Protection Threshold"},      // Hardware cutoff (-50mV for safety margin)
        {DF_CMD_PROTECTIONS_CUV_RECOVERY,      100,   "CUV Recovery Voltage"}           // Recovery to re-enable (+100mV)
    };

    // Apply all discharge cutoff configurations
    for(auto& cfg : configurations) {
        uint16_t target_voltage = cutoff_base + cfg.offest;
        res &= write_and_verify(cfg.cmd, target_voltage, cfg.name);
    }

    return res;
}


bool MeshSolar::bat_voltage_thresholds_setting_update(){


    return false;
}

bool MeshSolar::bat_charge_over_voltage_setting_update(){



    return false;
}

bool MeshSolar::bat_discharge_over_heat_setting_update(){


    return false;
}

bool MeshSolar::bat_charge_over_heat_setting_update(){


    return false;
}

bool MeshSolar::bat_discharge_low_temp_setting_update(){

    return false;
}

bool MeshSolar::bat_charge_low_temp_setting_update(){

    return false;
}


bool MeshSolar::bat_fet_toggle(){
    return this->_bq4050->fet_toggle(); // Call the BQ4050 method to toggle FETs
}


bool MeshSolar::bat_reset() {
    return this->_bq4050->reset(); // Call the BQ4050 method to reset the device
}
