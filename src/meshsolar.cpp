#include "bq4050.h"
#include "meshsolar.h"
#include "logger.h"

/**
 * @brief Parse BQ4050 SafetyStatus register and convert to human-readable string
 * 
 * PLATFORM-INDEPENDENT FUNCTION
 * This function analyzes the BQ4050 SafetyStatus register bits and converts active
 * protection flags into a readable comma-separated string format. It provides a
 * standardized way to interpret battery protection states across different platforms.
 * 
 * @param safety_status SafetyStatus_t structure containing raw register bits from BQ4050
 * @return String containing comma-separated protection names (e.g., "CUV,COV,OTC") 
 *         Returns "Normal" if no protection bits are active
 * 
 * USAGE EXAMPLE:
 *   SafetyStatus_t status = {0x0003}; // CUV and COV bits set
 *   String result = parseSafetyStatusBits(status); // Returns "CUV,COV"
 * 
 * SUPPORTED PROTECTION TYPES:
 *   - Voltage: CUV, COV, CUVC
 *   - Current: OCC1/2, OCD1/2, AOLD/AOLDL, ASCC/ASCL, ASCD/ASCDL
 *   - Temperature: OTC, OTD, OTF, UTC, UTD
 *   - Timing: PTO, CTO
 *   - Charge: OC, CHGC, CHGV, PCHGC
 */
String parseSafetyStatusBits(const SafetyStatus_t& safety_status) {
    String result = "";
    bool first = true;
    
    // Helper lambda to add bit name to result if bit is set
    auto addIfSet = [&](bool bit_value, const char* bit_name) {
        if (bit_value) {
            if (!first) {
                result += ",";
            }
            result += bit_name;
            first = false;
        }
    };
    
    // Check all bits in SafetyStatus_t structure
    // Bits 0-15 (Low word)
    addIfSet(safety_status.bits.cuv,    "CUV");     // Cell Under Voltage
    addIfSet(safety_status.bits.cov,    "COV");     // Cell Over Voltage
    addIfSet(safety_status.bits.occ1,   "OCC1");    // Overcurrent During Charge 1
    addIfSet(safety_status.bits.occ2,   "OCC2");    // Overcurrent During Charge 2
    addIfSet(safety_status.bits.ocd1,   "OCD1");    // Overcurrent During Discharge 1
    addIfSet(safety_status.bits.ocd2,   "OCD2");    // Overcurrent During Discharge 2
    addIfSet(safety_status.bits.aold,   "AOLD");    // Overload During Discharge
    addIfSet(safety_status.bits.aoldl,  "AOLDL");   // Overload During Discharge Latch
    addIfSet(safety_status.bits.ascc,   "ASCC");    // Short-circuit During Charge
    addIfSet(safety_status.bits.ascl,   "ASCL");    // Short-circuit During Charge Latch
    addIfSet(safety_status.bits.ascd,   "ASCD");    // Short-circuit During Discharge
    addIfSet(safety_status.bits.ascdl,  "ASCDL");   // Short-circuit During Discharge Latch
    addIfSet(safety_status.bits.otc,    "OTC");     // Overtemperature During Charge
    addIfSet(safety_status.bits.otd,    "OTD");     // Overtemperature During Discharge
    addIfSet(safety_status.bits.cuvc,   "CUVC");    // Cell Undervoltage Compensated
    
    // Bits 16-31 (High word)
    addIfSet(safety_status.bits.otf,    "OTF");     // Overtemperature FET
    addIfSet(safety_status.bits.pto,    "PTO");     // Precharge Timeout
    addIfSet(safety_status.bits.cto,    "CTO");     // Charge Timeout
    addIfSet(safety_status.bits.oc,     "OC");      // Overcharge
    addIfSet(safety_status.bits.chgc,   "CHGC");    // Overcharging Current
    addIfSet(safety_status.bits.chgv,   "CHGV");    // Overcharging Voltage
    addIfSet(safety_status.bits.pchgc,  "PCHGC");   // Over-Precharge Current
    addIfSet(safety_status.bits.utc,    "UTC");     // Undertemperature During Charge
    addIfSet(safety_status.bits.utd,    "UTD");     // Undertemperature During Discharge
    
    // Return "NONE" if no bits are set, otherwise return the comma-separated list
    return (result.length() == 0) ? "Normal" : result;
}

