#ifndef __MESH_SOLAR_APP_H__
#define __MESH_SOLAR_APP_H__
#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <ArduinoJson.h>
#include "driver/meshsolar.h"
#include "driver/SoftwareWire.h"
#include "driver/bq4050.h"
#include "utils/logger.h"
#include <Adafruit_NeoPixel.h>

void meshSolarStart(void);
int meshSolarCmdHandle(const char *cmd);

int meshSolarGetBatteryPercent();
uint16_t meshSolarGetBattVoltage();
bool meshSolarIsBatteryConnect();
bool meshSolarIsVbusIn();
bool meshSolarIsCharging();

#endif // __MESH_SOLAR_APP_H__