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
    // update status structure with battery information
    this->sta.total_voltage  = this->_bq4050->rd_reg_word(BQ4050_REG_VOLT);
    delay(10); 

    this->sta.charge_current = (int16_t)this->_bq4050->rd_reg_word(BQ4050_REG_CURRENT);
    delay(10); 

    this->sta.soc_gauge      = this->_bq4050->rd_reg_word(BQ4050_REG_RSOC); 
    delay(10); 

    uint8_t da = 0;
    this->_bq4050->rd_df_da_configuration(&da, 1); // Read current DA configuration again
    if(0 == (da & 0x03))      this->sta.cell_count = 1;
    else if(1 == (da & 0x03)) this->sta.cell_count = 2;
    else if(2 == (da & 0x03)) this->sta.cell_count = 3;
    else if(3 == (da & 0x03)) this->sta.cell_count = 4;
    else this->sta.cell_count = 3; // Default to 3 cells if DA configuration is invalid

    DAStatus2_t temperature = {0,};
    uint8_t data[14] = {0,};// 14 bytes for cell temperatures based on bq4050 documentation
    this->_bq4050->rd_cell_temp(data, sizeof(data));
    memcpy(&temperature, data, sizeof(DAStatus2_t)); // Copy the data into the temperature structure
    delay(10); 

    this->sta.cells[0].temperature = (this->sta.cell_count >= 1) ? temperature.ts1_temp / 10.0f - 273.15f : 0.0f;// Convert from Kelvin to Celsius
    this->sta.cells[1].temperature = (this->sta.cell_count >= 2) ? temperature.ts2_temp / 10.0f - 273.15f : 0.0f;
    this->sta.cells[2].temperature = (this->sta.cell_count >= 3) ? temperature.ts3_temp / 10.0f - 273.15f : 0.0f;
    this->sta.cells[3].temperature = (this->sta.cell_count >= 4) ? temperature.ts4_temp / 10.0f - 273.15f : 0.0f;

    // update battery cell voltage
    for (uint8_t i = 0; i < this->sta.cell_count; i++) {
        this->sta.cells[i].cell_num = i + 1;
        uint8_t cell_reg = BQ4050_CELL1_VOLTAGE - i;// Cell1=0x3F, Cell2=0x3E, Cell3=0x3D, Cell4=0x3C
        this->sta.cells[i].voltage = this->_bq4050->rd_reg_word(cell_reg); 
        delay(10); 
    }

    // update learned capacity
    this->sta.learned_capacity = this->_bq4050->rd_reg_word(BQ4050_REG_FCC); 
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
    uint8_t da = 0, cells_bits = 0b10;
    this->_bq4050->rd_df_da_configuration(&da, 1); // Read current DA configuration
    dbgSerial.print("DA Configuration before: 0x");
    dbgSerial.println(da, HEX);
    delay(100); // Ensure the read is complete before modifying


    if(this->cmd.battery.cell_number == 1)      cells_bits = 0b00;
    else if(this->cmd.battery.cell_number == 2) cells_bits = 0b01;
    else if(this->cmd.battery.cell_number == 3) cells_bits = 0b10;
    else if(this->cmd.battery.cell_number == 4) cells_bits = 0b11;
    else return false; // Invalid cell count, return false

    // Clear the last 2 bits first, then set new cell count bits
    da &= 0b11111100; // Clear bits 0 and 1 (last 2 bits)
    da |= cells_bits;  // Set new cell count bits
    this->_bq4050->wd_df_block(DF_CMD_DA_CONFIGURATION, &da, 1);

    delay(100); // Ensure the read is complete before modifying

    uint8_t ret = 0;
    this->_bq4050->rd_df_da_configuration(&ret, 1); // Read current DA configuration again
    dbgSerial.print("DA Configuration after: 0x");
    dbgSerial.println(ret, HEX);

    return (ret == da); 
}

bool MeshSolar::bat_design_capacity_setting_update(){
    // Set the battery capacity in mAh
    uint16_t design_cap = this->cmd.battery.design_capacity;
    this->_bq4050->wd_df_block(DF_CMD_LEARNED_CAPACITY, (uint8_t*)&design_cap, 2);
    delay(100); // Ensure the write is complete before reading

    uint16_t ret = 0;
    this->_bq4050->rd_df_learned_cap((uint8_t*)&ret, 2);
    dbgSerial.print(" Learned Capacity after: ");
    dbgSerial.print(ret);
    dbgSerial.println(" mAh");

    return (design_cap == ret); // Return true if the design capacity was set correctly
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


