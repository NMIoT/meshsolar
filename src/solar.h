#ifndef SOLAR_H
#define SOLAR_H

#include <stdbool.h>
#include "bq4050.h"


// Battery configuration structure
typedef struct {
    char    type[16];          // Battery type string
    int     cell_number;       // Number of battery cells
    int     design_capacity;   // Battery design capacity
    int     cutoff_voltage;    // Battery cutoff voltage
} battery_config_t;

// Temperature protection structure
typedef struct {
    int     high_temp_c;           // High temperature threshold (°C)
    bool    high_temp_enabled;    // High temperature protection enabled
    int     low_temp_c;            // Low temperature threshold (°C)
    bool    low_temp_enabled;     // Low temperature protection enabled
} temp_protection_t;

typedef struct {
    int   cell_num;         // Cell number
    float temperature;      // Cell temperature
    float voltage;          // Cell voltage
} cell_status_t;



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
    char                command[16];              // Command type: "config", "reset", or "switch"
    battery_config_t    battery;      // Battery configuration, valid only for "config" command
    temp_protection_t   temperature_protection; // Temperature protection, valid only for "config" command
    bool fet_en;                   // FET switch, valid only for "switch" command
} meshsolar_cmd_t;

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
    meshsolar_status_t sta;    // Initialize status structure
    meshsolar_cmd_t    cmd;

    MeshSolar();
    ~MeshSolar();
    void begin(BQ4050 *device);

    bool bat_type_setting_update();
    bool bat_model_setting_update();
    bool bat_cells_setting_update ();
    bool bat_design_capacity_setting_update();
    bool bat_cutoff_voltage_setting_update();
    bool bat_voltage_thresholds_setting_update();
    bool bat_charge_over_voltage_setting_update();
    bool bat_discharge_over_heat_setting_update();
    bool bat_charge_over_heat_setting_update();
    bool bat_discharge_low_temp_setting_update();
    bool bat_charge_low_temp_setting_update();
    
    bool bat_fet_toggle();
    bool bat_reset();

    bool get_bat_status();
    
};



#endif // SOLAR_H