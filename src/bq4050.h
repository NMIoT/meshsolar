#ifndef BQ4050_H
#define BQ4050_H
#include "SoftwareWire.h"

#define dbgSerial           Serial2

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
#define DF_CMD_SBS_DATA_CHEMISTRY   0x409a


#define DF_CMD_GAS_GAUGE_FD_SET_VOLTAGE_THR   0x4458
#define DF_CMD_GAS_GAUGE_FD_CLEAR_VOLTAGE_THR 0x445a

#define DF_CMD_GAS_GAUGE_TD_SET_VOLTAGE_THR   0x4464
#define DF_CMD_GAS_GAUGE_TD_CLEAR_VOLTAGE_THR 0x4466

#define DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV0  0x459d
#define DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV1  0x45a0
#define DF_CMD_GAS_GAUGE_CEDV_CFG_FIXED_EDV2  0x45a3

#define DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH  0x444d        // Design Capacity in mAh
#define DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH  0x444f        // Design Capacity in CWh
#define DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV    0x4451        // Design Capacity in CWh per cell
#define DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY 0x4100 // State Learned Full Capacity

#define DF_CMD_PROTECTIONS_CUV_THR            0x4481
#define DF_CMD_PROTECTIONS_CUV_RECOVERY       0x4484

#define DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR   0x4486
#define DF_CMD_PROTECTIONS_COV_STD_TEMP_THR   0x4488
#define DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR  0x448a
#define DF_CMD_PROTECTIONS_COV_REC_TEMP_THR   0x448c

#define DF_CMD_PROTECTIONS_CUV_RECOVERY_LOW_TEMP_THR   0x448f
#define DF_CMD_PROTECTIONS_CUV_RECOVERY_STD_TEMP_THR   0x4491
#define DF_CMD_PROTECTIONS_CUV_RECOVERY_HIGH_TEMP_THR  0x4493
#define DF_CMD_PROTECTIONS_CUV_RECOVERY_REC_TEMP_THR   0x4495



#define DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL         0x453c
#define DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL         0x4544
#define DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL        0x454c
#define DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL         0x4554




typedef enum{
    NUMBER,
    STRING
}block_type;

typedef struct{
    uint8_t  addr;
    uint16_t value;
}bq4050_reg_t;

typedef struct{
    uint16_t  cmd;     // command to access the data block
    uint8_t   len;     // Length of the data block
    uint8_t   *pvalue; // Pointer to the data block
    block_type type;   // Type of the data block (NUMBER or STRING)
}bq4050_block_t;


class BQ4050{
private:
    SoftwareWire *wire;
    uint8_t crctable[256];
    bool printResults ; // Set to true to print CRC results to the debug serial

    uint8_t devAddr;

    void crc8_tab_init();
    uint8_t compute_crc8(uint8_t *bytes, int byteLen);

    bool _wd_mac_cmd(uint16_t cmd);
    bool _rd_mac_block(bq4050_block_t *block);
    bool _rd_df_block(bq4050_block_t  *block);

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

    bool read_reg_word(bq4050_reg_t *reg);
    bool write_reg_word(bq4050_reg_t reg);

    bool read_mac_block(bq4050_block_t *block);

    bool write_dataflash_block(bq4050_block_t block);
    bool read_dataflash_block (bq4050_block_t *block);
    
    bool fet_toggle();
    bool reset();
};



#endif // BQ4050_H