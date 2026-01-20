#include <Arduino.h>

#include <timer.h>                                                // Timer operation
#include "esp_task_wdt.h"                                         // WatchDOG
#include <esp_intr_alloc.h>
#include <driver/uart.h>
#include "soc/rtc.h"                                              // FOR CLOCK OPERATION APB/AHB

#include <Preferences.h>                                          // For savaing data to NVS
#include <Ethernet_Generic.h>                                     // For Ethernet Module communication 
#include <WebServer.h>                                            // For Hosting Web Page 
#include <WiFi.h>                                                 // For WiFi communication of ESP32
#include <HardwareSerial.h>                                       // For Serial communication operation
#include <rtc_s.h>                                                // RTC Library of RTC sensor
#include <Wire.h>                                                 // For I2C communication
#include <ota.h>                                                  // For OTA Support
#include <gas_sensor.h>                                           // For UART Multiplexer gases sensor
#include <veml7700.h>                                             // For Ambient light sensor
#include <ltr390.h>                                               // For UV Radiation sensor 
#include <sps30.h>                                                // For PM sensor 
#include <s300e.h>                                                // For CO2 sensor
#include <bme680.h>                                               // For BME680 sensor
#include <gsm.h>

//-----------------------------------------------------------------------------------------------------------------
// W5500 Ethernet Module SPI configuration
#define W5500_CS                        5
#define W5500_RST                       4

//-----------------------------------------------------------------------------------------------------------------
// WiFi credentials - Router/AP ID & Password for hosting Web Page 
#define WIFI_SSID                       "Weather_Station_IOT"       // WiFi SSID 
#define WIFI_PASSWORD                   "Weather_Station_IOT"       // WiFi Password 

//-----------------------------------------------------------------------------------------------------------------
// Default Server IP and Server Port for TCP communication
IPAddress serverIP                      (192, 168, 1, 100);         // Default server IP
uint16_t serverPort                     = 1234;                     // Default server port

// Default Ethernet configuration of IP, Gateway and Subnet
IPAddress ip                            (192, 168, 1, 222);
IPAddress gateway                       (192, 168, 1, 1);
IPAddress subnet                        (255, 255, 255, 0);

//-----------------------------------------------------------------------------------------------------------------
// Global Flags 
bool isWiFiConnected                    = false;              // Wifi Connection Flag
bool webServerStarted                   = false;              // Web Server 
bool OTA_ENABLE_FLAG                    = false;              // OTA Flag
bool isAuthenticated                    = false;              // Login credential authentication flag 

//-----------------------------------------------------------------------------------------------------------------

// 90 Second - If no OTA process initiate after OTA command fire from wifi, device wait for 90 sec and then back to normal operations
const unsigned long OTA_TIMEOUT_PERIOD  = 90000U; 


//-----------------------------------------------------------------------------------------------------------------
// Global variable for Data interval timinig 
int DATA_INTERVAL                       = 1;                  // Default in minutes
int COUNTER_30SEC                       = 0;                  // Increase every 30 sec interval

//-----------------------------------------------------------------------------------------------------------------

//Global Variable
EthernetClient                          ethernetclient;       // Ethernet Client variable
WebServer                               server(80);           // Web server host
extern Preferences                      prefs;                // reference the global instance
ssl_timer_t                             ssl_timer;            // Timer structure varaible 
hw_timer_t *timer                       = NULL;               // Pointer to point timer related varaibles - (Hardware Timer)
Preferences                             prefs;                // NVS storage
byte                                    mac[6];               // ESP32 Wi-Fi MAC for W5500 in byte
weather_station_global_structure_t      weather_data;         // Global Structure Weather Data
String                                  ESP_MAC;              // FOR ESP32 MAC
//-----------------------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------------------------------
//Functions Declaration - (Developed in main.c File)
void WiFi_Mac_to_Byte_Array();
void sava_ethernet_config_NVS(IPAddress ip, IPAddress gateway, IPAddress subnet);
void Send_Data_If_Ethernet_Connected();
void startWebServer();
void cleanupClients();
uint8_t xor_checksum(const char *data, uint16_t length);


