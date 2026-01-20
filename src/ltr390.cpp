#include "ltr390.h"
#include "data_structure.h"

extern weather_station_global_structure_t weather_data;

//-------------------------------------------------------------------------------------------------
/* Write 8-bit register */
static ltr390_i2c_status_t writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission((uint8_t)LTR390_ADDR);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
    if (err != 0) 
    {
      Serial.print("I2C write error, reg 0x");
      Serial.print(reg, HEX);
      Serial.print(" code: ");
      Serial.println(err);
      return LTR390_I2C_ERR_WRITE;
    }
  return LTR390_I2C_OK;
}


//-------------------------------------------------------------------------------------------------
/* Read 8-bit register */
static ltr390_i2c_status_t readRegister8(uint8_t reg, uint8_t *value) 
{
  Wire.beginTransmission((uint8_t)LTR390_ADDR);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) 
  {
    return LTR390_I2C_ERR_BEGIN;
  }
  uint8_t bytes = Wire.requestFrom((uint8_t)LTR390_ADDR, (uint8_t)1);
  if (bytes != 1) 
  {
    return LTR390_I2C_ERR_READ_LEN;
  }
  *value = Wire.read();
  return LTR390_I2C_OK;
}

//-------------------------------------------------------------------------------------------------
/* Read 24-bit register */
static ltr390_i2c_status_t readRegister24(uint8_t reg, uint32_t *value)
{
    Wire.beginTransmission((uint8_t)LTR390_ADDR);
    Wire.write(reg);

    uint8_t err = Wire.endTransmission(false);
    if (err != 0) 
    {
      return LTR390_I2C_ERR_BEGIN;
    }

    uint8_t bytes = Wire.requestFrom((uint8_t)LTR390_ADDR, (uint8_t)3);
    if (bytes != 3) 
    {
      return LTR390_I2C_ERR_READ_LEN;
    }

    uint32_t v = 0;
    v  = (uint32_t)Wire.read();
    v |= (uint32_t)Wire.read() << 8;
    v |= (uint32_t)Wire.read() << 16;

    *value = v;
  return LTR390_I2C_OK;
}

//-------------------------------------------------------------------------------------------------
void init_LTR390 (void) 
{
  uint8_t partID;
    if (readRegister8(LTR390_PART_ID, &partID) != LTR390_I2C_OK) 
    {
      Serial.println("ERROR: LTR390 not responding");
    }

    Serial.print("LTR390 Part ID: 0x");
    Serial.println(partID, HEX);

    if (partID != 0xB2) 
    {
      Serial.println("ERROR: Invalid LTR390 ID");
    }


  // Enable in UV mode
  writeRegister(LTR390_MAIN_CTRL, LTR390_MODE_UVS);

  // Set resolution = 20-bit, measurement rate = 400ms
  writeRegister(LTR390_MEAS_RATE, 0x00);

  // Set gain = 18
  writeRegister(LTR390_GAIN, 0x04);

}

//-------------------------------------------------------------------------------------------------
void read_LTR390 (void)
{
    // Check if new data is ready
  uint8_t status;
  if (readRegister8(LTR390_STATUS, &status) != LTR390_I2C_OK) 
  {
    Serial.println("Failed to read STATUS");
    return;
  }

  if (status & 0x08) 
  {
    uint32_t uv_raw;
    if (readRegister24(LTR390_UVS_DATA_0, &uv_raw) == LTR390_I2C_OK) 
    {
      weather_data.uv_raw = uv_raw;

      // Convert to UV Index
      weather_data.uvi = weather_data.uv_raw / UV_SENSITIVITY;

      // Convert UVI to irradiance (µW/cm²)
      weather_data.uv_irradiance = weather_data.uvi * UVI_TO_IRRADIANCE;

      Serial.print(F("UV Raw: "));
      Serial.print(weather_data.uv_raw);
      Serial.print(F(" | UVI: "));
      Serial.print(weather_data.uvi, 2);
      Serial.print(F(" | Irradiance: "));
      Serial.print(weather_data.uv_irradiance, 2);
      Serial.println(F(" µW/cm²"));
    }
    else {
      Serial.println("Failed to read UV data");
    }
  }
}