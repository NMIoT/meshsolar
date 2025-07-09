#ifndef SOLAR_H
#define SOLAR_H

#include <stdbool.h>
#include "bq4050.h"

// Temperature protection structure
typedef struct {
    float   discharge_high_temp_c;       // Discharge high temperature threshold (°C)
    float   charge_high_temp_c;          // Charge high temperature threshold (°C)
    float   discharge_low_temp_c;        // Discharge low temperature threshold (°C)
    float   charge_low_temp_c;           // Charge low temperature threshold (°C)
    bool    enabled;                     // Whether temperature protection is enabled     
} temp_protection_t;

// Battery configuration structure
typedef struct {
    char              type[16];          // Battery type string
    int               cell_number;       // Number of battery cells
    int               design_capacity;   // Battery design capacity
    int               discharge_cutoff_voltage;    // Battery discharge cutoff voltage
    temp_protection_t protection; // Temperature protection settings
} basic_config_t;


// Advanced battery configuration structure
typedef struct {
    int     cuv;               // Cell Under Voltage (mV)
    int     eoc;               // End of Charge voltage (mV)
    int     eoc_protect;       // End of Charge protection voltage (mV)
} advance_battery_config_t;

// CEDV (Charge End-of-Discharge Voltage) configuration structure
typedef struct {
    int     cedv0;             // CEDV at 0% discharge
    int     cedv1;             // CEDV at 1% discharge
    int     cedv2;             // CEDV at 2% discharge
    int     discharge_cedv0;   // Discharge CEDV at 0%
    int     discharge_cedv10;  // Discharge CEDV at 10%
    int     discharge_cedv20;  // Discharge CEDV at 20%
    int     discharge_cedv30;  // Discharge CEDV at 30%
    int     discharge_cedv40;  // Discharge CEDV at 40%
    int     discharge_cedv50;  // Discharge CEDV at 50%
    int     discharge_cedv60;  // Discharge CEDV at 60%
    int     discharge_cedv70;  // Discharge CEDV at 70%
    int     discharge_cedv80;  // Discharge CEDV at 80%
    int     discharge_cedv90;  // Discharge CEDV at 90%
    int     discharge_cedv100; // Discharge CEDV at 100%
} advance_cedv_config_t;

// Advanced command structure
typedef struct {
    advance_battery_config_t    battery;            // Advanced battery configuration
    advance_cedv_config_t       cedv;               // CEDV configuration
} advance_config_t;

typedef struct {
    int   cell_num;         // Cell number
    float temperature;      // Cell temperature
    float voltage;          // Cell voltage
} cell_status_t;




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





// MeshSolar command structure
typedef struct {
    char                command[16];  
    basic_config_t      basic;         
    advance_config_t    advance;
    bool                fet_en;                   
} meshsolar_config_t;

typedef struct {
    char            command[16];         // Command type, e.g. "status"
    int             soc_gauge;           // State of charge (%)
    int16_t         charge_current;      // Charge current (mA)
    float           total_voltage;       // Total voltage (V)
    float           learned_capacity;    // Learned capacity (Ah)
    cell_status_t   cells[4];            // Array for cell status (adjust size as needed)
    int             cell_count;          // Number of valid cells in the array
} meshsolar_status_t;



class MeshSolar{
private:
    BQ4050 *_bq4050;                // Instance of BQ4050 class for battery
public:
    meshsolar_status_t sta;         // Initialize status structure
    meshsolar_config_t cmd;         // Basic command structure
    struct {
        basic_config_t basic;         // Basic configuration
        advance_config_t advance;     // Advanced configuration
    }sync_rsp;

    MeshSolar();
    ~MeshSolar();
    void begin(BQ4050 *device);

    // Basic configuration functions
    bool bat_basic_type_setting_update();
    bool bat_basic_model_setting_update();
    bool bat_basic_cells_setting_update ();
    bool bat_basic_design_capacity_setting_update();
    bool bat_basic_discharge_cutoff_voltage_setting_update();
    bool bat_basic_temp_protection_setting_update(); // Update temperature protection settings

    // Advanced configuration functions
    bool bat_advance_battery_config_update();
    bool bat_advance_cedv_setting_update();

    bool bat_fet_toggle();
    bool bat_reset();

    bool get_bat_realtime_status();
    bool get_bat_realtime_basic_config();
    bool get_bat_realtime_advance_config();
};



#endif // SOLAR_H