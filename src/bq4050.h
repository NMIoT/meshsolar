#ifndef BQ4050_H
#define BQ4050_H
#include "SoftwareWire.h"

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
#define MAC_CMD_SAFETY_STATUS       0x0051
#define MAC_CMD_OPERATION_STATUS    0x0054
#define MAC_CMD_MANUFACTURER_STATUS 0x0057
#define MAC_CMD_DA_STATUS1          0x0071
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

#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_0    0x45a6 // CEDV Profile 1 Voltage 0
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_10   0x45a8 // CEDV Profile 1 Voltage 10
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_20   0x45aa // CEDV Profile 1 Voltage 20
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_30   0x45ac // CEDV Profile 1 Voltage 30
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_40   0x45ae // CEDV Profile 1 Voltage 40
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_50   0x45b0 // CEDV Profile 1 Voltage 50
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_60   0x45b2 // CEDV Profile 1 Voltage 60
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_70   0x45b4 // CEDV Profile 1 Voltage 70
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_80   0x45b6 // CEDV Profile 1 Voltage 80
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_90   0x45b8 // CEDV Profile 1 Voltage 90
#define DF_CMD_GAS_GAUGE_CEDV_PROFILE1_VOLTAGE_100  0x45ba // CEDV Profile 1 Voltage 100


#define DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_MAH  0x444d        // Design Capacity in mAh
#define DF_CMD_GAS_GAUGE_DESIGN_CAPACITY_CWH  0x444f        // Design Capacity in CWh
#define DF_CMD_GAS_GAUGE_DESIGN_VOLTAGE_MV    0x4451        // Design Capacity in CWh per cell
#define DF_CMD_GAS_GAUGE_STATE_LEARNED_FULL_CAPACITY 0x4100 // State Learned Full Capacity

#define DF_CMD_PROTECTIONS_CUV_THR            0x4481
#define DF_CMD_PROTECTIONS_CUV_RECOVERY       0x4484

#define DF_CMD_PROTECTIONS_COV_THR            0x4481
#define DF_CMD_PROTECTIONS_COV_RECOVERY       0x4484

#define DF_CMD_PROTECTIONS_COV_LOW_TEMP_THR   0x4486
#define DF_CMD_PROTECTIONS_COV_STD_TEMP_THR   0x4488
#define DF_CMD_PROTECTIONS_COV_HIGH_TEMP_THR  0x448a
#define DF_CMD_PROTECTIONS_COV_REC_TEMP_THR   0x448c

#define DF_CMD_PROTECTIONS_COV_LOW_TEMP_RECOVERY  0x448f // recovery low temperature threshold
#define DF_CMD_PROTECTIONS_COV_STD_TEMP_RECOVERY  0x4491 // recovery standard temperature threshold
#define DF_CMD_PROTECTIONS_COV_HIGH_TEMP_RECOVERY 0x4493 // recovery high temperature threshold
#define DF_CMD_PROTECTIONS_COV_REC_TEMP_RECOVERY  0x4495 // recovery temperature threshold

#define DF_CMD_PROTECTIONS_OTD_THR                     0x44ba //discharge over temperature threshold
#define DF_CMD_PROTECTIONS_OTD_RECOVERY                0x44bd //discharge over temperature recovery
#define DF_CMD_PROTECTIONS_OTC_THR                     0x44b5// charge over temperature threshold
#define DF_CMD_PROTECTIONS_OTC_RECOVERY                0x44b8// charge over temperature recovery

#define DF_CMD_PROTECTIONS_UTD_THR                     0x44c9 //discharge over temperature threshold
#define DF_CMD_PROTECTIONS_UTD_RECOVERY                0x44cc //discharge over temperature recovery
#define DF_CMD_PROTECTIONS_UTC_THR                     0x44c4 // charge over temperature threshold
#define DF_CMD_PROTECTIONS_UTC_RECOVERY                0x44c7 // charge over temperature recovery

