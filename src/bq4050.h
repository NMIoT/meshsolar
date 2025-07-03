#ifndef BQ4050_H
#define BQ4050_H
#include "SoftwareWire.h"

#define dbgSerial           Serial2
#define comSerial           Serial

#define BQ4050ADDR          0x0B


#define BLOCK_ACCESS_CMD            (0x44)

#define BQ4050_REG_CAPACITY_ALARM   0x01 // Remaining Capacity Alarm
#define BQ4050_REG_TIME_ALARM       0x02 // Remaining Time Alarm
#define BQ4050_REG_BAT_MODE         0x03 // Battery Mode
#define BQ4050_REG_TEMP             0x08
#define BQ4050_REG_VOLT             0x09
#define BQ4050_REG_CURRENT          0x0A
#define BQ4050_REG_AVG_CURRENT      0x0B
#define BQ4050_REG_RSOC             0x0D // Relative State of Charge
#define BQ4050_REG_ASOC             0x0E // Absolute State of Charge
#define BQ4050_REG_RC               0x0F // predicted remaining battery capacity
#define BQ4050_REG_FCC              0x10 // Full Charge Capacity
#define BQ4050_REG_ATTE             0x12 // Average Time To Empty
#define BQ4050_REG_ATTF             0x13 // Average Time To Full
#define BQ4050_REG_RMC              0x0F // Remaining Capacity

#define BQ4050_CELL4_VOLTAGE        0x3C // Cell 4 Voltage
#define BQ4050_CELL3_VOLTAGE        0x3D // Cell 3 Voltage
#define BQ4050_CELL2_VOLTAGE        0x3E // Cell 2 Voltage
#define BQ4050_CELL1_VOLTAGE        0x3F // Cell 1 Voltage

#define BQ4050_REG_MAC              0x44

/* ManufacturerAccess */
#define PCHG_FET_Toggle             0x1E
#define CHG_FET_Toggle              0x1F
#define DSG_FET_Toggle              0x20
#define FETcontrol                  0x22

#define MAC_CMD_FW_VER              0x0002
#define MAC_CMD_HW_VER              0x0003
#define MAC_CMD_FET_CONTROL         0x0022
#define MAC_CMD_DEV_RESET           0x0041
#define MAC_CMD_SECURITY_KEYS       0x0035
#define MAC_CMD_DA_STATUS2          0x0072
#define DF_CMD_DEVICE_NAME          0x4085
#define DF_CMD_MANUFACTURER_NAME    0x4070
#define DF_CMD_DA_CONFIGURATION     0x457b
#define DF_CMD_LEARNED_CAPACITY     0x4100


class BQ4050{
private:
    SoftwareWire *wire;
    uint8_t crctable[256];
    bool printResults ; // Set to true to print CRC results to the debug serial

    uint8_t devAddr;

    void crc8_tab_init();
    uint8_t compute_crc8(uint8_t *bytes, int byteLen);


public:
    BQ4050(bool printResults = false){
        this->printResults = printResults;
    }
    ~BQ4050() {
        this->wire->end();
        delete this->wire;
    }

    void begin(SoftwareWire *pwire, uint8_t devaddr = BQ4050ADDR) {
        crc8_tab_init();
        this->wire = pwire;
        this->devAddr = devaddr;
        this->wire->begin();
    }

    uint16_t rd_reg_word(uint8_t reg);
    void wd_reg_word(uint8_t reg, uint16_t value);

    bool rd_mac_block(uint8_t *data, uint8_t arrLen = 32, bool withPEC = false);
    bool rd_df_block(uint16_t cmd,  uint8_t *data, uint8_t arrLen = 32, bool isString = false);
    bool wd_df_block(uint16_t cmd, uint8_t *data, uint8_t arrLen);
    bool wd_mac_cmd(uint16_t cmd);
    



    bool rd_hw_version(uint8_t *data, uint8_t len);
    bool rd_fw_version(uint8_t *data, uint8_t len);
    bool rd_df_dev_name(uint8_t *df, uint8_t len);
    bool rd_df_manufacturer_name(uint8_t *df, uint8_t len);
    bool rd_df_da_configuration(uint8_t *data, uint8_t len);
    bool rd_df_learned_cap(uint8_t *data, uint8_t len);

    bool rd_cell_temp(uint8_t *data, uint8_t len);
    bool fet_toggle();
    bool reset();





    // bool wd_mac_cmd(uint16_t cmd);
    // bool rd_mac_block(uint16_t cmd, uint8_t *data, size_t len);
    // bool rd_reg_word_with_pec(uint8_t reg, uint16_t *value);
    // void wd_reg_word(uint8_t reg, uint16_t value);
    // bool bq4050_read_hw_version();
    // bool rd_fw_version();
    // bool rd_df_dev_name(uint8_t *df, uint8_t len);
    // bool bq4050_fet_control();
    // bool bq4050_reset();



};



#endif // BQ4050_H