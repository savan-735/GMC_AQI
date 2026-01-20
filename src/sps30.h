#ifndef __SPS30_H
#define __SPS30_H


#include <Arduino.h>
#include <Wire.h>

// SPS30 I2C address
#define SPS30_ADDR          0x69


#define SDA_PIN             8
#define SCL_PIN             9


bool startMeasurement();
void stopMeasurement();
bool readMeasurement();
void sps_30_init();
void sps_30_data();


#endif