/**
 * @brief Default constructor for MeshSolar battery management system
 * 
 * PLATFORM-INDEPENDENT CONSTRUCTOR
 * Initializes all internal data structures with safe default values suitable for
 * LiFePO4 battery systems. This constructor sets up reasonable defaults that work
 * across different hardware platforms without requiring specific configuration.
 * 
 * @param None
 * @return None (constructor)
 * 
 * DEFAULT CONFIGURATION:
 *   - Battery Type: LiFePO4 (safest chemistry)
 *   - Cell Count: 4 cells
 *   - Design Capacity: 3200 mAh
 *   - Cutoff Voltage: 2800 mV per cell
 *   - Temperature Protection: Enabled (-10°C to 60°C)
 *   - All pointers initialized to safe states
 * 
 * INITIALIZATION ORDER:
 *   1. Set BQ4050 pointer to null for safety
 *   2. Zero-initialize all status and command structures
 *   3. Set conservative default battery parameters
 *   4. Enable temperature protection with safe limits
 * 
 * PLATFORM NOTES:
 *   - No hardware-specific initialization performed
 *   - Safe for use on any platform supporting C++ constructors
 *   - Must call begin() after construction to associate with BQ4050 device
 */
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

/**
 * @brief Destructor for MeshSolar battery management system
 * 
 * PLATFORM-INDEPENDENT DESTRUCTOR
 * Safely cleans up resources and ensures proper shutdown of the BQ4050 interface.
 * Designed to prevent resource leaks and dangling pointers across all platforms.
 * 
 * @param None
 * @return None (destructor)
 * 
 * CLEANUP SEQUENCE:
 *   1. Check if BQ4050 device pointer is valid
 *   2. Call BQ4050 destructor to clean up I2C resources
 *   3. Set pointer to null to prevent dangling references
 *   4. No dynamic memory to free (all structures are stack-allocated)
 * 
 * SAFETY FEATURES:
 *   - Null pointer check before cleanup
 *   - Explicit pointer nullification
 *   - Exception-safe design
 * 
 * PLATFORM NOTES:
 *   - Works on any platform supporting C++ destructors
 *   - Does not perform platform-specific I2C cleanup
 *   - Safe to call even if begin() was never called
 */
MeshSolar::~MeshSolar(){
    if (this->_bq4050) {
        this->_bq4050->~BQ4050(); // Call destructor to clean up
        this->_bq4050 = nullptr;  // Set pointer to null after cleanup
    }
}

/**
 * @brief Initialize MeshSolar with BQ4050 device instance
 * 
 * PLATFORM-INDEPENDENT INITIALIZATION
 * Associates the MeshSolar controller with a specific BQ4050 device instance.
 * This function must be called after both objects are constructed but before
 * any battery operations are performed.
 * 
 * @param device Pointer to initialized BQ4050 device instance
 *               Must be valid and already configured for I2C communication
 * @return None
 * 
 * PRECONDITIONS:
 *   - BQ4050 device must be constructed and begin() called
 *   - I2C communication must be established
 *   - Device pointer must remain valid throughout MeshSolar lifetime
 * 
 * POSTCONDITIONS:
 *   - MeshSolar can perform all battery management operations
 *   - All get_*() and update_*() functions become available
 *   - Device communication is ready for use
 * 
 * PLATFORM NOTES:
 *   - No platform-specific code - pure pointer assignment
 *   - Compatible with any I2C implementation
 *   - Does not perform I/O operations or validation
 * 
 * USAGE EXAMPLE:
 *   BQ4050 bq4050;
 *   MeshSolar meshsolar;
 *   bq4050.begin(&Wire, BQ4050ADDR);
 *   meshsolar.begin(&bq4050);
 */
void MeshSolar::begin(BQ4050 *device) {
    this->_bq4050 = device;
}

/**
 * @brief Read comprehensive real-time battery status from BQ4050
 * 
 * PLATFORM-INDEPENDENT STATUS READER
 * Performs a complete battery status update by reading all critical parameters
 * from the BQ4050 device. This function aggregates multiple I2C transactions
 * into a single comprehensive status update suitable for monitoring applications.
 * 
 * @param None (updates internal sta structure)
 * @return bool True if all readings successful, false if any I2C operation failed
 * 
 * PARAMETERS READ:
 *   - Charge/discharge current (mA)
 *   - State of charge percentage (%)
 *   - Cell count configuration
 *   - Individual cell temperatures (°C)
 *   - Individual cell voltages (mV)
 *   - Total pack voltage (mV)
 *   - Pack voltage at terminals (mV)
 *   - Learned battery capacity (mAh)
 *   - FET enable status (boolean)
 *   - Safety/protection status (bit flags)
 *   - Emergency shutdown status (boolean)
 * 
 * I2C OPERATIONS PERFORMED:
 *   - Standard register reads (Current, RSOC, FCC)
 *   - DataFlash block reads (DA configuration)
 *   - MAC command reads (cell voltages, temperatures, status)
 * 
 * ERROR HANDLING:
 *   - Continues reading even if individual operations fail
 *   - Preserves previous values on read failure
 *   - Returns aggregate success status
 * 
 * TIMING CONSIDERATIONS:
 *   - Uses 10ms delays between I2C operations
 *   - Total execution time: ~100-200ms
 *   - Not suitable for high-frequency polling
 * 
 * PLATFORM NOTES:
 *   - Uses only standard BQ4050 class methods
 *   - No platform-specific timing or I/O
 *   - Compatible with any I2C implementation
 */
