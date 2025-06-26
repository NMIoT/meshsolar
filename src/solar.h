#ifndef SOLAR_H
#define SOLAR_H

#include <stdbool.h>

// Battery configuration structure
typedef struct {
    char type[16];         // Battery type string
    int cell_number;       // Number of battery cells
    int design_capacity;   // Battery design capacity
    int cutoff_voltage;    // Battery cutoff voltage
} battery_config_t;

// Temperature protection structure
typedef struct {
    int high_temp_c;           // High temperature threshold (°C)
    bool high_temp_enabled;    // High temperature protection enabled
    int low_temp_c;            // Low temperature threshold (°C)
    bool low_temp_enabled;     // Low temperature protection enabled
} temp_protection_t;

typedef struct {
    int cell_num;         // Cell number
    float temperature;    // Cell temperature
    float voltage;        // Cell voltage
} cell_status_t;









// MeshSolar command structure
typedef struct {
    char command[16];              // Command type: "config", "reset", or "switch"
    battery_config_t battery;      // Battery configuration, valid only for "config" command
    temp_protection_t temperature_protection; // Temperature protection, valid only for "config" command
    bool fet_en;                   // FET switch, valid only for "switch" command
} meshsolar_cmd_t;

typedef struct {
    char command[16];         // Command type, e.g. "status"
    int soc_gauge;            // State of charge (%)
    int16_t charge_current;       // Charge current (mA)
    float total_voltage;      // Total voltage (V)
    float learned_capacity;   // Learned capacity (Ah)
    cell_status_t cells[8];   // Array for cell status (adjust size as needed)
    int cell_count;           // Number of valid cells in the array
} meshsolar_status_t;

#endif // SOLAR_H