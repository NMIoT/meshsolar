# MeshSolar Battery Management System

An intelligent battery management system based on the BQ4050 chip, supporting multiple lithium battery chemistries with comprehensive battery monitoring, protection, and configuration capabilities.

## 🚀 Features

### Core Functionality
- **Multi-Chemistry Support**: LiFePO4, Li-ion, LiPo batteries
- **Real-time Monitoring**: Voltage, current, temperature, SOC parameters
- **Intelligent Protection**: Over/under voltage, over/under temperature protection
- **JSON Interface**: Standardized command and status output
- **Platform Independent**: Supports porting to multiple MCU platforms

### Battery Management Features
- **Accurate SOC Calculation**: CEDV algorithm-based capacity estimation
- **Multi-level Protection**: Hardware + software dual protection mechanisms
- **Temperature Compensation**: Adjusts charge/discharge parameters based on temperature
- **Learning Algorithms**: Adaptive to battery aging and capacity changes
- **FET Control**: Software-controlled charge/discharge switching

### Supported Configurations
- **Battery Series**: 1S-4S (1-4 cells in series)
- **Capacity Range**: 100mAh - 65535mAh
- **Temperature Range**: -40°C to +85°C
- **Voltage Accuracy**: ±1mV
- **Current Accuracy**: ±1mA

## 📋 Hardware Requirements

### Minimum Configuration
- **MCU**: Microcontroller supporting Arduino framework
- **RAM**: >8KB (16KB+ recommended)
- **Flash**: >64KB
- **I2C**: Hardware or software I2C interface
- **UART**: At least 1 UART (2 recommended)

### Recommended Platforms
| Platform | MCU | RAM | Flash | I2C | Status | 
|----------|-----|-----|-------|-----|--------|
| nRF52840 | ARM Cortex-M4 | 256KB | 1MB | Hardware | ✅ Tested |
| ESP32 | Xtensa LX6 | 320KB | 4MB | Hardware | ✅ Compatible |
| STM32F4 | ARM Cortex-M4 | 192KB | 1MB | Hardware | ✅ Compatible |
| Arduino Mega | ATmega2560 | 8KB | 256KB | Software | ⚠️ Memory Limited |

### External Components
- **BQ4050**: TI battery management IC
- **Pull-up Resistors**: 4.7kΩ (SDA/SCL)
- **Power Supply**: 3.3V stable supply
- **Crystal**: BQ4050 external 32.768kHz crystal

## 🔧 Software Dependencies

### Arduino Libraries
```cpp
#include <Arduino.h>        // Arduino core library
#include <ArduinoJson.h>    // JSON parsing library (v6.x)
#include "SoftwareWire.h"   // Software I2C library (optional)
```

### Custom Modules
- `bq4050.h/cpp` - BQ4050 driver
- `meshsolar.h/cpp` - Battery management core logic
- `logger.h/cpp` - Debug logging system

## 📦 Installation Guide

### 1. Download Project
```bash
git clone https://github.com/your-repo/meshsolar.git
cd meshsolar
```

### 2. Install Dependencies
Install using Arduino Library Manager:
- Copy SoftwareWire.cpp and SoftwareWire.h to your workspace.

### 3. Hardware Connection
```
BQ4050    →    MCU
------------------------
SDA       →    GPIO_SDA
SCL       →    GPIO_SCL
VCC       →    3.3V
GND       →    GND
```

### 4. Configure Platform Parameters
Edit configuration in `src/main.cpp`:

```cpp
// Serial port configuration - modify for target platform
#define comSerial           Serial      // Primary communication port
// ESP32: Serial, Serial1, Serial2
// Arduino: Serial, SoftwareSerial
// STM32: Serial, Serial1, etc.

// I2C pin configuration - modify for your hardware
#define SDA_PIN             33          // I2C data line
#define SCL_PIN             32          // I2C clock line
// ESP32: GPIO 21 (SDA), GPIO 22 (SCL)
// Arduino Uno: A4 (SDA), A5 (SCL)
// nRF52840: Any GPIO pin

// Global object declarations - initialization order is critical!
// For nRF52840: Uses pin mapping
SoftwareWire Wire(g_ADigitalPinMap[SDA_PIN], g_ADigitalPinMap[SCL_PIN]);
// For other platforms: Use direct pin numbers
// SoftwareWire Wire(SDA_PIN, SCL_PIN);
```

