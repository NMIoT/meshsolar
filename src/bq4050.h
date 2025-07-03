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





class BQ4050{
private:
    SoftwareWire *wire;
    uint8_t crctable[256];
    bool printResults ; // Set to true to print CRC results to the debug serial

    uint8_t devAddr;
    uint8_t crc8_tab[256];

    void crc8_tab_init();
    uint8_t compute_crc8(uint8_t *bytes, int byteLen);


public:
    BQ4050(SoftwareWire *wire, uint8_t devaddr = BQ4050ADDR, bool printResults = false)
        : wire(wire), crctable{0}, printResults(printResults), devAddr(devaddr), crc8_tab{0}
    {
        crc8_tab_init();
        wire->begin();
    }
    ~BQ4050() {
        wire->end();
        delete wire; // Clean up the SoftwareWire instance
    }


    uint16_t bq4050_rd_word(uint8_t reg);
    bool bq4050_rd_word_with_pec(uint8_t reg, uint16_t *value); 
    void bq4050_wd_word(uint8_t reg, uint16_t value);


    // bool writeMACommand(uint16_t cmd);
    // bool readBQ4050BlockData(uint16_t cmd, uint8_t *data, size_t len);
    // bool bq4050_rd_word_with_pec(uint8_t reg, uint16_t *value);
    // void bq4050_wd_word(uint8_t reg, uint16_t value);
    // bool bq4050_read_hw_version();
    // bool bq4050_read_fw_version();
    // bool bq4050_df_dev_name_read(uint8_t *df, uint8_t len);
    // bool bq4050_fet_control();
    // bool bq4050_reset();



};



#endif // BQ4050_H