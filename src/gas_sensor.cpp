#include "gas_sensor.h"

extern weather_station_global_structure_t weather_data;

// ---------------- SERIAL ----------------
static HardwareSerial Gas_Sensor_Serial(1);

// ---------------- REQUEST FRAME ----------------
static uint8_t cmdRequest[9] = { 0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78};     // Request frame (same for all sensors)

// ---------------- MUX MAP ----------------
// MUX IC function logic for Selection (S0, S1) according to datasheet
static const bool S0_SELECT[6] = {0,1,0,1,0,1};
static const bool S1_SELECT[6] = {0,0,1,1,0,0};

// ---------------- CHECKSUM ----------------
// Checksum Calculation 
static uint8_t computeChecksum(uint8_t *buffer)
{
    uint16_t sum = 0;
    for (int i = 0; i <= 11; i++) sum += buffer[i];
    return (0xFF - (sum & 0xFF));
}

// ---------------- SENSOR SELECT ----------------
// Function to select one of the 6 sensors using dual 74HC4052 (Select Sensor (0–5))
static void selectSensor(uint8_t sensor)
{
    digitalWrite(EN1, HIGH);
    digitalWrite(EN2, HIGH);
    delay(600);

    if (sensor < 4) 
    {
        digitalWrite(EN1, LOW);
    }
    else{            
        digitalWrite(EN2, LOW);
    }
    digitalWrite(S0, S0_SELECT[sensor]);
    digitalWrite(S1, S1_SELECT[sensor]);

    delay(1000);
}

// ---------------- READ FRAME ----------------
// Function to Read & Parse 13-byte response frame from the active sensor
static bool readGasFrame(uint8_t sensorID)
{
    uint8_t buffer[13] = {0};
    uint8_t index = 0;
    uint32_t start = millis();

    Gas_Sensor_Serial.write(cmdRequest, sizeof(cmdRequest));

    while (millis() - start < RESPONSE_TIMEOUT)
    {
        if (Gas_Sensor_Serial.available())
        {
            uint8_t gases_data = Gas_Sensor_Serial.read();
            if (index == 0 && gases_data != 0xFF) {
                continue;
            }
            else {
            buffer[index++] = gases_data;
            }
            if (index >= 13) {
                break;
            }
        }
    }

    Gas_Sensor_Serial.flush();

    /*
    // Print raw data
    Serial.print("R_Data (Sensor id)= ");
    Serial.printf("%d",sensorID);
    Serial.print(" = ");

    for(uint8_t i=0; i < 13; i++)
    {
        Serial.printf("%02X,",buffer[i]);
    }
    Serial.println();
    */

    // Validate frame header
    if (index < 13 || buffer[0] != 0xFF || buffer[1] != 0x87)
    {
        return false;
    }
    // Validate checksum 
    if (computeChecksum(buffer) != buffer[12]) 
    {
        return false;
    }

    // Extract base values
    float consentration2 = (buffer[2] << 8) | buffer[3];
    float consentration1 = (buffer[6] << 8) | buffer[7];
    float temp  = ((buffer[8] << 8) | buffer[9])  / 100.0f;
    float hum  = ((buffer[10] << 8) | buffer[11]) / 100.0f;

    // Parse by sensor type
    switch (sensorID)
    {
        // CO sensor
        case 0:
            weather_data.CO_mg     = consentration2 / 10.0f;   
            weather_data.CO_ppm    = consentration1 / 10.0f;
            weather_data.CO_T      = temp;            
            weather_data.CO_H      = hum;
        break;

        // O3 sensor
        case 1:
            weather_data.O3_mg     = consentration2 / 1000.0f; 
            weather_data.O3_ppm    = consentration1 / 1000.0f;
            weather_data.O3_T      = temp;            
            weather_data.O3_H      = hum;
        break;

        // NO2 sensor
        case 2:
            weather_data.NO2_mg    = consentration2 / 1000.0f; 
            weather_data.NO2_ppm   = consentration1 / 1000.0f;
            weather_data.NO2_T     = temp;            
            weather_data.NO2_H     = hum;
        break;

        // H2S sensor
        case 3:
            weather_data.H2S_mg    = consentration2 / 1000.0f; 
            weather_data.H2S_ppm   = consentration1 / 1000.0f;
            weather_data.H2S_T     = temp;            
            weather_data.H2S_H     = hum;
        break;

        // SO2 sensor
        case 4:
            weather_data.SO2_mg    = consentration2 / 1000.0f; 
            weather_data.SO2_ppm   = consentration1 / 1000.0f;
            weather_data.SO2_T     = temp;            
            weather_data.SO2_H     = hum;
        break;

        // NH3 sensor
        case 5:
            weather_data.NH3_mg    = consentration2 / 100.0f;  
            weather_data.NH3_ppm   = consentration1 / 100.0f;
            weather_data.NH3_T     = temp;            
            weather_data.NH3_H     = hum;
        break;

        default:
        break;
    }
    return true;
}

// ---------------- INIT ----------------
void gas_sensor_init(void)
{
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(EN1, OUTPUT);
    pinMode(EN2, OUTPUT);

    digitalWrite(EN1, HIGH);
    digitalWrite(EN2, HIGH);

    Gas_Sensor_Serial.begin(9600, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);
    
    Serial.println("\nGas Multiplexer System Ready\n");
}

// ---------------- TASK (loop logic) ----------------
void gas_sensor_task(void)
{
    for (uint8_t i = 0; i < NUM_SENSORS; i++)
    {
        selectSensor(i);
        delay(POLL_DELAY);

        if (!readGasFrame(i))
        {
            Serial.printf("Sensor %d → BAD FRAME\n", i);
        }
    }
}

// ---------------- PRINT ----------------
void gas_sensor_print(void)
{
    Serial.println("================================");
    Serial.printf("CO   : %.2f mg/m3   %.2f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.CO_mg,  weather_data.CO_ppm,  weather_data.CO_T,  weather_data.CO_H);
    Serial.printf("O3   : %.3f mg/m3   %.3f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.O3_mg,  weather_data.O3_ppm,  weather_data.O3_T,  weather_data.O3_H);
    Serial.printf("NO2  : %.3f mg/m3   %.3f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.NO2_mg, weather_data.NO2_ppm, weather_data.NO2_T, weather_data.NO2_H);
    Serial.printf("H2S  : %.3f mg/m3   %.3f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.H2S_mg, weather_data.H2S_ppm, weather_data.H2S_T, weather_data.H2S_H);
    Serial.printf("SO2  : %.3f mg/m3   %.3f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.SO2_mg, weather_data.SO2_ppm, weather_data.SO2_T, weather_data.SO2_H);
    Serial.printf("NH3  : %.2f mg/m3   %.2f ppm   T=%.2f°C  H=%.2f%%\n",  weather_data.NH3_mg, weather_data.NH3_ppm, weather_data.NH3_T, weather_data.NH3_H);
    Serial.println("================================\n");
}