## 🚀 Quick Start

### Basic Usage
```cpp
#include "meshsolar.h"
#include "bq4050.h"

// Initialize objects
BQ4050 bq4050;
MeshSolar meshsolar;

void setup() {
    Serial.begin(115200);
    
    // Initialize BQ4050
    bq4050.begin(&Wire, BQ4050ADDR);
    
    // Initialize MeshSolar
    meshsolar.begin(&bq4050);
}

void loop() {
    // Read battery status
    meshsolar.get_realtime_bat_status();
    
    // Output JSON status
    String json;
    meshsolar_status_to_json(&meshsolar.sta, json);
    Serial.println(json);
    
    delay(1000);
}
```

### JSON Command Interface

#### 1. Basic Configuration Command
```json
{
    "command": "config",
    "battery": {
        "type": "lifepo4",
        "cell_number": 4,
        "design_capacity": 3200,
        "cutoff_voltage": 2800
    },
    "temperature_protection": {
        "charge_high_temp_c": 60,
        "charge_low_temp_c": -10,
        "discharge_high_temp_c": 60,
        "discharge_low_temp_c": -10,
        "temp_enabled": true
    }
}
```

#### 2. Advanced Configuration Command
```json
{
    "command": "advance",
    "battery": {
        "cuv": 2500,
        "eoc": 3600,
        "eoc_protect": 3750
    },
    "cedv": {
        "cedv0": 2800,
        "cedv1": 2820,
        "cedv2": 2850,
        "discharge_cedv0": 2800,
        "discharge_cedv10": 2900,
        // ... more CEDV points
        "discharge_cedv100": 3600
    }
}
```

#### 3. Control Commands
```json
// FET switch control
{"command": "switch", "fet_en": true}

// Battery gauge reset
{"command": "reset"}

// Configuration sync
{"command": "sync", "times": 3}

// Status query
{"command": "status"}
```

### Status Output Example
```json
{
    "command": "status",
    "soc_gauge": 85,
    "charge_current": -1200,
    "total_voltage": "12.345",
    "learned_capacity": "3.200",
    "pack_voltage": "12345",
    "fet_enable": true,
    "protection_sta": "Normal",
    "emergency_shutdown": false,
    "cells": [
        {"cell_num": 1, "temperature": 25.123, "voltage": 3.234},
        {"cell_num": 2, "temperature": 25.156, "voltage": 3.245},
        {"cell_num": 3, "temperature": 25.089, "voltage": 3.238},
        {"cell_num": 4, "temperature": 25.201, "voltage": 3.251}
    ]
}
```

## 🔧 Platform Porting Guide

### Porting Checklist

#### Hardware Requirements
- [ ] MCU RAM > 8KB
- [ ] I2C interface capability
- [ ] At least 2 serial ports (command + debug)
- [ ] 3.3V stable power supply
- [ ] I2C pull-up resistors (4.7kΩ)

#### Software Requirements
- [ ] Arduino framework or compatible environment
- [ ] ArduinoJson library (v6.x)
- [ ] Platform-specific I2C library
- [ ] Serial/USB communication support

#### Configuration Steps
1. **Serial Assignment**: Confirm serial port numbers match hardware
2. **I2C Pins**: Correctly define SDA/SCL pins
3. **Baud Rate**: Confirm platform supports 115200 baud rate
4. **BQ4050 Address**: Verify I2C address (0x0B) is accessible
5. **Pin Mapping**: Confirm pin mapping functions work correctly

#### Testing Steps
1. Verify serial communication (send/receive test strings)
2. Test I2C communication (read BQ4050 firmware version)
3. Send basic JSON commands and verify responses
4. Monitor status updates for reasonable values
5. Test all command types (config, advance, switch, reset, sync)

### Common Issues

#### I2C Timeout
- Check wiring, pull-up resistors, power supply
- Verify I2C clock frequency settings
- Confirm BQ4050 is working properly

#### JSON Parse Error
- Verify command format and buffer sizes
- Check ArduinoJson version compatibility
- Confirm sufficient memory allocation

#### No Serial Response
- Check port assignment and baud rate
- Verify USB drivers
- Confirm serial port is not occupied by other programs

