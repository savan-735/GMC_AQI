#ifndef __SDS011_H
#define __SDS011_H

#include <Arduino.h>

// UART config
#define SDS011_BAUD        9600

// UART pins (change if needed)
#define PM_RX_PIN 44
#define PM_TX_PIN 43

// Warmup timings
#define FIRST_WARMUP_TIME  30000   // 30 sec
#define NORMAL_WARMUP_TIME 5000    // 5 sec after wake

bool startMeasurement();
bool stopMeasurement();
bool readMeasurement();
void sds011_init();
void sds011_data();

#endif

// #ifndef __SPS30_H
// #define __SPS30_H


// #include <Arduino.h>
// #include <Wire.h>

// // SPS30 I2C address
// #define SPS30_ADDR          0x69


// #define SDA_PIN             8
// #define SCL_PIN             9


// bool startMeasurement();
// void stopMeasurement();
// bool readMeasurement();
// void sps_30_init();
// void sps_30_data();


// #endif