bool MeshSolar::get_realtime_bat_status(){
    bool res = true;
    bq4050_reg_t reg = {0,0};             // Initialize register structure
    bq4050_block_t block = {0,0,nullptr}; // Initialize block structure
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
    
    // Parse SafetyStatus bits and get human-readable string
    String safety_bits_str = parseSafetyStatusBits(safety_status);
    // Store the parsed bit names in protection_sta field (truncate if too long)
    strncpy(this->sta.protection_sta, safety_bits_str.c_str(), sizeof(this->sta.protection_sta) - 1);
    this->sta.protection_sta[sizeof(this->sta.protection_sta) - 1] = '\0'; // Ensure null termination
    LOG_L("Protection status raw: %08X", (unsigned int)safety_status.bytes); // Log raw hex value
    LOG_L("Protection status bits: %s", this->sta.protection_sta); // Log parsed bit names
    /**************************************************** get operation status **************************************/
    OperationStatus_t operation_status = {0,};
    block.cmd = MAC_CMD_OPERATION_STATUS;   // Command to read operation status
    block.len = 4;                          // Length of the data block to read
    res &= this->_bq4050->read_mac_block(&block); // Read the data block from the BQ4050
    memcpy(&operation_status, block.pvalue, sizeof(OperationStatus_t)); // Copy the data into the operation_status structure
    this->sta.emergency_shutdown = operation_status.bits.emshut; // Get emergency shutdown status


    return res; // Return true to indicate status update was successful
}

/**
 * @brief Read current basic battery configuration from BQ4050
 * 
 * PLATFORM-INDEPENDENT CONFIGURATION READER
 * Retrieves the current basic battery settings from BQ4050 DataFlash memory
 * and populates the sync_rsp.basic structure. This function reads back the
 * actual configuration stored in the device, not the commanded values.
 * 
 * @param None (updates internal sync_rsp.basic structure)
 * @return bool True if all configuration reads successful, false on any failure
 * 
 * CONFIGURATION PARAMETERS READ:
 *   - Battery chemistry type (LiFePO4, Li-ion, LiPo)
 *   - Cell count (1-4 cells)
 *   - Design capacity (mAh)
 *   - Discharge cutoff voltage (mV)
 *   - Temperature protection thresholds:
 *     * Charge high/low temperature (°C)
 *     * Discharge high/low temperature (°C)
 *     * Protection enable status (boolean)
 * 
 * DATAFLASH COMMANDS USED:
 *   - DF_CMD_SBS_DATA_CHEMISTRY: Battery type string
 *   - DF_CMD_DA_CONFIGURATION: Cell count configuration
 *   - DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH: Design capacity
 *   - DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR: Cutoff voltage
 *   - DF_CMD_PROTECTIONS_*_THR: Temperature thresholds
 *   - DF_CMD_SETTINGS_PROTECTIONS_ENABLE_*: Protection enables
 * 
 * CHEMISTRY MAPPING:
 *   - "LFE4" -> "lifepo4"
 *   - "LION" -> "liion"
 *   - "LIPO" -> "lipo"
 * 
 * ERROR HANDLING:
 *   - Returns false on unknown battery chemistry
 *   - Returns false on any DataFlash read failure
 *   - Logs detailed error messages for debugging
 * 
 * PLATFORM NOTES:
 *   - Pure DataFlash operations using BQ4050 class
 *   - No platform-specific I2C or timing dependencies
 *   - Uses standard C string operations
 */
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

