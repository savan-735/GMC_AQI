#include "debug_serial.h"
#include "sds011.h"//PM SENSOR(Sensiron)
#include "data_structure.h"

extern weather_station_global_structure_t weather_data;

// UART object
static HardwareSerial PM_Serial(0);

uint32_t readFails = 0;
uint8_t failStreak = 0;   // consecutive fails

//---------------------------------------------
// SDS011 COMMANDS
//---------------------------------------------
void sendCommand(uint8_t *cmd, uint8_t len)
{
    PM_Serial.write(cmd, len);
}

// Wakeup
bool startMeasurement()
{
    uint8_t cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x01,
                     0,0,0,0,0,0,0,0,0,
                     0xFF, 0xFF, 0x06, 0xAB};

   // sendCommand(cmd, sizeof(cmd));
   size_t sent = PM_Serial.write(cmd, sizeof(cmd));

   return (sent == sizeof(cmd));
}

// Sleep
bool stopMeasurement()
{
    uint8_t cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x00,
                     0,0,0,0,0,0,0,0,0,
                     0xFF, 0xFF, 0x05, 0xAB};

    size_t sent = PM_Serial.write(cmd, sizeof(cmd));
    return (sent == sizeof(cmd));
}

//---------------------------------------------
// READ DATA
//---------------------------------------------
bool readMeasurement()
{
   static uint8_t buffer[10];

    // 🔥 Flush if buffer overflow / backlog
    if (PM_Serial.available() > 200)
    {
        // only drop few bytes, full flush nahi
        for(int i=0;i<20;i++) PM_Serial.read();
    }

    while (PM_Serial.available() >= 10)
    {
        // Sync to header
        if (PM_Serial.peek() != 0xAA)
        {
            PM_Serial.read(); // discard garbage
            continue;
        }

        // Read full packet
        PM_Serial.readBytes(buffer, 10);

        // Validate packet
        if (buffer[1] == 0xC0 && buffer[9] == 0xAB)
        {
            uint8_t checksum = 0;
            for (int i = 2; i <= 7; i++)
                checksum += buffer[i];

            if (checksum == buffer[8])
            {
                uint16_t pm25_raw = buffer[2] | (buffer[3] << 8);
                uint16_t pm10_raw = buffer[4] | (buffer[5] << 8);

                weather_data.pm1   = 0;
                weather_data.pm2_5 = pm25_raw / 10.0;
                weather_data.pm4   = 0;
                weather_data.pm10  = pm10_raw / 10.0;

                // Not supported
                weather_data.nc0_5       = 0;
                weather_data.nc1_0       = 0;
                weather_data.nc2_5       = 0;
                weather_data.nc4_0       = 0;
                weather_data.nc10        = 0;
                weather_data.typicalSize = 0;

                failStreak = 0;
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------
// INIT (WITH 30 SEC WARMUP 🔥)
//---------------------------------------------
void sds011_init()
{
    USBSerial.println(F("Starting SDS011..."));

    PM_Serial.begin(SDS011_BAUD, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);

    startMeasurement(); // wakeup

    USBSerial.println(F("Warm-up 30 sec..."));
    delay(FIRST_WARMUP_TIME);  // 🔥 30 sec warmup
}

//---------------------------------------------
// DATA FUNCTION (WITH SLEEP LOGIC 🔥)
//---------------------------------------------
void sds011_data()
{
    static uint32_t lastPrint = 0;


    if (readMeasurement())
    {
        // Print every 2 sec (avoid flooding)
        if (millis()- lastPrint>2000)
        {
            USBSerial.print(F("PM2.5: "));
            USBSerial.print(weather_data.pm2_5, 2);
            USBSerial.println(F(" µg/m³"));

            USBSerial.print(F("PM10 : "));
            USBSerial.print(weather_data.pm10, 2);
            USBSerial.println(F(" µg/m³"));

            USBSerial.print(F("Read fails: "));
            USBSerial.println(readFails);

            USBSerial.println(F("---------------------------------"));

            lastPrint = millis();
            // // 🔥 After reading → Sleep to save life
            // Serial.println(F("Sleeping sensor..."));
            // stopMeasurement();

            // delay(30000); // sleep 30 sec

            // Serial.println(F("Waking sensor..."));
            // startMeasurement();

            // Serial.println(F("Re-warmup 30 sec..."));
            // delay(30000);
        }
    }
    else
    {
        // Count fails only occasionally
        static uint32_t lastFailCheck = 0;

        if (millis() - lastFailCheck > 2000)
        {
            readFails++;
            failStreak++;
            lastFailCheck = millis();
        }
    }

    // Auto recovery
    if (failStreak > 5)
    {
        USBSerial.println(F("Too many fails → restarting sensor..."));

        stopMeasurement();
        delay(200);
        startMeasurement();
        delay(30000);
        failStreak = 0;
    }
}