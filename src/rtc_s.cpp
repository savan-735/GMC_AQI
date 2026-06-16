#include "debug_serial.h"
#include <Arduino.h>
#include <rtc_s.h>
#include <data_structure.h>
#include <Wire.h>

//-------------------------------------------------------------------------------------------------
void initRTC() {
    Wire.begin(SDA_PIN, SCL_PIN); // SDA, SCL
}

//-------------------------------------------------------------------------------------------------
uint8_t decToBcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

//-------------------------------------------------------------------------------------------------
uint8_t bcdToDec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

//-------------------------------------------------------------------------------------------------
void setRTCDateTime(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t date, uint8_t month, uint8_t year) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x00); // Start at register 0
    Wire.write(decToBcd(sec));
    Wire.write(decToBcd(min));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(day));
    Wire.write(decToBcd(date));
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
}

//-------------------------------------------------------------------------------------------------
void readRTCDateTimeToStruct(weather_station_global_structure_t *weather_data) {
    if (!weather_data) return;

    bool rtcValid = true;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(0x00);
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 7);
    if (Wire.available() < 7) {
        USBSerial.println(F("RTC read failed: insufficient data."));
        rtcValid = false;
    }
    uint8_t sec = bcdToDec(Wire.read());
    uint8_t min = bcdToDec(Wire.read());
    uint8_t hour = bcdToDec(Wire.read());
    uint8_t day = bcdToDec(Wire.read());
    uint8_t date = bcdToDec(Wire.read());
    uint8_t month = bcdToDec(Wire.read());
    uint8_t year = bcdToDec(Wire.read());

    if (rtcValid) {
    weather_data->rtc_sec = sec;
    weather_data->rtc_min = min;
    weather_data->rtc_hour = hour;
    weather_data->rtc_day = day;
    weather_data->rtc_date = date;
    weather_data->rtc_month = month;
    weather_data->rtc_year = year;

    // Print the date and time to Serial Monitor
    USBSerial.print(F("Date: "));
    USBSerial.print(date);
    USBSerial.print(F("/"));
    USBSerial.print(month);
    USBSerial.print(F("/20"));
    USBSerial.print(year);
    USBSerial.print(F("  Time: "));
    USBSerial.print(hour);
    USBSerial.print(F(":"));
    USBSerial.print(min);
    USBSerial.print(F(":"));
    USBSerial.println(sec);
    }

    else {
        // Fallback to 00/00/00 00:00:00
        weather_data->rtc_sec = 0;
        weather_data->rtc_min = 0;
        weather_data->rtc_hour = 0;
        weather_data->rtc_day = 0;
        weather_data->rtc_date = 0;
        weather_data->rtc_month = 0;
        weather_data->rtc_year = 0;
    }
}

