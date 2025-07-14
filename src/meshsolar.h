#ifndef __MESHSOLAR_H__
#define __MESHSOLAR_H__

#include <stdbool.h>
#include "bq4050.h"

// Forward declaration for SafetyStatus_t parsing function
String parseSafetyStatusBits(const SafetyStatus_t& safety_status);

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
    bool enable;            // Enable or disable the FET
} fet_config_t;


typedef struct {
    uint16_t    times;          // Number of times to sync
} sync_config_t;


typedef struct {
    int   cell_num;         // Cell number
    float temperature;      // Cell temperature
    float voltage;          // Cell voltage
} cell_status_t;


// MeshSolar command structure
typedef struct {
    char                command[16];  
    basic_config_t      basic;         // Basic configuration
    advance_config_t    advance;       // Advanced configuration
    fet_config_t        fet_en;        // FET enable configuration
    sync_config_t       sync;          // Sync configuration               
} meshsolar_config_t;

typedef struct {
    char            command[16];         // Command type, e.g. "status"
    int             soc_gauge;           // State of charge (%)
    int16_t         charge_current;      // Charge current (mA)
    float           total_voltage;       // Total voltage (V)
    float           learned_capacity;    // Learned capacity (Ah)
    cell_status_t   cells[4];            // Array for cell status (adjust size as needed)
    int             cell_count;          // Number of valid cells in the array
    bool            fet_enable;          // FET enable status
    uint16_t        pack_voltage;        // pack voltage (mV)
    char            protection_sta[128]; // Protection status as parsed bit names string, e.g. "CUV,COV,OTC"
    bool            emergency_shutdown;  // Emergency shutdown status
} meshsolar_status_t;



class MeshSolar{
private:
    BQ4050 *_bq4050;                // Instance of BQ4050 class for battery
public:
    meshsolar_status_t sta;         // Initialize status structure
    meshsolar_config_t cmd;         // Basic and advance command structure
    struct {
        basic_config_t basic;         // Basic configuration
        advance_config_t advance;     // Advanced configuration
    }sync_rsp;

    MeshSolar();
    ~MeshSolar();
    void begin(BQ4050 *device);

    // Basic configuration functions
    bool update_basic_bat_type_setting();
    bool update_basic_bat_model_setting();
    bool update_basic_bat_cells_setting ();
    bool update_basic_bat_design_capacity_setting();
    bool update_basic_bat_discharge_cutoff_voltage_setting();
    bool update_basic_bat_temp_protection_setting(); // Update temperature protection settings

    // Advanced configuration functions
    bool update_advance_bat_battery_setting();
    bool update_advance_bat_cedv_setting();

    bool toggle_fet();
    bool reset_bat_gauge();

    bool get_realtime_bat_status();
    bool get_basic_bat_realtime_setting();
    bool get_advance_bat_realtime_setting();
};



#endif // __MESHSOLAR_H__