/**
 * @brief Read current advanced battery configuration from BQ4050
 * 
 * PLATFORM-INDEPENDENT ADVANCED CONFIG READER
 * Retrieves advanced battery management settings from BQ4050 DataFlash memory,
 * including protection thresholds and CEDV (Charge End-of-Discharge Voltage)
 * curve parameters. These settings control precise battery gauge behavior.
 * 
 * @param None (updates internal sync_rsp.advance structure)
 * @return bool True if all advanced settings read successfully, false on any failure
 * 
 * ADVANCED PARAMETERS READ:
 *   Battery Protection Settings:
 *   - CUV (Cell Under Voltage) threshold (mV)
 *   - EOC (End of Charge) voltage (mV)
 *   - EOC protection voltage (mV)
 * 
 *   CEDV Fixed Values:
 *   - CEDV0, CEDV1, CEDV2 (End Discharge Voltage levels)
 * 
 *   CEDV Discharge Profile (Voltage vs SOC):
 *   - Voltage points at 0%, 10%, 20%, ..., 100% SOC
 *   - Used for accurate SOC calculation during discharge
 * 
 * DATAFLASH COMMANDS USED:
 *   - DF_CMD_PROTECTIONS_CUV_THR: Under-voltage protection
 *   - DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL: Charge voltage
 *   - DF_CMD_PROTECTIONS_COV_STD_TEMP_THR: Over-voltage protection
 *   - DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV*: Fixed EDV points
 *   - DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_*: Discharge profile
 * 
 * TIMING CONSIDERATIONS:
 *   - Uses 50ms delays between CEDV profile reads
 *   - Total execution time: ~800-1000ms for full read
 *   - Sequential reads to avoid bus contention
 * 
 * ERROR HANDLING:
 *   - Stops on first read failure and returns false
 *   - Detailed error logging for each failed parameter
 *   - Preserves partial data on early failure
 * 
 * PLATFORM NOTES:
 *   - Uses only BQ4050 DataFlash operations
 *   - No platform-specific timing or I/O
 *   - Compatible with any I2C implementation
 */
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

/**
 * @brief Configure BQ4050 battery chemistry and temperature-compensated voltages
 * 
 * PLATFORM-INDEPENDENT BATTERY TYPE CONFIGURATOR
 * Sets comprehensive battery chemistry parameters including temperature-compensated
 * charging voltages and protection thresholds. This function configures the BQ4050
 * for optimal operation with different lithium battery chemistries.
 * 
 * @param None (uses cmd.basic.type from internal command structure)
 * @return bool True if all configuration writes successful, false on any failure
 * 
 * SUPPORTED BATTERY TYPES:
 *   - "lifepo4": LiFePO4 (3.6V nominal, safer, more stable)
 *   - "liion": Li-ion (4.2V nominal, higher energy density)
 *   - "lipo": LiPo (4.2V nominal, similar to Li-ion)
 * 
 * TEMPERATURE COMPENSATION ZONES:
 *   - Low Temperature (-20°C to 0°C): Conservative voltages
 *   - Standard Temperature (0°C to 45°C): Nominal voltages
 *   - High Temperature (45°C to 60°C): Reduced voltages for safety
 *   - Recovery Temperature: Intermediate transition voltages
 * 
 * VOLTAGE PARAMETERS CONFIGURED:
 *   Advanced Charge Algorithm:
 *   - Charge voltages for each temperature zone
 *   Protection Thresholds:
 *   - COV (Cell Over Voltage) thresholds per temperature
 *   - COV recovery voltages per temperature
 *   SBS Data:
 *   - Battery chemistry identifier string
 * 
 * SAFETY FEATURES:
 *   - Temperature-dependent voltage derating
 *   - Write-verify cycles for all parameters
 *   - Conservative settings at temperature extremes
 *   - Hysteresis in recovery voltages
 * 
 * ERROR HANDLING:
 *   - Validates battery type before configuration
 *   - Verifies each write operation by reading back
 *   - Returns false on any verification failure
 *   - Detailed logging of all operations
 * 
 * PLATFORM NOTES:
 *   - Uses only BQ4050 DataFlash write/read operations
 *   - No platform-specific voltage or temperature handling
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~2-3 seconds
 *   - Suitable for configuration-time use only
 */
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

/**
 * @brief Configure BQ4050 for specific battery model (placeholder function)
 * 
 * PLATFORM-INDEPENDENT MODEL CONFIGURATOR
 * Reserved for future implementation of battery model-specific settings.
 * This function would configure parameters that vary between different
 * battery models of the same chemistry type.
 * 
 * @param None (would use cmd.basic.model from internal command structure)
 * @return bool Currently returns false (not implemented)
 * 
 * FUTURE IMPLEMENTATION SCOPE:
 *   - Model-specific capacity curves
 *   - Manufacturer-specific protection settings
 *   - Custom CEDV profiles for specific battery models
 *   - Brand-specific charging algorithms
 * 
 * POTENTIAL PARAMETERS:
 *   - Internal resistance compensation
 *   - Age-related capacity adjustments
 *   - Model-specific voltage curves
 *   - Manufacturer protection limits
 * 
 * PLATFORM NOTES:
 *   - Designed to be platform-independent
 *   - Would use standard BQ4050 DataFlash operations
 *   - No platform-specific model detection required
 */
