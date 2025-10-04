
#include <Arduino.h>
#include <ESPmDNS.h>
#include "ConfigPortal32Adv.h"
#include <LittleFS.h>
#include <Wire.h>

#include <ArduinoIoTCloud.h>
#include <LittleFS.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiSTA.h>
#include <WiFiAP.h>
#include <WiFi.h>
#include <WiFiScan.h>
#include <WiFiUdp.h>
#include <WiFiGeneric.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>

#include <Arduino_Lzss.h>
#include <Arduino_FlashFormatter.h>
#include <Arduino_HEX.h>
#include <Arduino_TimedAttempt.h>
#include <Arduino_CBOR.h>
#include <Arduino_CRC32.h>
#include <Arduino_SHA256.h>
#include <Arduino_TinyCBOR.h>
#include <Arduino_CRC16.h>

#include "thingProperties.h"

char* ssid_pfix = (char*)"CaptivePortal";
String user_config_html = "";

InputField myInputs[] = {
  // Basic text input
  { "text", "dev_name", "Device Name", "ESP32-C3", false },

  // Checkbox
  { "checkbox", "debug", "Enable Debug", nullptr, true },

  // Password field
  { "password", "admin_pw", "Admin Password", "", false },

  // Email input
  { "email", "user_email", "Email Address", "user@example.com", false },

  // Number input
  { "number", "max_clients", "Max Clients", "5", false },

  // Date input
  { "date", "install_date", "Installation Date", "2025-08-15", false },

  // Time input
  { "time", "start_time", "Start Time", "08:00", false },

  // Color picker
  { "color", "theme_color", "Theme Color", "#ff0000", false }

};

InputGroup userInputs = {
  myInputs,
  sizeof(myInputs) / sizeof(myInputs[0])
};


/*
 *  ConfigPortal library to extend and implement the WiFi connected IOT device
 *
 *  Yoonseok Hur
 *
 *  Usage Scenario:
 *  0. copy the example template in the README.md
 *  1. Modify the ssid_pfix to help distinquish your Captive Portal SSID
 *          char   ssid_pfix[];
 *  2. Modify user_config_html to guide and get the user config data through the Captive Portal
 *          String user_config_html;
 *  2. declare the user config variable before setup
 *  3. In the setup(), read the cfg["meta"]["your field"] and assign to your config variable
 *
 */

void testLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  File file = LittleFS.open("/test_example.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("File Content:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}


void setup() {
  Serial.begin(115200);

  u8g2.begin();

  u8g2_prepare();

  //u8g2.clearBuffer();          // clear the internal memory
  //u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawStr(0,10,"Hello World!");  // write something to the internal memory
  u8g2.sendBuffer();  

  testLittleFS();

  loadConfig();
  // TODO: the configDevice() call needs to be removed in production
  //configDevice();
  
  // *** If no "config" is found or "config" is not "done", run configDevice ***
  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    configDevice();
  }


  unsigned long startAttemptTime = millis();
  const unsigned long WIFI_TIMEOUT_MS = 20000; // 20 seconds
  // Normal startup, no config
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Connecting ... ");
    u8g2.sendBuffer();
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect. Starting AP mode.");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Failed to connect. Starting AP mode.");
    u8g2.sendBuffer();
    configDevice();  // start AP + portal to configure new credentials
  }

  u8g2_prepare();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Wifi connected");
  u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
  // main setup
  Serial.printf("\nIP address : ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("miniwifi")) {
    Serial.println("MDNS responder started");
  }

  
  
  //WiFiConnectionHandler ArduinoIoTPreferredConnection;
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void loop() {
  Serial.println("Loop is running...");
  delay(500);

  ArduinoCloud.update();
}



/*
    Since Temperature is READ_WRITE variable, onTemperatureChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onTemperatureChange()  {
    // Add your code here to act upon Temperature change
  }
  
  /*
    Since Humidity is READ_WRITE variable, onHumidityChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onHumidityChange()  {
    // Add your code here to act upon Humidity change
  }
  
  /*
    Since LightLevel is READ_WRITE variable, onLightLevelChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onLightLevelChange()  {
    // Add your code here to act upon LightLevel change
  }
  
  /*
    Since SoilMoisture is READ_WRITE variable, onSoilMoistureChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onSoilMoistureChange()  {
    // Add your code here to act upon SoilMoisture change
  }
  
  /*
    Since PumpState is READ_WRITE variable, onPumpStateChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onPumpStateChange()  {
    // Add your code here to act upon PumpState change
  }
  

#include <HTTPUpdate.h>

void doOTAUpdate() {
  t_httpUpdate_return ret = HTTP_UPDATE_OK;
  //httpUpdate.update("http://your-server.com/firmware.bin");

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed. Error (%d): %s\n", 
        httpUpdate.getLastError(), 
        httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("Update successful, rebooting...");
      //ESP.restart();
      break;
  }
}