#define DF_CMD_SETTINGS_PROTECTIONS_ENABLE_A           0x447d // Enable/Disable protections A
#define DF_CMD_SETTINGS_PROTECTIONS_ENABLE_B           0x447e // Enable/Disable protections B
#define DF_CMD_SETTINGS_PROTECTIONS_ENABLE_C           0x447f // Enable/Disable protections C
#define DF_CMD_SETTINGS_PROTECTIONS_ENABLE_D           0x4480 // Enable/Disable protections D


#define DF_CMD_ADVANCED_CHARGE_ALG_LOW_TEMP_CHARG_VOL         0x453c
#define DF_CMD_ADVANCED_CHARGE_ALG_STD_TEMP_CHARG_VOL         0x4544
#define DF_CMD_ADVANCED_CHARGE_ALG_HIGH_TEMP_CHARG_VOL        0x454c
#define DF_CMD_ADVANCED_CHARGE_ALG_REC_TEMP_CHARG_VOL         0x4554

typedef struct {
    union {
        uint32_t bytes;                    // 32-bit raw data
        struct {
            // Bits 0-15 (Low word)
            uint32_t cuv    : 1;           // bit 0:  Cell Under Voltage
            uint32_t cov    : 1;           // bit 1:  Cell Over Voltage  
            uint32_t occ1   : 1;           // bit 2:  Overcurrent During Charge 1
            uint32_t occ2   : 1;           // bit 3:  Overcurrent During Charge 2
            uint32_t ocd1   : 1;           // bit 4:  Overcurrent During Discharge 1
            uint32_t ocd2   : 1;           // bit 5:  Overcurrent During Discharge 2
            uint32_t aold   : 1;           // bit 6:  Overload During Discharge
            uint32_t aoldl  : 1;           // bit 7:  Overload During Discharge Latch
            uint32_t ascc   : 1;           // bit 8:  Short-circuit During Charge
            uint32_t ascl   : 1;           // bit 9:  Short-circuit During Charge Latch
            uint32_t ascd   : 1;           // bit 10: Short-circuit During Discharge
            uint32_t ascdl  : 1;           // bit 11: Short-circuit During Discharge Latch
            uint32_t otc    : 1;           // bit 12: Overtemperature During Charge
            uint32_t otd    : 1;           // bit 13: Overtemperature During Discharge
            uint32_t cuvc   : 1;           // bit 14: Cell Undervoltage Compensated
            uint32_t        : 1;           // bit 15: Reserved
            
            // Bits 16-31 (High word)
            uint32_t otf    : 1;           // bit 16: Overtemperature FET
            uint32_t        : 1;           // bit 17: Reserved
            uint32_t pto    : 1;           // bit 18: Precharge Timeout
            uint32_t        : 1;           // bit 19: Reserved
            uint32_t cto    : 1;           // bit 20: Charge Timeout
            uint32_t        : 1;           // bit 21: Reserved
            uint32_t oc     : 1;           // bit 22: Overcharge
            uint32_t chgc   : 1;           // bit 23: Overcharging Current
            uint32_t chgv   : 1;           // bit 24: Overcharging Voltage
            uint32_t pchgc  : 1;           // bit 25: Over-Precharge Current
            uint32_t utc    : 1;           // bit 26: Undertemperature During Charge
            uint32_t utd    : 1;           // bit 27: Undertemperature During Discharge
            uint32_t        : 4;           // bit 28-31: Reserved
        } bits;
    };
} SafetyStatus_t;