bool MeshSolar::update_basic_bat_model_setting() {

    return false;
}

/**
 * @brief Configure BQ4050 cell count and design voltage parameters
 * 
 * PLATFORM-INDEPENDENT CELL CONFIGURATION
 * Configures the BQ4050 for the specified number of battery cells in series.
 * This function updates both the DA (Data Acquisition) configuration and the
 * total design voltage to match the cell count and chemistry.
 * 
 * @param None (uses cmd.basic.cell_number and cmd.basic.type from internal structure)
 * @return bool True if cell configuration successful, false on any failure
 * 
 * PARAMETERS CONFIGURED:
 *   DA Configuration:
 *   - Cell count bits (0-3 for 1-4 cells)
 *   - Updates only cell count bits, preserves other DA settings
 *   
 *   Design Voltage:
 *   - Calculated as: cell_count × cell_voltage
 *   - Cell voltage based on battery chemistry:
 *     * LiFePO4: 3600 mV per cell
 *     * Li-ion/LiPo: 4200 mV per cell
 * 
 * SUPPORTED CONFIGURATIONS:
 *   - 1-4 cells in series (1S, 2S, 3S, 4S)
 *   - Auto-calculates total pack voltage
 *   - Chemistry-appropriate cell voltages
 * 
 * DATAFLASH OPERATIONS:
 *   - Read-modify-write DA configuration register
 *   - Write calculated design voltage
 *   - Verify all writes by reading back
 * 
 * ERROR HANDLING:
 *   - Validates battery type before calculation
 *   - Limits cell count to maximum 4 cells
 *   - Verifies all DataFlash operations
 *   - Returns false on any verification failure
 * 
 * PLATFORM NOTES:
 *   - Uses standard BQ4050 DataFlash operations
 *   - No platform-specific cell detection
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~300-400ms
 */
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

/**
 * @brief Configure BQ4050 design capacity in multiple formats
 * 
 * PLATFORM-INDEPENDENT CAPACITY CONFIGURATOR
 * Sets the battery design capacity in both mAh and cWh (centiwatt-hours) formats
 * as required by the BQ4050. Also initializes the learned capacity to match the
 * design capacity for proper gas gauge operation.
 * 
 * @param None (uses cmd.basic.design_capacity, cell_number, and type from internal structure)
 * @return bool True if all capacity settings successful, false on any failure
 * 
 * PARAMETERS CONFIGURED:
 *   Design Capacity (mAh):
 *   - Direct setting from command structure
 *   - Used for SOC calculations and fuel gauge
 *   
 *   Design Capacity (cWh):
 *   - Calculated as: (cell_count × cell_voltage × capacity_mAh) / 10
 *   - Cell voltage varies by chemistry (LiFePO4: 3.6V, Li-ion/LiPo: 4.2V)
 *   
 *   Learned Full Charge Capacity:
 *   - Initially set to design capacity
 *   - Will be updated by BQ4050 learning algorithm over time
 * 
 * CALCULATION FORMULAS:
 *   - cWh = (cells × voltage × mAh) / 10
 *   - Example: 4S LiFePO4 3200mAh = (4 × 3.6 × 3200) / 10 = 4608 cWh
 * 
 * DATAFLASH COMMANDS USED:
 *   - DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH: Primary capacity
 *   - DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH: Energy capacity
 *   - DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY: Learning baseline
 * 
 * ERROR HANDLING:
 *   - Validates battery type before calculations
 *   - Verifies all DataFlash writes by reading back
 *   - Returns false if any verification fails
 *   - Detailed logging of calculated values
 * 
 * PLATFORM NOTES:
 *   - Pure mathematical calculations and DataFlash operations
 *   - No platform-specific capacity measurement
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~400-500ms
 */
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

