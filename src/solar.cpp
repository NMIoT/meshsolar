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
    res = this->_bq4050->rd_reg_word(&reg);
    this->sta.total_voltage  = (res) ? reg.value : this->sta.total_voltage; // Convert from mV to V
    delay(10); 
    /************************************************ get charge current ***********************************************/
    reg.addr = BQ4050_REG_CURRENT; // Register address for charge current
    res = (int16_t)this->_bq4050->rd_reg_word(&reg);
    this->sta.charge_current = (res) ? reg.value : this->sta.charge_current; // Convert from mA to A
    delay(10); 
    /*************************************************** get soc gauge ************************************************/
    reg.addr = BQ4050_REG_RSOC; // Register address for state of charge
    res = this->_bq4050->rd_reg_word(&reg); 
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
        res = this->_bq4050->rd_reg_word(&reg); 
        this->sta.cells[i].voltage = (res) ? reg.value : this->sta.cells[i].voltage; // Convert from mV to V
        delay(10); 
    }

    /**************************************************** get learned capacity ********************************************/
    reg.addr = BQ4050_REG_FCC; 
    res  = this->_bq4050->rd_reg_word(&reg);
    this->sta.learned_capacity = (res) ? reg.value : this->sta.learned_capacity; 

    delay(10); 
    return true; // Return true to indicate status update was successful
}

bool MeshSolar::bat_type_setting_update(){


    return false;
}

bool MeshSolar::bat_model_setting_update() {



    return false;
}


bool MeshSolar::bat_cells_setting_update() {
    uint8_t cells_bits = 0b10;
    bq4050_block_t block = {0, 0, nullptr, NUMBER}; // Initialize block structure for DA configuration
    
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    block.len = 1; // Length of the data block to read
    block.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&block); // Read current DA configuration

    dbgSerial.print("DA Configuration before: 0x");
    dbgSerial.println(block.pvalue[0], HEX);
    delay(100); // Ensure the read is complete before modifying


    if(this->cmd.battery.cell_number == 1)      cells_bits = 0b00;
    else if(this->cmd.battery.cell_number == 2) cells_bits = 0b01;
    else if(this->cmd.battery.cell_number == 3) cells_bits = 0b10;
    else if(this->cmd.battery.cell_number == 4) cells_bits = 0b11;
    else return false; // Invalid cell count, return false

    // Clear the last 2 bits first, then set new cell count bits
    block.pvalue[0] &= 0b11111100; // Clear bits 0 and 1 (last 2 bits)
    block.pvalue[0] |= cells_bits;  // Set new cell count bits
    block.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    this->_bq4050->write_dataflash_block(&block);

    delay(100); // Ensure the read is complete before modifying


    bq4050_block_t ret;

    memset(&ret, 0, sizeof(ret)); // Reset block structure
    ret.cmd = DF_CMD_DA_CONFIGURATION; // Command to access DA configuration
    ret.len = 1; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER


    this->_bq4050->read_dataflash_block(&ret); // Read current DA configuration again
    dbgSerial.print("DA Configuration after: 0x");
    dbgSerial.println(ret.pvalue[0], HEX);

    return (ret.pvalue[0] == block.pvalue[0]); // Return true if the DA configuration was set correctly
}

bool MeshSolar::bat_design_capacity_setting_update(){
    // Set the battery capacity in mAh
    bq4050_block_t block = {
        DF_CMD_LEARNED_CAPACITY, 
        2, 
        (uint8_t*)&this->cmd.battery.design_capacity, 
        NUMBER
    }; // Initialize block structure for DA configuration
    
    this->_bq4050->write_dataflash_block(&block);
    delay(100); // Ensure the write is complete before reading

    bq4050_block_t ret = {0, 0, nullptr, NUMBER}; // Initialize block structure for learned capacity
    ret.cmd = DF_CMD_LEARNED_CAPACITY; // Command to access learned capacity
    ret.len = 2; // Length of the data block to read
    ret.type = NUMBER; // Set block type to NUMBER

    this->_bq4050->read_dataflash_block(&ret);
    dbgSerial.print(" Learned Capacity after: ");
    dbgSerial.print(ret.pvalue[0] | (ret.pvalue[1] << 8), DEC); // Combine the two bytes into a single value
    dbgSerial.println(" mAh");

    return (this->cmd.battery.design_capacity == *(uint16_t*)(ret.pvalue)); // Return true if the design capacity was set correctly
}

bool MeshSolar::bat_cutoff_voltage_setting_update(){


    return false;
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
