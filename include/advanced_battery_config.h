/*
 * 高级电池配置示例
 * 展示如何为不同应用场景定制温度补偿配置
 */

#ifndef ADVANCED_BATTERY_CONFIG_H
#define ADVANCED_BATTERY_CONFIG_H

#include <stdint.h>

// 应用场景枚举
enum application_type_t {
    SOLAR_STORAGE,      // 太阳能储能系统
    UPS_BACKUP,         // UPS备用电源
    VEHICLE_POWER,      // 车载电源
    PORTABLE_DEVICE     // 便携设备
};

// 气候环境枚举
enum climate_zone_t {
    TROPICAL,           // 热带 (20°C - 35°C)
    TEMPERATE,          // 温带 (-10°C - 40°C)
    ARCTIC,             // 极地 (-40°C - 20°C)
    DESERT              // 沙漠 (-10°C - 55°C)
};

// 电池品质等级
enum battery_grade_t {
    CONSUMER,           // 消费级
    INDUSTRIAL,         // 工业级
    AUTOMOTIVE,         // 车规级
    AEROSPACE           // 航空航天级
};

// 扩展的温度电压配置
struct extended_temp_config_t {
    uint16_t extreme_low;   // 极低温 (< -20°C)
    uint16_t low;           // 低温 (-20°C to 0°C)
    uint16_t standard;      // 标准 (0°C to 45°C)
    uint16_t high;          // 高温 (45°C to 60°C)
    uint16_t extreme_high;  // 极高温 (> 60°C)
    uint16_t recovery;      // 恢复状态
};

// 完整的电池配置档案
struct battery_profile_t {
    const char* name;
    const char* chemistry;
    application_type_t application;
    climate_zone_t climate;
    battery_grade_t grade;
    
    extended_temp_config_t charge_voltage;
    extended_temp_config_t cov_threshold;
    extended_temp_config_t cov_recovery;
    
    // 温度阈值 (摄氏度)
    int8_t temp_extreme_low;
    int8_t temp_low;
    int8_t temp_standard_min;
    int8_t temp_standard_max;
    int8_t temp_high;
    int8_t temp_extreme_high;
    
    // 其他参数
    uint16_t max_charge_current_ma;
    uint16_t max_discharge_current_ma;
    uint8_t  charge_termination_current_percent;
};

// 预定义的电池配置档案
class BatteryProfiles {
public:
    // 太阳能储能系统 - LiFePO4 工业级
    static constexpr battery_profile_t SOLAR_LIFEPO4_INDUSTRIAL = {
        .name = "Solar LiFePO4 Industrial",
        .chemistry = "lifepo4",
        .application = SOLAR_STORAGE,
        .climate = TEMPERATE,
        .grade = INDUSTRIAL,
        
        .charge_voltage = {3400, 3500, 3600, 3550, 3450, 3580},
        .cov_threshold =  {3550, 3650, 3750, 3700, 3600, 3730},
        .cov_recovery =   {3400, 3500, 3600, 3550, 3450, 3580},
        
        .temp_extreme_low = -30,
        .temp_low = -20,
        .temp_standard_min = 0,
        .temp_standard_max = 45,
        .temp_high = 60,
        .temp_extreme_high = 70,
        
        .max_charge_current_ma = 5000,
        .max_discharge_current_ma = 10000,
        .charge_termination_current_percent = 5
    };
    
    // 便携设备 - Li-ion 消费级
    static constexpr battery_profile_t PORTABLE_LIION_CONSUMER = {
        .name = "Portable Li-ion Consumer",
        .chemistry = "liion",
        .application = PORTABLE_DEVICE,
        .climate = TEMPERATE,
        .grade = CONSUMER,
        
        .charge_voltage = {4000, 4100, 4200, 4150, 4050, 4180},
        .cov_threshold =  {4100, 4200, 4300, 4250, 4150, 4280},
        .cov_recovery =   {3900, 4000, 4100, 4050, 3950, 4080},
        
        .temp_extreme_low = -20,
        .temp_low = -10,
        .temp_standard_min = 0,
        .temp_standard_max = 40,
        .temp_high = 50,
        .temp_extreme_high = 60,
        
        .max_charge_current_ma = 2000,
        .max_discharge_current_ma = 3000,
        .charge_termination_current_percent = 10
    };
    