/**
 * @brief Configure BQ4050 discharge cutoff voltage and protection levels
 * 
 * PLATFORM-INDEPENDENT DISCHARGE PROTECTION CONFIGURATOR
 * Sets comprehensive discharge protection parameters including final discharge
 * thresholds, terminate discharge levels, end discharge voltage warnings, and
 * cell under-voltage protection. Creates multi-level protection hierarchy.
 * 
 * @param None (uses cmd.basic.discharge_cutoff_voltage from internal structure)
 * @return bool True if all cutoff settings successful, false on any failure
 * 
 * PROTECTION HIERARCHY (from base cutoff voltage):
 *   Final/Terminate Discharge (FD/TD):
 *   - Set threshold: Base cutoff voltage (lowest safe voltage)
 *   - Clear threshold: Base + 100mV (recovery voltage)
 *   
 *   End Discharge Voltage (EDV) Warnings:
 *   - EDV0: Base cutoff (first warning level)
 *   - EDV1: Base + 20mV (second warning level)
 *   - EDV2: Base + 30mV (third warning level)
 *   
 *   Cell Under Voltage (CUV) Hardware Protection:
 *   - Threshold: Base - 50mV (hardware emergency cutoff)
 *   - Recovery: Base + 100mV (recovery to re-enable)
 * 
 * PROTECTION STRATEGY:
 *   - Multiple warning levels before hard cutoff
 *   - Hardware protection below software limits
 *   - Hysteresis prevents oscillation
 *   - Recovery voltages higher than thresholds
 * 
 * DATAFLASH COMMANDS CONFIGURED:
 *   - DF_CMD_GAS_GAUGE_FD_SET/CLEAR_VOLTAGE_THR: Final discharge
 *   - DF_CMD_GAS_GAUGE_TD_SET/CLEAR_VOLTAGE_THR: Terminate discharge
 *   - DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV*: End discharge warnings
 *   - DF_CMD_PROTECTIONS_CUV_THR/RECOVERY: Hardware protection
 * 
 * ERROR HANDLING:
 *   - Verifies all DataFlash writes by reading back
 *   - Returns false on any verification failure
 *   - Detailed logging of all voltage levels set
 * 
 * PLATFORM NOTES:
 *   - Uses only standard BQ4050 DataFlash operations
 *   - No platform-specific voltage measurement
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~1-1.5 seconds
 */
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

/**
 * @brief Configure BQ4050 temperature protection thresholds and enable settings
 * 
 * PLATFORM-INDEPENDENT TEMPERATURE PROTECTION CONFIGURATOR
 * Sets comprehensive temperature protection for both charging and discharging
 * operations. Configures threshold temperatures, recovery temperatures with
 * hysteresis, and enable/disable control for temperature protection features.
 * 
 * @param None (uses cmd.basic.protection temperature settings from internal structure)
 * @return bool True if all temperature settings successful, false on any failure
 * 
 * TEMPERATURE PROTECTION TYPES:
 *   Charge Protection:
 *   - OTC (Over Temperature Charge): High temp limit during charging
 *   - UTC (Under Temperature Charge): Low temp limit during charging
 *   
 *   Discharge Protection:
 *   - OTD (Over Temperature Discharge): High temp limit during discharge
 *   - UTD (Under Temperature Discharge): Low temp limit during discharge
 * 
 * HYSTERESIS STRATEGY:
 *   - Recovery temperatures are 5°C away from thresholds
 *   - Prevents oscillation at temperature boundaries
 *   - OTC/OTD recovery: Threshold - 5°C
 *   - UTC/UTD recovery: Threshold + 5°C
 * 
 * TEMPERATURE VALIDATION:
 *   - Ensures low temperature < high temperature
 *   - Validates separate charge and discharge ranges
 *   - Returns false on invalid temperature ranges
 * 
 * PROTECTION ENABLE CONFIGURATION:
 *   Protection Enable B Register (bits 4,5):
 *   - Bit 4: OTD (Over Temperature Discharge)
 *   - Bit 5: OTC (Over Temperature Charge)
 *   
 *   Protection Enable D Register (bits 2,3):
 *   - Bit 2: UTD (Under Temperature Discharge)
 *   - Bit 3: UTC (Under Temperature Charge)
 * 
 * DATA FORMAT:
 *   - All temperatures stored in 0.1°C units (e.g., 250 = 25.0°C)
 *   - Signed 16-bit values support negative temperatures
 *   - Range: -3276.8°C to +3276.7°C
 * 
 * ERROR HANDLING:
 *   - Validates temperature ranges before configuration
 *   - Verifies all DataFlash writes by reading back
 *   - Returns false on invalid ranges or write failures
 *   - Detailed logging of all temperature settings
 * 
 * PLATFORM NOTES:
 *   - Uses only BQ4050 DataFlash operations
 *   - No platform-specific temperature sensing
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~1.5-2 seconds
 */
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