//-----------------------------------------------------------------------------------------------------------------
// Function : Convert ESP32 Wi-Fi MAC to byte array
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void WiFi_Mac_to_Byte_Array() {
  ESP_MAC = WiFi.macAddress();
  int macVals[6];
  sscanf(ESP_MAC.c_str(), "%x:%x:%x:%x:%x:%x", &macVals[0], &macVals[1], &macVals[2], &macVals[3], &macVals[4], &macVals[5]);
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)macVals[i];
  }
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Save Ethernet settings to NVS
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void sava_ethernet_config_NVS(IPAddress ip, IPAddress gateway, IPAddress subnet) {
  prefs.begin("ethconfig", false);
  prefs.putString("ip", ip.toString());
  prefs.putString("gateway", gateway.toString());
  prefs.putString("subnet", subnet.toString());
  prefs.end();
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Load Ethernet settings from NVS
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
bool load_ethernet_config_NVS(IPAddress &ip, IPAddress &gateway, IPAddress &subnet) {
  prefs.begin("ethconfig", true);
  String ipStr = prefs.getString("ip", "");
  String gwStr = prefs.getString("gateway", "");
  String snStr = prefs.getString("subnet", "");
  prefs.end();

  if (ipStr != "" && gwStr != "" && snStr != "") 
  {
    if (ip.fromString(ipStr) && gateway.fromString(gwStr) && subnet.fromString(snStr)) {
      return true;
    }
  }
  return false;
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Save and Load TCP Server IP/Port to/from NVS
// Argument : Server ip address and port
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void save_server_config_NVS(IPAddress ip, uint16_t port) {
  prefs.begin("tcpconfig", false);
  prefs.putString("server_ip", ip.toString());
  prefs.putUShort("server_port", port);
  prefs.end();
}

bool load_server_config_NVS(IPAddress &ip, uint16_t &port) {
  prefs.begin("tcpconfig", true);
  String ipStr = prefs.getString("server_ip", "");
  port = prefs.getUShort("server_port", 0);
  prefs.end();

  if (ipStr != "" && ip.fromString(ipStr) && port != 0) {
    return true;
  }
  return false;
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Save and Load Data Interval timinig to/from NVS
// Argument : Server ip address and port
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void save_data_interval_NVS(int interval) {
  Preferences prefs;
  prefs.begin("interval", false);
  prefs.putInt("data_interval", interval);
  prefs.end();
}

// Function to load DATA_INTERVAL from NVS
int load_data_interval_NVS() {
  Preferences prefs;
  prefs.begin("interval", true);
  int interval = prefs.getInt("data_interval", 1); // Default to 1 min
  prefs.end();
  return interval;
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Login page logic 
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void handleLoginPage() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Login</title>
      <style>
        body { 
          font-family: Arial; 
          margin: 0; 
          height: 100vh; 
          display: flex; 
          justify-content: center; 
          align-items: center; 
          background: #f4f4f4;
        }
        .card { 
          border: 1px solid #ddd; 
          border-radius: 8px; 
          padding: 24px; 
          width: 320px; 
          background: white;
          box-shadow: 0px 4px 10px rgba(0,0,0,0.1);
          text-align: center;
        }
        input { 
          margin: 10px 0; 
          padding: 10px; 
          width: 100%; 
          box-sizing: border-box; 
        }
        .btn { 
          margin-top: 12px; 
          padding: 10px; 
          width: 100%; 
          background: #007bff; 
          color: white; 
          border: none; 
          border-radius: 4px; 
          cursor: pointer; 
        }
        .btn:hover { background: #0056b3; }
      </style>
    </head>
    <body>
      <div class="card">
        <h2>Login</h2>
        <form action="/" method="get">
          <input type="text" name="username" placeholder="Username" required><br>
          <input type="password" name="password" placeholder="Password" required><br>
          <input class="btn" type="submit" value="Login">
        </form>
      </div>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

//-----------------------------------------------------------------------------------------------------------------
// Function : Logicn Page credential verifying and redirecting logic 
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void handleDoLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String user = server.arg("username");
    String pass = server.arg("password");
    if (user == "admin" && pass == "admin") {
      isAuthenticated = true;
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "Redirecting...");
      return;
    }
  }
  server.send(401, "text/html", "<h3>Login Failed. <a href=\"/login\">Try Again</a></h3>");
}


//-----------------------------------------------------------------------------------------------------------------
// Live Data block (served separately for AJAX refresh)
void handleLiveData() {
  String html = R"rawliteral(
    <table class="kv">
      <tr><td>CO (ppm)</td><td>%CO%</td></tr>
      <tr><td>O₃ (ppm)</td><td>%O3%</td></tr>
      <tr><td>NO₂ (ppm)</td><td>%NO2%</td></tr>
      <tr><td>H₂S (ppm)</td><td>%H2S%</td></tr>
      <tr><td>SO₂ (ppm)</td><td>%SO2%</td></tr>
      <tr><td>NH₃ (ppm)</td><td>%NH3%</td></tr>
      <tr><td>CO₂ (ppm)</td><td>%CO2%</td></tr>

      <tr><td>PM2.5 (µg/m³)</td><td>%PM25%</td></tr>
      <tr><td>PM10 (µg/m³)</td><td>%PM10%</td></tr>

      <tr><td>Ambient Light (Lux)</td><td>%LUX%</td></tr>

      <tr><td>UV Raw</td><td>%UVRAW%</td></tr>
      <tr><td>UV Index</td><td>%UV%</td></tr>
      <tr><td>UV Irradiance</td><td>%UVIRR%</td></tr>

      <tr><td>BME680 Temperature (°C)</td><td>%BMETEMP%</td></tr>
      <tr><td>BME680 Humidity (%)</td><td>%BMEHUM%</td></tr>
      <tr><td>BME680 Pressure (hPa)</td><td>%BMEPRESS%</td></tr>

      <tr><td>Rain (mm)</td><td>%RAIN%</td></tr>
    </table>
  )rawliteral";

  html.replace("%LUX%", String(weather_data.lux, 2));
  html.replace("%PM25%", String(weather_data.pm2_5, 2));
  html.replace("%PM10%", String(weather_data.pm10, 2));
  html.replace("%CO%",   String(weather_data.CO_ppm,2));
  html.replace("%CO2%",  String(weather_data.CO2_ppm));
  html.replace("%H2S%",  String(weather_data.H2S_ppm,3));
  html.replace("%NH3%",  String(weather_data.NH3_ppm,2));
  html.replace("%NO2%",  String(weather_data.NO2_ppm,3));
  html.replace("%O3%",   String(weather_data.O3_ppm,2));
  html.replace("%SO2%",  String(weather_data.SO2_ppm,3));
  html.replace("%RAIN%", "0.0");
  html.replace("%UVRAW%", String(weather_data.uv_raw));
  html.replace("%UV%", String(weather_data.uvi, 2));
  html.replace("%UVIRR%", String(weather_data.uv_irradiance, 2));
  html.replace("%BMETEMP%",  String(weather_data.BME680_Temperature, 2));
  html.replace("%BMEHUM%",   String(weather_data.BME680_Humidity, 2));
  html.replace("%BMEPRESS%", String(weather_data.BME680_Pressure, 2));


  server.send(200, "text/html", html);
}





//-----------------------------------------------------------------------------------------------------------------
// Function : Web form (no JS validation)
// Argument : void
// Return   : void
// Note     : If you are updating the code then Change the Version of the Code in the <h1>Version_V_1.0</h1>  
//-----------------------------------------------------------------------------------------------------------------
void handleRoot() {
  if (!server.hasArg("username") || !server.hasArg("password")) {
    handleLoginPage();
    return;
  }

  String user = server.arg("username");
  String pass = server.arg("password");

  if (user == "admin" && pass == "admin") {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <meta charset="utf-8">
        <title>GMC AQI</title>
        <style>
          body { font-family: Arial, sans-serif; margin: 24px; }
          h1 { margin-bottom: 4px; }
          .card { border: 1px solid #ddd; border-radius: 8px; padding: 16px; margin: 12px 0; }
          .row { margin: 8px 0; }
          label { display: inline-block; width: 180px; }
          input[type="text"], input[type="number"], input[type="date"], input[type="time"] { width: 220px; }
          hr { margin: 24px 0; }
          .btn { padding: 8px 14px; border-radius: 6px; border: 1px solid #999; background: #f7f7f7; cursor: pointer; }
          .btn:hover { background: #efefef; }
          table.kv { width: 100%; border-collapse: collapse; }
          table.kv td { padding: 6px 8px; border-bottom: 1px dashed #e3e3e3; }
          table.kv td:first-child { width: 260px; color: #444; }
          small { color: #666; }
        </style>
      </head>
      <body>
        <h1>GMC AQI Version_1.0</h1>

        <div class="card">
          <h2>Live Sensor Data</h2>
          <div id="live-data">Loading...</div>
          <small>Values refresh every 5 seconds. Lux updates live; the rest are dummy placeholders.</small>
        </div>

        <div class="card">
          <h2>Ethernet, TCP Server & RTC Configuration</h2>
          <form action="/set" method="get">
            <h3>Ethernet and TCP Server</h3>
            <div class="row"><label>IP Address:</label><input type="text" name="ip" value="%IP%"></div>
            <div class="row"><label>Gateway:</label><input type="text" name="gateway" value="%GATEWAY%"></div>
            <div class="row"><label>Subnet:</label><input type="text" name="subnet" value="%SUBNET%"></div>
            <div class="row"><label>TCP Server IP:</label><input type="text" name="serverip" value="%SERVERIP%"></div>
            <div class="row"><label>TCP Server Port:</label><input type="number" name="serverport" value="%SERVERPORT%"></div>

            <h3>RTC Date & Time</h3>
            <div class="row"><label>Date (YYYY-MM-DD):</label><input type="date" name="date"></div>
            <div class="row"><label>Time (HH:MM:SS):</label><input type="time" name="time" step="1"></div>

            <h3>Data Interval</h3>
            <div class="row"><label>Interval (in minutes):</label><input type="number" name="datainterval" value="%DATAINTERVAL%"></div>

            <div class="row"><input class="btn" type="submit" value="Save All Configurations"></div>
          </form>
        </div>

        <hr>
        <div class="card">
          <form action="/ota" method="get">
            <input class="btn" type="submit" value="Enable OTA">
          </form>
        </div>

        <script>
          function fetchLiveData() {
            fetch('/livedata')
              .then(response => response.text())
              .then(html => { document.getElementById("live-data").innerHTML = html; })
              .catch(err => console.error("Error fetching live data:", err));
          }
          setInterval(fetchLiveData, 5000);
          fetchLiveData();
        </script>
      </body>
      </html>
    )rawliteral";

    html.replace("%IP%", ip.toString());
    html.replace("%GATEWAY%", gateway.toString());
    html.replace("%SUBNET%", subnet.toString());
    html.replace("%SERVERIP%", serverIP.toString());
    html.replace("%SERVERPORT%", String(serverPort));
    html.replace("%DATAINTERVAL%", String(DATA_INTERVAL));

    server.send(200, "text/html", html);
  } else {
    server.send(401, "text/html", "<h3>Invalid login. <a href=\"/\">Try Again</a></h3>");
  }
}

//-----------------------------------------------------------------------------------------------------------------
// Function : Handle Ethernet configuration submission
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void handleSet() {
  if (server.hasArg("ip") && server.hasArg("gateway") && server.hasArg("subnet")) {
    IPAddress newIP, newGateway, newSubnet;

    if (newIP.fromString(server.arg("ip")) &&
        newGateway.fromString(server.arg("gateway")) &&
        newSubnet.fromString(server.arg("subnet"))) {

      ip = newIP;
      gateway = newGateway;
      subnet = newSubnet;

      sava_ethernet_config_NVS(ip, gateway, subnet);

      // TCP Server config
      if (server.hasArg("serverip") && server.hasArg("serverport")) {
        IPAddress newServerIP;
        uint16_t newServerPort = server.arg("serverport").toInt();
        if (newServerIP.fromString(server.arg("serverip"))) {
          serverIP = newServerIP;
          serverPort = newServerPort;
          save_server_config_NVS(serverIP, serverPort);
        }
      }

      // RTC Config
      if (server.hasArg("date") && server.hasArg("time")) 
      {
        String dateStr = server.arg("date");                  // Format: YYYY-MM-DD
        String timeStr = server.arg("time");                  // Format: HH:MM:SS

        uint16_t fullYear = dateStr.substring(0, 4).toInt();  // Full year
        uint8_t year = fullYear % 100;                        // RTC accepts only two digits
        uint8_t month = dateStr.substring(5, 7).toInt();
        uint8_t date = dateStr.substring(8, 10).toInt();

        uint8_t hour = timeStr.substring(0, 2).toInt();
        uint8_t min  = timeStr.substring(3, 5).toInt();
        uint8_t sec  = timeStr.substring(6, 8).toInt();

        //Serial.printf("Parsed Date/Time: %02d/%02d/20%02d  %02d:%02d:%02d\n", date, month, year, hour, min, sec);

        uint8_t day = 1;  // Optional: could calculate actual weekday if needed
        setRTCDateTime(sec, min, hour, day, date, month, year);
      }

      // DATA_INTERVAL handling
      if (server.hasArg("datainterval")) {
        DATA_INTERVAL = server.arg("datainterval").toInt();
        save_data_interval_NVS(DATA_INTERVAL);
      }

      // Reset W5500
      digitalWrite(W5500_RST, LOW);
      delay(100);
      digitalWrite(W5500_RST, HIGH);
      delay(200);

      Ethernet.init(W5500_CS);
      Ethernet.begin(mac, ip, gateway, gateway, subnet);

      String msg = "<h3>Configuration Saved Successfully!</h3>";
      msg += "<p><b>IP:</b> " + ip.toString() + "</p>";
      msg += "<p><b>Gateway:</b> " + gateway.toString() + "</p>";
      msg += "<p><b>Subnet:</b> " + subnet.toString() + "</p>";
      msg += "<p><b>Server IP:</b> " + serverIP.toString() + "</p>";
      msg += "<p><b>Server Port:</b> " + String(serverPort) + "</p>";
      msg += "<form action=\"/restart\" method=\"get\">";
      msg += "<input type=\"submit\" value=\"Restart ESP32\">";
      msg += "</form>";

      server.send(200, "text/html", msg);
      return;
    }
  }

  server.send(400, "text/plain", "Invalid parameters");
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Handle ESP32 restart from web
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void handleRestart() {
  server.send(200, "text/html", "<h3>Restarting ESP32...</h3>");
  delay(500);
  ESP.restart();
}

//-----------------------------------------------------------------------------------------------------------------
// Function : For stoping client connection
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void cleanupClients()
{
    WiFiClient client = server.client();
    if (client && !client.connected())
    {
        client.stop();
    }
}


//-----------------------------------------------------------------------------------------------------------------
// Function : OTA Handler
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void handleOTAEnable() {
  OTA_ENABLE_FLAG = true;
  server.send(200, "text/html", "<h3>OTA Enabled. You can now start the OTA process.</h3>");
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Pinting ESP MAC Address 
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void Ethernet_MAC_Address()
{
  Serial.print(F("Ethernet MAC: "));
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

bool tryWiFiConnection(unsigned long timeout_ms) {
  WiFi.mode(WIFI_STA);  // Ensure STA mode
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Trying to connect to WiFi"));

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout_ms) {
    delay(100);
    //Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi connected!"));
    Serial.println("IP Address: " + WiFi.localIP().toString());
    isWiFiConnected = true;
    return true;
  } else
  {
    Serial.println(F("\nWiFi connection failed."));
    isWiFiConnected = false;
    return false;
  }
}


//-----------------------------------------------------------------------------------------------------------------
// Function : Starting Web Page Service  
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void startWebServer() 
{
  if (!webServerStarted) 
  {
    server.on("/", handleRoot);
    server.on("/login", handleLoginPage);
    server.on("/dologin", HTTP_POST, handleDoLogin);
    server.on("/set", handleSet);
    server.on("/restart", handleRestart);
    server.on("/ota", handleOTAEnable);       // OTA Route
    server.on("/livedata", handleLiveData);  // << NEW
    server.begin();
    webServerStarted = true;
    if (WiFi.status() == WL_CONNECTED) 
    {
      Serial.println("Web server started at: http://" + WiFi.localIP().toString());
    }
  }
}


//-----------------------------------------------------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);

  //while (!Serial) 
  //{
   //delay(10);                                   // Wait for USB connection
  //}
  //Serial.println("USB CDC Serial Connected");   // UART on USB
  
  Serial.print(F("APB CLK SET = "));            // To know our ESP32 operates on defined frequency
  Serial.println(rtc_clk_apb_freq_get());

  delay(1000);                                  // Initialization delay

  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, HIGH);

  pinMode(SDA_PIN, OUTPUT);   
  pinMode(SCL_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);     // I2C initialization

  gsmSetup();

  BME680_Init();                    // BME680 initialization                                              

  // WiFi initalization
  tryWiFiConnection(3000);
   if (isWiFiConnected) {
    startWebServer();
  }

  WiFi_Mac_to_Byte_Array();         // Converting ESP MAC Address in Byte Array for Ethernet Module 
  Ethernet_MAC_Address();           // Printing ESP MAC Address on Serial monitor 
  
  gas_sensor_init();                // UART Multiplexer initialization            
  veml7700_init();                  // Ambient Light initialization
  init_LTR390();                    // UV radiation sensor LTR390 initialization

  sps_30_init();                    // PM sensor initialization
  S300E_Init();                     // CO2 sensor initialization
  initRTC();                        // RTC initialization 

  load_ethernet_config_NVS(ip, gateway, subnet);      // Saving Ethernet Configuration to NVS
  load_server_config_NVS(serverIP, serverPort);       // Saving TCP Server Configuration to NVS
  DATA_INTERVAL = load_data_interval_NVS();           // Saving Data Interval to NVS

  Ethernet.init(W5500_CS);                            // Ethernet Module initialization 
  Ethernet.begin(mac, ip, gateway, gateway, subnet);  // Assigning Default MAC, IP Address, Gateway, Subnet to Ethernet Module

  Serial.println(F("Ethernet started:"));
  Serial.print("Local IP: "); Serial.println(Ethernet.localIP());
  Serial.print("Server IP: "); Serial.println(serverIP);
  Serial.print("Server Port: "); Serial.println(serverPort);

  // Initializing Web Page Service 
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/restart", handleRestart);
  server.on("/ota", handleOTAEnable);
  server.on("/livedata", handleLiveData);
  
  server.begin();
  Serial.println("Web server started at: http://" + WiFi.localIP().toString());


  //Initialize OTA related box
  init_ssl_ota();
  init_hw_ssl_timer(timer,&ssl_timer);                      // After all init process complete, lets start Timer 

  // Initialize the Task Watchdog Timer (WDT)
  // 30 Second Timeout (To avoid controller's reboot during OTA process)
  esp_task_wdt_init(30, true);                                              // Set timeout to 30 seconds, and enable panic handling
  esp_task_wdt_add(NULL);                                                   // Add the current task to WDT monitoring

}

//-----------------------------------------------------------------------------------------------------------------
// Main loop
//-----------------------------------------------------------------------------------------------------------------
void loop() 
{
  //if (isWiFiConnected) 
  {
    server.handleClient();
  }

  if (OTA_ENABLE_FLAG == true) 
  {
    esp_task_wdt_reset();  // Prevent WDT during OTA pause

    //OTA begin() - Initialization + Network listening begin
    ArduinoOTA.begin();

    Serial.println("ESP32 machine is ready for OTA");
    //Time stamp when device in OTA mode
    unsigned long OTA_START_TIME = millis();

    //Wait for certain time else back to normal mode
    while((millis() - OTA_START_TIME) < OTA_TIMEOUT_PERIOD)
    {
      ArduinoOTA.handle();
      esp_task_wdt_reset();
    }

    //Reset Flag & stop OTA listening - Timeout
    ArduinoOTA.end();    
    OTA_ENABLE_FLAG = false;
    Serial.println("[OTA] OTA window expired. Resuming normal operation.");
    esp_task_wdt_reset();

    // Add actual OTA setup here (if using ArduinoOTA or ElegantOTA)
  }

  //Clear Watchdog Frequently....
  esp_task_wdt_reset();

  //--------------------------------------------------------------------------------------------------------------
  
  //--------------------------------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------------------------------
  //---------------------------------------------------------------------------------------------------------------
  //Time based functionality (5SEC. / 10 SEC. / 20 SEC. / 40 SEC. / 50 SEC.
  
  //5 Seconds elapsed ??  -> For Very high priority Task
  if(ssl_timer.FIVE_SECOND_ELAPSED_FLAG == true)
  {
    //RESET FLAG 
    ssl_timer.FIVE_SECOND_ELAPSED_FLAG = false;
    
    esp_task_wdt_reset();

    veml7700_task();                          // Ambient light data 
    read_LTR390();                            // UV radiation data
    readRTCDateTimeToStruct(&weather_data);   // RTC data

    esp_task_wdt_reset();
    
  }

  //---------------------------------------------------------------------------------------------------------------
  //10 second elapsed??   -> For normal priority Task
  if(ssl_timer.TEN_SECOND_ELAPSED_FLAG == true)
  {
    //RESET FLAG 
    ssl_timer.TEN_SECOND_ELAPSED_FLAG = false;

    
    esp_task_wdt_reset();

    sps_30_data();          // PM sensor data
    BME680_Process();       // BME680 data

    // WiFi reconnection logic
    if((!isWiFiConnected) || ((WiFi.status() != WL_CONNECTED)))
    {
      isWiFiConnected = false;
      bool connected = tryWiFiConnection(3000);  // Try for 3 seconds

      if (connected && (WiFi.status() == WL_CONNECTED))
      {
        startWebServer();
        //Serial.println("Web server started at: http://" + WiFi.localIP().toString());
      } 
      else 
      {
        Serial.println(F("WiFi connection failed. WL_CONNECT_FAILED"));
      } 
    
    }

    cleanupClients();
    
    esp_task_wdt_reset();
  }


  //---------------------------------------------------------------------------------------------------------------
  //30 second elapsed ??  -> For Ideal Task
  if(ssl_timer.THIRTY_SECOND_ELAPSED_FLAG == true)
  {
    //RESET FLAG 
    ssl_timer.THIRTY_SECOND_ELAPSED_FLAG = false;

    esp_task_wdt_reset();

    gas_sensor_task();      // Gases sensor data
    gas_sensor_print();     // Print gases sensor data
    S300E_data();           // CO2 data

    //increase 30 sec counter
    COUNTER_30SEC++;
    if(COUNTER_30SEC >= (DATA_INTERVAL * 2))
    {
      //Send_Data_If_Ethernet_Connected();
      gsmProcessLoop();
      COUNTER_30SEC = 0;
    }

    esp_task_wdt_reset();

    //Serial.println(">>30SEC...");

  }


  //---------------------------------------------------------------------------------------------------------------
  //40 second elapsed ??  -> For Low priority Task
  if(ssl_timer.FOURTY_SECOND_ELAPSED_FLAG == true)
  {
    //RESET FLAG 
    ssl_timer.FOURTY_SECOND_ELAPSED_FLAG = false;

    //Clear Watchdog Frequently....
    esp_task_wdt_reset();

  
    esp_task_wdt_reset();
  }


  //---------------------------------------------------------------------------------------------------------------
  //50 seconds elapsed ? -> For very Low priority Task
  if(ssl_timer.FIFTY_SECOND_ELAPSED_FLAG == true)
  {
    //RESET FLAG 
    ssl_timer.FIFTY_SECOND_ELAPSED_FLAG = false;

    //Clear Watchdog Frequently....
    esp_task_wdt_reset();
    
    
    
    esp_task_wdt_reset();

    //Serial.println(">>50SEC...");
  }

}

//-----------------------------------------------------------------------------------------------------------------
// Function : Sending data of sensors through Ethernet module to TCP Server 
// Argument : void
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
void Send_Data_If_Ethernet_Connected()
{
  if (!ethernetclient.connected()) 
  {
    Serial.println(F("Connecting to server..."));
    if (ethernetclient.connect(serverIP, serverPort)) 
    {
      Serial.println(F("Connected to server."));

      String message = "*";
      String crc_input = "";

      // Construct message body (human-readable with delimiters)
      message += String(ESP_MAC);
      message += "," + String(weather_data.rtc_date) + "/" + String(weather_data.rtc_month) + "/" + String(weather_data.rtc_year);
      message += "T" + String(weather_data.rtc_hour) + ":" + String(weather_data.rtc_min) + ":" + String(weather_data.rtc_sec);
      
      
      

      //message += "," + String(weather_data.BMP280_temperature, 2);
      //message += "," + String(weather_data.ZPHS01_humidity, 2);
      //message += "," + String(weather_data.BMP280_pressure, 2);

      // Construct raw string for CRC (no formatting or symbols)
      crc_input += ESP_MAC;
      crc_input += String(weather_data.rtc_date);
      crc_input += String(weather_data.rtc_month);
      crc_input += String(weather_data.rtc_year);
      crc_input += String(weather_data.rtc_hour);
      crc_input += String(weather_data.rtc_min);
      crc_input += String(weather_data.rtc_sec);
      
      

      uint8_t crc = xor_checksum(crc_input.c_str(), crc_input.length());

      // Append CRC and end signature
      message += "," + String(crc, DEC);
      message += "#\r\n";

      ethernetclient.println(message);
      Serial.println("TCP_SEND: " + message);
      ethernetclient.stop();
    }
  }
  else
  {
    Serial.println(F("Failed to connect to server."));
  }
}

//-----------------------------------------------------------------------------------------------------------------
// Function : XOR CHECKSUM
// Argument : Pointer of Data of sensor 
// Return   : void
//-----------------------------------------------------------------------------------------------------------------
uint8_t xor_checksum(const char *data, uint16_t length)
{
    uint8_t checksum = 0x00;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}



