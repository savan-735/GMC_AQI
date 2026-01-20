#include "sps30.h"
#include "data_structure.h"

extern weather_station_global_structure_t weather_data;

uint32_t crcFails = 0;
uint32_t readFails = 0;
uint8_t failStreak = 0;   // consecutive fails

//-------------------------------------------------------------------------------------------------
// CRC 
uint8_t calcCrc(uint8_t data[2]) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < 2; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc <<= 1;
    }
  }
  return crc;
}

//-------------------------------------------------------------------------------------------------
// Write Command
bool writeCommand(uint16_t cmd) {
  Wire.beginTransmission(SPS30_ADDR);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  delay(5);
  return true;
}

//-------------------------------------------------------------------------------------------------
// Read Command 
bool readCommand(uint16_t cmd, uint8_t* data, uint16_t len) {
  if (!writeCommand(cmd)) return false;

  Wire.requestFrom(SPS30_ADDR, (int)len);
  if (Wire.available() != len) return false;

  for (uint16_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return true;
}

//-------------------------------------------------------------------------------------------------
// Start Measurement 
bool startMeasurement() {
  uint8_t cmd[5] = {0x00, 0x10, 0x03, 0x00, 0xAC};  // Start measurement
  Wire.beginTransmission(SPS30_ADDR);
  for (uint8_t i = 0; i < 5; i++) Wire.write(cmd[i]);
  return (Wire.endTransmission() == 0);
}

//-------------------------------------------------------------------------------------------------
// Stop Measurement 
void stopMeasurement() {
  writeCommand(0x0104);
  delay(100);
}

//-------------------------------------------------------------------------------------------------
// Read Measurement 
bool readMeasurement() {
  uint8_t raw[60];
  if (!readCommand(0x0300, raw, 60)) {
    readFails++;
    failStreak++;
    return false;
  }

  // Parse floats
  for (int i = 0; i < 20; i++) {
    uint8_t d[2] = { raw[i * 3], raw[i * 3 + 1] };
    uint8_t crc = raw[i * 3 + 2];
    if (calcCrc(d) != crc) {
      crcFails++;
      failStreak++;
      return false;
    }
  }

  uint32_t vals[10];
  for (int i = 0; i < 10; i++) {
    vals[i] = ((uint32_t)raw[i*6] << 24) | ((uint32_t)raw[i*6+1] << 16) |
              ((uint32_t)raw[i*6+3] << 8) | raw[i*6+4];
  }

    weather_data.pm1         = *(float*)&vals[0];
    weather_data.pm2_5       = *(float*)&vals[1];
    weather_data.pm4         = *(float*)&vals[2];
    weather_data.pm10        = *(float*)&vals[3];
    weather_data.nc0_5       = *(float*)&vals[4];
    weather_data.nc1_0       = *(float*)&vals[5];
    weather_data.nc2_5       = *(float*)&vals[6];
    weather_data.nc4_0       = *(float*)&vals[7];
    weather_data.nc10        = *(float*)&vals[8];
    weather_data.typicalSize = *(float*)&vals[9];


  failStreak = 0;  // success → reset
  return true;
}

//-------------------------------------------------------------------------------------------------
void sps_30_init(){
    Wire.setClock(50000); // safer speed for ESP32

    delay(1000);
    Serial.println(F("Starting SPS30..."));

    if (!startMeasurement()) 
    {
    Serial.println(F("Failed to start measurement!"));
    return;
    }

    Serial.println(F("SPS30 started, waiting 10s warm-up..."));
    delay(10000);
}

//-------------------------------------------------------------------------------------------------
void sps_30_data()
{
  if (readMeasurement()) 
  {
    Serial.print(F("PM1.0: "));
    Serial.print(weather_data.pm1, 2);    // 2 decimal places
    Serial.println(F(" µg/m³"));

    Serial.print(F("PM2.5: "));
    Serial.print(weather_data.pm2_5, 2);
    Serial.println(F(" µg/m³"));

    Serial.print(F("PM4.0: "));
    Serial.print(weather_data.pm4, 2);
    Serial.println(F(" µg/m³"));

    Serial.print(F("PM10 : "));
    Serial.print(weather_data.pm10, 2);
    Serial.println(F(" µg/m³"));

    Serial.print(F("NC0.5: "));
    Serial.print(weather_data.nc0_5, 2);
    Serial.println(F(" #/cm³"));

    Serial.print(F("NC1.0: "));
    Serial.print(weather_data.nc1_0, 2);
    Serial.println(F(" #/cm³"));

    Serial.print(F("NC2.5: "));
    Serial.print(weather_data.nc2_5, 2);
    Serial.println(F(" #/cm³"));

    Serial.print(F("NC4.0: "));
    Serial.print(weather_data.nc4_0, 2);
    Serial.println(F(" #/cm³"));

    Serial.print(F("NC10 : "));
    Serial.print(weather_data.nc10, 2);
    Serial.println(F(" #/cm³"));

    Serial.print(F("Typical particle size: "));
    Serial.print(weather_data.typicalSize, 2);
    Serial.println(F(" µm"));

    // For unsigned long counters
    Serial.print(F("CRC fails: "));
    Serial.print(crcFails);
    Serial.print(F(" | Read fails: "));
    Serial.println(readFails);

    Serial.println(F("---------------------------------"));
  } else {
    Serial.println(F("Measurement read failed! Keeping last values."));
  }

  // Auto recovery if too many fails in a row
  if (failStreak > 5) 
  {
    Serial.println(F("Too many consecutive fails → restarting SPS30..."));
    stopMeasurement();
    delay(200);
    startMeasurement();
    delay(500);
    failStreak = 0;
  }
}