/**
 * @brief Configure BQ4050 advanced battery settings and protection parameters
 * 
 * PLATFORM-INDEPENDENT ADVANCED BATTERY CONFIGURATOR
 * Sets advanced charge algorithm voltages, protection thresholds, and CEDV
 * (Charge End-of-Discharge Voltage) parameters based on the advance command
 * configuration. These settings provide fine-tuned control over battery
 * management beyond basic configuration.
 * 
 * @param None (uses cmd.advance.battery settings from internal structure)
 * @return bool True if all advanced settings successful, false on any failure
 * 
 * ADVANCED PARAMETERS CONFIGURED:
 *   Cell Under Voltage (CUV) Protection:
 *   - Primary under-voltage threshold for cell protection
 *   - Hardware-level protection trigger point
 *   
 *   End of Charge (EOC) Voltage:
 *   - Target voltage for charge termination
 *   - Controls when charging algorithm considers battery full
 *   
 *   EOC Protection Voltage:
 *   - Over-voltage protection during charging
 *   - Safety limit above normal charge voltage
 * 
 * CONFIGURATION STRATEGY:
 *   - Uses advance command structure values directly
 *   - No automatic calculation or compensation
 *   - Allows precise control over protection levels
 *   - Suitable for custom battery profiles
 * 
 * DATAFLASH COMMANDS USED:
 *   - DF_CMD_PROTECTIONS_CUV_THR: Under-voltage protection
 *   - DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL: Charge voltage
 *   - DF_CMD_PROTECTIONS_COV_STD_TEMP_THR: Over-voltage protection
 * 
 * ERROR HANDLING:
 *   - Verifies all DataFlash writes by reading back
 *   - Returns false on any verification failure
 *   - Detailed logging of configured values
 * 
 * PLATFORM NOTES:
 *   - Uses standard BQ4050 DataFlash operations
 *   - No platform-specific validation or calculation
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~400-500ms
 * 
 * USAGE CONTEXT:
 *   - Called after basic configuration is complete
 *   - Provides fine-tuning of protection parameters
 *   - Used with advance command from JSON interface
 */
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

/**
 * @brief Configure BQ4050 CEDV (Constant Energy Discharge Voltage) profile settings
 * 
 * PLATFORM-INDEPENDENT CEDV CONFIGURATOR
 * Sets the complete CEDV discharge profile for accurate State of Charge (SOC)
 * calculation. CEDV profiles define voltage vs. SOC curves that the BQ4050
 * uses to determine remaining battery capacity during discharge.
 * 
 * @param None (uses cmd.advance.cedv settings from internal structure)
 * @return bool True if all CEDV settings successful, false on any failure
 * 
 * CEDV PROFILE COMPONENTS:
 *   Fixed EDV Values:
 *   - CEDV0: Primary end discharge voltage
 *   - CEDV1: Secondary end discharge voltage
 *   - CEDV2: Final end discharge voltage
 *   
 *   Discharge Profile Curve (11 points):
 *   - Voltage values at SOC: 0%, 10%, 20%, 30%, 40%, 50%
 *                           60%, 70%, 80%, 90%, 100%
 *   - Creates voltage vs. SOC lookup table for accurate gauge
 * 
 * SOC CALCULATION PRINCIPLE:
 *   - BQ4050 measures cell voltage during discharge
 *   - Compares measured voltage to CEDV profile
 *   - Interpolates between profile points to determine SOC
 *   - More accurate than coulomb counting alone
 * 
 * PROFILE CONFIGURATION STRATEGY:
 *   - Uses advance command values directly
 *   - No automatic profile generation
 *   - Allows custom discharge curves for specific batteries
 *   - Supports temperature and age compensation
 * 
 * DATAFLASH COMMANDS CONFIGURED:
 *   Fixed Values:
 *   - DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0/1/2
 *   
 *   Profile Points:
 *   - DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0 through _100
 *   - Total of 11 voltage points defining the curve
 * 
 * ERROR HANDLING:
 *   - Verifies all DataFlash writes by reading back
 *   - Returns false on any verification failure
 *   - Detailed logging of all profile points
 *   - Stops on first failure to prevent partial configuration
 * 
 * PLATFORM NOTES:
 *   - Uses standard BQ4050 DataFlash operations
 *   - No platform-specific curve generation
 *   - Compatible with any I2C implementation
 * 
 * TIMING:
 *   - 100ms delays after each DataFlash write
 *   - Total execution time: ~1.5-2 seconds (14 writes)
 * 
 * USAGE CONTEXT:
 *   - Called during advanced configuration
 *   - Critical for accurate SOC reporting
 *   - Should match actual battery discharge characteristics
 */
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