    // 车载电源 - LiFePO4 车规级
    static constexpr battery_profile_t VEHICLE_LIFEPO4_AUTOMOTIVE = {
        .name = "Vehicle LiFePO4 Automotive",
        .chemistry = "lifepo4",
        .application = VEHICLE_POWER,
        .climate = TEMPERATE,
        .grade = AUTOMOTIVE,
        
        .charge_voltage = {3450, 3550, 3650, 3600, 3500, 3620},
        .cov_threshold =  {3600, 3700, 3800, 3750, 3650, 3780},
        .cov_recovery =   {3450, 3550, 3650, 3600, 3500, 3620},
        
        .temp_extreme_low = -40,
        .temp_low = -30,
        .temp_standard_min = -10,
        .temp_standard_max = 50,
        .temp_high = 65,
        .temp_extreme_high = 80,
        
        .max_charge_current_ma = 20000,
        .max_discharge_current_ma = 50000,
        .charge_termination_current_percent = 3
    };
};

// 根据温度获取对应的电压值
class TemperatureVoltageMapper {
public:
    static uint16_t getVoltageForTemperature(const extended_temp_config_t& config, 
                                           const battery_profile_t& profile, 
                                           int8_t current_temp) {
        if (current_temp < profile.temp_extreme_low) {
            return config.extreme_low;
        } else if (current_temp < profile.temp_low) {
            return config.low;
        } else if (current_temp >= profile.temp_standard_min && 
                   current_temp <= profile.temp_standard_max) {
            return config.standard;
        } else if (current_temp <= profile.temp_high) {
            return config.high;
        } else if (current_temp <= profile.temp_extreme_high) {
            return config.extreme_high;
        } else {
            // 超出安全温度范围，返回最保守的值
            return config.extreme_low;
        }
    }
    
    // 获取适合当前环境的电压配置
    static battery_profile_t adjustForClimate(const battery_profile_t& base_profile, 
                                             climate_zone_t target_climate) {
        battery_profile_t adjusted = base_profile;
        adjusted.climate = target_climate;
        
        switch (target_climate) {
            case TROPICAL:
                // 热带地区 - 降低高温阈值，提高低温性能
                adjusted.temp_standard_max = 40;
                adjusted.temp_high = 50;
                break;
                
            case ARCTIC:
                // 极地地区 - 提高低温性能
                adjusted.temp_extreme_low = -50;
                adjusted.temp_low = -40;
                adjusted.temp_standard_min = -20;
                // 降低所有电压以适应极地环境
                adjustVoltagesForExtremeCold(adjusted);
                break;
                
            case DESERT:
                // 沙漠地区 - 极端温度范围
                adjusted.temp_extreme_low = -20;
                adjusted.temp_extreme_high = 70;
                // 更激进的温度降额
                adjustVoltagesForDesert(adjusted);
                break;
                
            case TEMPERATE:
            default:
                // 温带 - 保持默认设置
                break;
        }
        
        return adjusted;
    }

private:
    static void adjustVoltagesForExtremeCold(battery_profile_t& profile) {
        // 极地环境的电压调整 - 全面降低
        auto adjustConfig = [](extended_temp_config_t& config) {
            config.extreme_low = static_cast<uint16_t>(config.extreme_low * 0.90);
            config.low = static_cast<uint16_t>(config.low * 0.95);
            config.standard = static_cast<uint16_t>(config.standard * 0.98);
        };
        
        adjustConfig(profile.charge_voltage);
        adjustConfig(profile.cov_threshold);
        adjustConfig(profile.cov_recovery);
    }
    
    static void adjustVoltagesForDesert(battery_profile_t& profile) {
        // 沙漠环境的电压调整 - 高温更保守
        auto adjustConfig = [](extended_temp_config_t& config) {
            config.high = static_cast<uint16_t>(config.high * 0.95);
            config.extreme_high = static_cast<uint16_t>(config.extreme_high * 0.90);
        };
        
        adjustConfig(profile.charge_voltage);
        adjustConfig(profile.cov_threshold);
        adjustConfig(profile.cov_recovery);
    }
};

#endif // ADVANCED_BATTERY_CONFIG_H