#### Invalid Battery Data
- Verify BQ4050 configuration and connections
- Check battery type settings
- Confirm protection circuits are normal

## 🔍 Technical Details

### Battery Protection Mechanism

#### Multi-level Protection Architecture
1. **Hardware Protection**: BQ4050 built-in protection circuits
2. **Software Protection**: Application layer protection algorithms
3. **Communication Protection**: I2C communication error handling

#### Protection Types
- **Voltage Protection**: COV (Cell Over Voltage), CUV (Cell Under Voltage)
- **Current Protection**: OCC (Overcurrent Charge), OCD (Overcurrent Discharge)
- **Temperature Protection**: OTC/UTC (charging), OTD/UTD (discharging)
- **Time Protection**: Precharge timeout, charge timeout

### CEDV Algorithm
CEDV (Constant Energy Discharge Voltage) is an advanced SOC calculation algorithm:
- Estimates remaining capacity based on discharge voltage curve
- Supports temperature and aging compensation
- More accurate than traditional coulomb counting
- Requires accurate battery characteristic curves

### Temperature Compensation Strategy
- **Low Temperature Zone** (-20°C~0°C): Conservative voltage settings
- **Standard Zone** (0°C~45°C): Nominal voltage parameters
- **High Temperature Zone** (45°C~60°C): Derated operation for protection
- **Recovery Zone**: Prevents temperature boundary oscillation

## 📊 Performance Specifications

### Measurement Accuracy
- **Voltage Accuracy**: ±1mV (16-bit ADC)
- **Current Accuracy**: ±1mA (16-bit ADC)
- **Temperature Accuracy**: ±1°C (built-in sensor)
- **SOC Accuracy**: ±1% (CEDV algorithm)

### Response Time
- **Command Response**: <100ms
- **Status Update**: 1 second cycle
- **Protection Response**: <10ms (hardware)
- **I2C Operation**: 10-100ms

### Power Consumption
- **Normal Operation**: <5mA
- **Sleep Mode**: <100μA
- **Protection State**: <1mA

## 🛠️ Development Tools

### Recommended IDEs
- **PlatformIO**: Cross-platform development environment
- **Arduino IDE**: Simple and fast development
- **VSCode**: Professional code editing

### Debug Tools
- **Serial Monitor**: View logs and JSON output
- **Oscilloscope**: Analyze I2C communication
- **Multimeter**: Verify voltage and current
- **BQ4050 EV2400**: TI official evaluation tool

### Development References
- [BQ4050 Technical Manual](doc/bq4050.pdf)
- [BQ4050 Configuration Guide](doc/BQ4050配置手册.pdf)
- [SMBus Protocol Specification](doc/smbus.pdf)

## 📈 Version History

### v0.6 (Current Version)
- ✅ Complete JSON command interface
- ✅ Multi-chemistry support
- ✅ Temperature compensation algorithm
- ✅ CEDV configuration functionality
- ✅ Platform porting guide

### Planned Features
- [ ] Battery aging monitoring
- [ ] Wireless communication interface
- [ ] Graphical configuration tool
- [ ] Historical data logging
- [ ] Fault diagnosis system

## 🤝 Contributing

### Report Issues
Please report on GitHub Issues:
- Detailed error description
- Steps to reproduce
- Hardware configuration information
- Related log output

### Code Contribution
1. Fork the project repository
2. Create a feature branch
3. Submit code changes
4. Create Pull Request
5. Code review and testing

### Porting Contribution
Welcome to submit new platform ports:
- Add platform-specific configurations
- Update README documentation
- Provide test reports

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## 📞 Support & Contact

- **Technical Documentation**: [Wiki Pages](https://github.com/your-repo/meshsolar/wiki)
- **Issue Reporting**: [GitHub Issues](https://github.com/your-repo/meshsolar/issues)
- **Discussion**: [GitHub Discussions](https://github.com/your-repo/meshsolar/discussions)

## 🙏 Acknowledgments

- Texas Instruments - BQ4050 chip and technical documentation
- Arduino Community - Development framework and library support
- ArduinoJson - Excellent JSON processing library
- All contributors and testers

---

**⚠️ Safety Warning**: Lithium batteries are potentially hazardous. Please ensure development and testing are conducted under professional guidance with appropriate safety measures.
