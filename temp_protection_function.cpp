bool MeshSolar::bat_temp_protection_setting_update() {
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

    // Configuration table: {command, value_source, description}
    struct temp_config_entry_t {
        uint16_t cmd;
        int16_t value;
        const char* name;
    };

    // Get temperature values from configuration (convert to BQ4050 format: Kelvin * 10)
    // BQ4050 expects temperature in 0.1K units (e.g., 25°C = 2981 = (25 + 273.15) * 10)
    int16_t charge_high_temp = (this->cmd.basic.protection.charge_high_temp_c + 273) * 10;
    int16_t charge_low_temp = (this->cmd.basic.protection.charge_low_temp_c + 273) * 10;
    int16_t discharge_high_temp = (this->cmd.basic.protection.discharge_high_temp_c + 273) * 10;
    int16_t discharge_low_temp = (this->cmd.basic.protection.discharge_low_temp_c + 273) * 10;
    
    // Recovery temperatures (typically 5°C hysteresis for stability)
    int16_t charge_high_recovery = charge_high_temp - 50;    // 5°C lower than threshold
    int16_t charge_low_recovery = charge_low_temp + 50;      // 5°C higher than threshold
    int16_t discharge_high_recovery = discharge_high_temp - 50;
    int16_t discharge_low_recovery = discharge_low_temp + 50;

    temp_config_entry_t configurations[] = {
        // Charge temperature protection
        {DF_CMD_PROTECTIONS_OTC_THR,      charge_high_temp,      "Charge Over Temperature Threshold"},
        {DF_CMD_PROTECTIONS_OTC_RECOVERY, charge_high_recovery,  "Charge Over Temperature Recovery"},
        {DF_CMD_PROTECTIONS_UTC_THR,      charge_low_temp,       "Charge Under Temperature Threshold"},
        {DF_CMD_PROTECTIONS_UTC_RECOVERY, charge_low_recovery,   "Charge Under Temperature Recovery"},
        
        // Discharge temperature protection
        {DF_CMD_PROTECTIONS_OTD_THR,      discharge_high_temp,      "Discharge Over Temperature Threshold"},
        {DF_CMD_PROTECTIONS_OTD_RECOVERY, discharge_high_recovery,  "Discharge Over Temperature Recovery"},
        {DF_CMD_PROTECTIONS_UTD_THR,      discharge_low_temp,       "Discharge Under Temperature Threshold"},
        {DF_CMD_PROTECTIONS_UTD_RECOVERY, discharge_low_recovery,   "Discharge Under Temperature Recovery"}
    };

    // Helper lambda function to write and verify temperature setting
    auto write_and_verify_temp = [&](uint16_t cmd, int16_t value, const char* name) -> bool {
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

        int16_t read_value = ret.pvalue[0] | (ret.pvalue[1] << 8);
        float temp_celsius = (read_value / 10.0f) - 273.15f;
        
        dbgSerial.print(name);
        dbgSerial.print(" set to: ");
        dbgSerial.print(temp_celsius, 1);
        dbgSerial.print(" °C (");
        dbgSerial.print(read_value);
        dbgSerial.print(" raw)");
        
        if (value == read_value) {
            dbgSerial.println(" - OK");
            return true;
        } else {
            float expected_celsius = (value / 10.0f) - 273.15f;
            dbgSerial.print(" - ERROR (expected ");
            dbgSerial.print(expected_celsius, 1);
            dbgSerial.print(" °C, ");
            dbgSerial.print(value);
            dbgSerial.println(" raw)");
            return false;
        }
    };

    // Apply all temperature protection configurations
    for(auto& cfg : configurations) {
        res &= write_and_verify_temp(cfg.cmd, cfg.value, cfg.name);
    }

    // Print configuration summary
    if (res) {
        dbgSerial.println("\nTemperature Protection Configuration Summary:");
        dbgSerial.print("  Charge: ");
        dbgSerial.print(this->cmd.basic.protection.charge_low_temp_c);
        dbgSerial.print("°C to ");
        dbgSerial.print(this->cmd.basic.protection.charge_high_temp_c);
        dbgSerial.println("°C");
        
        dbgSerial.print("  Discharge: ");
        dbgSerial.print(this->cmd.basic.protection.discharge_low_temp_c);
        dbgSerial.print("°C to ");
        dbgSerial.print(this->cmd.basic.protection.discharge_high_temp_c);
        dbgSerial.println("°C");
        
        dbgSerial.print("  Protection enabled: ");
        dbgSerial.println(this->cmd.basic.protection.enabled ? "Yes" : "No");
    }

    return res;
}
