#include "s300e.h"
#include "data_structure.h"

extern weather_station_global_structure_t weather_data;

/* ================= STORAGE ================= */
static s300e_frame_t frame;

/* ================= INIT ================= */
void S300E_Init(void)
{
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
}

/* ================= READ ================= */
bool S300E_ReadCO2(uint16_t *ppm)
{
    if (ppm == NULL)
    {
        return false;
    }

    /* Send read command */
    Wire.beginTransmission(S300E_ADDR);
    Wire.write(CMD_READ);
    if (Wire.endTransmission(true) != 0)
    {
        return false;
    }

    delay(2);

    /* Read frame */
    if (Wire.requestFrom(S300E_ADDR, FRAME_LEN) != FRAME_LEN)
    {
        while (Wire.available()) Wire.read();
        return false;
    }

    uint8_t *raw = (uint8_t *)&frame;
    for (uint8_t i = 0; i < FRAME_LEN; i++)
    {
        raw[i] = Wire.read();
    }

    /* Sanity check */
    if (frame.config != 0x08)
    {
        return false;
    }

    *ppm = BASE_CO2_PPM + frame.co2_delta;
    return true;
}


void S300E_data(void)
{
    uint16_t co2_ppm = 0;

    if (S300E_ReadCO2(&co2_ppm))
    {
        weather_data.CO2_ppm = co2_ppm;

        Serial.print("CO2 = ");
        Serial.print(weather_data.CO2_ppm);
        Serial.println(" ppm");
    }
    else
    {
        Serial.println("CO2 read error");
    }

    delay(SAMPLE_TIME_MS);
}