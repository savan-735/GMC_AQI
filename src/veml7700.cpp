#include "veml7700.h"
#include "data_structure.h"

extern weather_station_global_structure_t weather_data;

//-------------------------------------------------------------------------------------------------
/* Write 16-bit register */
static veml7700_i2c_status_t write16(uint8_t reg, uint16_t value)
{
    Wire.beginTransmission(VEML7700_ADDR);
    Wire.write(reg);
    Wire.write(value & 0xFF);          // LSB
    Wire.write((value >> 8) & 0xFF);   // MSB

    uint8_t err = Wire.endTransmission();
    if (err != 0) 
    {
        Serial.print("I2C write error, code: ");
        Serial.println(err);
        return VEML7700_I2C_ERR_WRITE;
    }

    return VEML7700_I2C_OK;
}

//-------------------------------------------------------------------------------------------------
/* Read 16-bit register */
static veml7700_i2c_status_t read16(uint8_t reg, uint16_t *value)
{
    Wire.beginTransmission(VEML7700_ADDR);
    Wire.write(reg);

    uint8_t err = Wire.endTransmission(false);  // repeated start
    if (err != 0) 
    {
        Serial.print("I2C address error, code: ");
        Serial.println(err);
        return VEML7700_I2C_ERR_BEGIN;
    }

    uint8_t bytesRead = Wire.requestFrom((uint8_t)VEML7700_ADDR, (uint8_t)2);
    if (bytesRead != 2) 
    {
        Serial.print("I2C read length error, bytes: ");
        Serial.println(bytesRead);
        return VEML7700_I2C_ERR_READ_LEN;
    }

    uint16_t lsb = Wire.read();
    uint16_t msb = Wire.read();
    *value = (msb << 8) | lsb;

    return VEML7700_I2C_OK;
}

//-------------------------------------------------------------------------------------------------
/* Initialize VEML7700 */
void veml7700_init(void)
{
    //Wire.begin(SDA_PIN, SCL_PIN);
    veml7700_i2c_status_t status = write16(ALS_CONF_REG, 0x0000); // Gain=1/4, IT=100ms
    if (status != VEML7700_I2C_OK) {
        Serial.println("VEML7700 config failed!");
    } else {
        Serial.println("VEML7700 initialized successfully");
    }
}

//-------------------------------------------------------------------------------------------------
void veml7700_task(void)
{
    uint16_t als_raw;
    veml7700_i2c_status_t status = read16(ALS_DATA_REG, &als_raw);

    if (status == VEML7700_I2C_OK) 
    {
        float lux = als_raw * 0.0576f;      // default conversion factor
        weather_data.lux = lux;             // store into global structure
        Serial.print("Ambient Light: ");
        Serial.print(weather_data.lux);
        Serial.println(" lx");
    } else {
        Serial.println("Failed to read ALS data");
    }
}