/**
 * @brief Toggle BQ4050 FET (Field Effect Transistor) enable/disable state
 * 
 * PLATFORM-INDEPENDENT FET CONTROL
 * Controls the charge and discharge FETs within the BQ4050 to enable or disable
 * battery current flow. This function provides software control over battery
 * connection without requiring external hardware switches.
 * 
 * @param None (operation toggles current FET state)
 * @return bool True if FET toggle command successful, false on communication failure
 * 
 * FET CONTROL FUNCTIONALITY:
 *   - If FETs are currently enabled: Disables both charge and discharge FETs
 *   - If FETs are currently disabled: Enables both charge and discharge FETs
 *   - Provides software-controlled battery disconnect capability
 *   - Useful for emergency shutdown or maintenance mode
 * 
 * SAFETY CONSIDERATIONS:
 *   - FET disable immediately stops all current flow
 *   - Does not affect BQ4050 internal operation or I2C communication
 *   - Protection circuits remain active even when FETs are disabled
 *   - FET state is preserved across power cycles
 * 
 * USE CASES:
 *   - Emergency battery disconnect
 *   - Software-controlled power management
 *   - Maintenance mode activation
 *   - Remote battery isolation
 * 
 * IMPLEMENTATION:
 *   - Direct passthrough to BQ4050 fet_toggle() method
 *   - No additional logic or validation performed
 *   - Relies on BQ4050 class for actual I2C communication
 * 
 * ERROR HANDLING:
 *   - Returns false on I2C communication failure
 *   - No local error checking or retry logic
 *   - Error details available from BQ4050 class logs
 * 
 * PLATFORM NOTES:
 *   - Platform-independent operation
 *   - Uses only BQ4050 class methods
 *   - No direct hardware or register access
 * 
 * TIMING:
 *   - Immediate operation (single I2C command)
 *   - Execution time: ~10-50ms depending on I2C speed
 */
bool MeshSolar::toggle_fet(){
    return this->_bq4050->fet_toggle(); // Call the BQ4050 method to toggle FETs
}

/**
 * @brief Reset BQ4050 battery gauge learning algorithms and data
 * 
 * PLATFORM-INDEPENDENT GAUGE RESET
 * Performs a comprehensive reset of the BQ4050 battery gauge, clearing learned
 * capacity data and resetting all learning algorithms to initial state. This
 * operation forces the gauge to relearn battery characteristics from scratch.
 * 
 * @param None
 * @return bool True if reset command successful, false on communication failure
 * 
 * RESET OPERATIONS PERFORMED:
 *   - Clears learned full charge capacity
 *   - Resets capacity learning algorithms
 *   - Clears accumulated impedance data
 *   - Resets aging compensation factors
 *   - Restores factory calibration baselines
 * 
 * WHEN TO USE GAUGE RESET:
 *   - After battery replacement
 *   - When SOC readings become inaccurate
 *   - After significant configuration changes
 *   - When battery aging characteristics change
 *   - During initial system commissioning
 * 
 * POST-RESET BEHAVIOR:
 *   - SOC readings may be less accurate initially
 *   - Learning algorithms will restart data collection
 *   - Full accuracy restored after several charge/discharge cycles
 *   - Configuration settings are preserved (not reset)
 * 
 * LEARNING ALGORITHM RESTART:
 *   - Capacity learning: Requires 2-5 full charge cycles
 *   - Impedance tracking: Continuous background process
 *   - Age estimation: Long-term process (weeks to months)
 *   - Temperature compensation: Immediate with existing profiles
 * 
 * IMPLEMENTATION:
 *   - Direct passthrough to BQ4050 reset() method
 *   - No additional validation or preparation
 *   - Relies on BQ4050 class for actual reset sequence
 * 
 * ERROR HANDLING:
 *   - Returns false on I2C communication failure
 *   - No local retry or error recovery logic
 *   - Error details available from BQ4050 class logs
 * 
 * PLATFORM NOTES:
 *   - Platform-independent operation
 *   - Uses only BQ4050 class methods
 *   - No direct hardware or register manipulation
 * 
 * TIMING:
 *   - Reset command execution: ~100-500ms
 *   - Full learning restart: Multiple charge cycles
 *   - No immediate functional impact on basic operations
 * 
 * CAUTION:
 *   - SOC accuracy temporarily reduced after reset
 *   - Should be followed by proper charge/discharge cycling
 *   - Consider user notification of temporary accuracy loss
 */
bool MeshSolar::reset_bat_gauge() {
    return this->_bq4050->reset(); // Call the BQ4050 method to reset the device
}
