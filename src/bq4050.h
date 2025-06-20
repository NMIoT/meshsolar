#ifndef BQ4050_H
#define BQ4050_H

#define SDA_PIN 33
#define SCL_PIN 32

#define BQ4050addr                  (0x0B)
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
#define MAC_CMD_SECURITY_KEYS       0x0035

#endif // BQ4050_H