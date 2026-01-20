#ifndef __GAS_SENSOR_H
#define __GAS_SENSOR_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "data_structure.h"

// ---------------- UART ----------------
#define GAS_RX_PIN          15
#define GAS_TX_PIN          16

// ---------------- MUX PINS ----------------
#define S0                  35
#define S1                  36
#define EN1                 37
#define EN2                 38

// ---------------- CONFIG ----------------
#define RESPONSE_TIMEOUT    3000
#define POLL_DELAY          500
#define NUM_SENSORS         6

// ---------------- FUNCTION ----------------
void gas_sensor_init(void);
void gas_sensor_task();
void gas_sensor_print();

#endif