typedef struct {
    union {
        uint32_t bytes;                    // 32-bit raw data
        struct {
            // Bits 0-15 (Low word)
            uint32_t pres       : 1;       // bit 0: System present low
            uint32_t dsg        : 1;       // bit 1: DSG FET status
            uint32_t chg        : 1;       // bit 2: CHG FET status
            uint32_t pchg       : 1;       // bit 3: Precharge FET status
            uint32_t            : 1;       // bit 4: Reserved
            uint32_t fuse       : 1;       // bit 5: Fuse status
            uint32_t smooth     : 1;       // bit 6: Smoothing active status
            uint32_t btp_int    : 1;       // bit 7: Battery Trip Point Interrupt
            uint32_t sec0       : 1;       // bit 8: SECURITY mode bit 0
            uint32_t sec1       : 1;       // bit 9: SECURITY mode bit 1
            uint32_t sdv        : 1;       // bit 10: Shutdown triggered via low pack voltage
            uint32_t ss         : 1;       // bit 11: SAFETY mode status
            uint32_t pf         : 1;       // bit 12: PERMANENT FAILURE mode status
            uint32_t xdsg       : 1;       // bit 13: Discharging disabled
            uint32_t xchg       : 1;       // bit 14: Charging disabled
            uint32_t sleep      : 1;       // bit 15: SLEEP mode conditions met
            
            // Bits 16-31 (High word)
            uint32_t sdm        : 1;       // bit 16: Shutdown triggered via command
            uint32_t led        : 1;       // bit 17: LED Display
            uint32_t auth       : 1;       // bit 18: Authentication in progress
            uint32_t autocalm   : 1;       // bit 19: Auto CC Offset Calibration
            uint32_t cal        : 1;       // bit 20: Calibration Output (raw ADC and CC data)
            uint32_t cal_offset : 1;       // bit 21: Calibration Output (raw CC offset data)
            uint32_t xl         : 1;       // bit 22: 400-kHz SMBus mode
            uint32_t sleepm     : 1;       // bit 23: SLEEP mode triggered via command
            uint32_t init       : 1;       // bit 24: Initialization after full reset
            uint32_t smblcal    : 1;       // bit 25: Auto CC calibration when bus is low
            uint32_t slpad      : 1;       // bit 26: ADC Measurement in SLEEP mode
            uint32_t slpcc      : 1;       // bit 27: CC Measurement in SLEEP mode
            uint32_t cb         : 1;       // bit 28: Cell balancing status
            uint32_t emshut     : 1;       // bit 29: Emergency Shutdown
            uint32_t            : 2;       // bit 30-31: Reserved
        } bits;
    };
} OperationStatus_t;

typedef struct {
    uint16_t cell_1_voltage;  // Cell 1 voltage (mV)
    uint16_t cell_2_voltage;  // Cell 2 voltage (mV)
    uint16_t cell_3_voltage;  // Cell 3 voltage (mV)
    uint16_t cell_4_voltage;  // Cell 4 voltage (mV)
    uint16_t bat_voltage;     // Battery voltage (mV)
    uint16_t pack_voltage;    // Pack voltage (mV)
    uint16_t cell_1_current;  // Cell 1 current (mA)
    uint16_t cell_2_current;  // Cell 2 current (mA)
    uint16_t cell_3_current;  // Cell 3 current (mA)
    uint16_t cell_4_current;  // Cell 4 current (mA)
    uint16_t cell_1_power;    // Cell 1 power (cW)
    uint16_t cell_2_power;    // Cell 2 power (cW)
    uint16_t cell_3_power;    // Cell 3 power (cW)
    uint16_t cell_4_power;    // Cell 4 power (cW)
    uint16_t power;           // Total power (cW)
    uint16_t avg_power;       // Average power (cW)
} DAStatus1_t;

typedef struct {
    int16_t int_temp;        // Internal temperature (units: 0.1K)
    int16_t ts1_temp;        // Temperature sensor 1 (units: 0.1K)
    int16_t ts2_temp;        // Temperature sensor 2 (units: 0.1K)
    int16_t ts3_temp;        // Temperature sensor 3 (units: 0.1K)
    int16_t ts4_temp;        // Temperature sensor 4 (units: 0.1K)
    int16_t cell1_temp;      // Cell 1 temperature (units: 0.1K)
    int16_t fet_temp;        // FET temperature (units: 0.1K)
} DAStatus2_t;




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