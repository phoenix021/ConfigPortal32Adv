
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

#define BOOT_PIN 00


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

  pinMode(BOOT_PIN, INPUT_PULLUP); 

  u8g2.begin();

  u8g2_prepare();

  //u8g2.clearBuffer();          // clear the internal memory
  //u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawStr(0,10,"Hello World!");  // write something to the internal memory
  u8g2.sendBuffer();  

  testLittleFS();

  //loadConfig();
  loadConfigWithSavedNetworks();
  // TODO: the configDevice() call needs to be removed in production
  //configDevice();
  
  // *** If no "config" is found or "config" is not "done", run configDevice ***
  //if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
  //  configDevice();
  //}

  if (!cfg.containsKey("savedNetworks") || !cfg["savedNetworks"].is<JsonArray>() ) {
    cfg["savedNetworks"] = JsonArray();
  }

  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    // Enter config mode (start AP and web portal)
    configDevice();

    // After config saved via web portal:

    //saveNewNetwork((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    //saveConfigToFile();
  }

  while (WiFi.status() != WL_CONNECTED) {
    loadConfigWithSavedNetworks();
    if (!connectToKnownNetworks()) {
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_ncenB08_tr);
     u8g2.drawStr(0, 10, "WiFi Failed");
     u8g2.drawStr(0, 30, "Starting AP mode");
     u8g2.sendBuffer();
     delay(2000); // optional pause to read
     configDevice();  // fallback to AP config portal
    } else {
           u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_ncenB08_tr);
     u8g2.drawStr(0, 10, "WiFi  Success");
     u8g2.drawStr(0, 30, "in setup code part");
     u8g2.sendBuffer();
    }
  }

  //saveNewNetwork((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect. Starting AP mode.");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "WiFi Failed");
    u8g2.drawStr(0, 30, "Starting AP mode");
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

  cfg["lastConnectedSSID"] = WiFi.SSID();;
  //save_config_json();
  
  
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

void format_config_json() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS. Cannot erase config.");
    return;
  }

  if (LittleFS.exists("/config.json")) {
    Serial.println("Erasing /config.json...");
    LittleFS.remove("/config.json");
    Serial.println("Config erased.");
  } else {
    Serial.println("No config file found to erase.");
  }
}

void loop() {
  Serial.println("Loop is running...");
  delay(500);

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "eraseconfig") {
      format_config_json();
      Serial.println("Restarting...");
      delay(1000);
      ESP.restart();
    }
  }

  if (digitalRead(BOOT_PIN) == LOW) {
    Serial.println("Boot dugme pritisnuto"); 
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "BOOT button pressed");
    u8g2.drawStr(0, 30, "Going to config mode");
    u8g2.sendBuffer();
    delay(1500);
    configDevice();
  } else {
    Serial.println("Click the boot button to configure new wifi network");
  }
  ArduinoCloud.update();
}




bool connectToNetwork() {
  const char* lastSSID = cfg["lastConnectedSSID"] | "";

  if (strlen(lastSSID) > 0) {
    // Try connecting to last connected SSID first
    JsonArray savedNetworks = cfg["savedNetworks"].as<JsonArray>();
    for (JsonObject net : savedNetworks) {
      if (strcmp(net["ssid"], lastSSID) == 0) {
        Serial.print("Trying last connected network: ");
        Serial.println(lastSSID);
        if (wifiConnect(net["ssid"], net["password"])) {
          Serial.println("Connected to last connected network!");
          return true;
        }
      }
    }
  }

  // If last connected network fails or not set, try others
  JsonArray savedNetworks = cfg["savedNetworks"].as<JsonArray>();
  for (JsonObject net : savedNetworks) {
    if (strcmp(net["ssid"], lastSSID) == 0) {
      // Already tried this one
      continue;
    }
    Serial.print("Trying saved network: ");
    Serial.println(net["ssid"].as<const char*>());
    if (wifiConnect(net["ssid"], net["password"])) {
      Serial.println("Connected!");
      cfg["lastConnectedSSID"] = net["ssid"].as<const char*>();
      save_config_json();
      return true;
    }
  }
  
  Serial.println("Failed to connect to any saved networks.");
  return false;
}
  
bool connectToKnownNetworks() {
  Serial.println("connectToKnownNetworks() called");

  if (!cfg.containsKey("savedNetworks")) {
    Serial.println("No saved networks in config.");
    return false;
  }

  JsonArray savedNetworks = cfg["savedNetworks"].as<JsonArray>();
  Serial.printf("Found %d saved networks.\n", savedNetworks.size());

  // Scan available SSIDs
  int n = WiFi.scanNetworks();
  Serial.printf("Scan complete. Found %d networks.\n", n);

  std::vector<String> visibleSSIDs;
  for (int i = 0; i < n; i++) {
    visibleSSIDs.push_back(WiFi.SSID(i));
    Serial.printf(" - %s (RSSI: %d)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  // Try to connect only to visible saved networks
  for (JsonObject net : savedNetworks) {
    const char* ssid = net["ssid"];
    const char* password = net["password"];

    if (std::find(visibleSSIDs.begin(), visibleSSIDs.end(), ssid) == visibleSSIDs.end()) {
      Serial.printf("Skipping %s: Not visible.\n", ssid);
      continue;
    }

    Serial.printf("Trying saved network: %s\n", ssid);
    if (wifiConnect(ssid, password)) {
      Serial.println("Successfully connected.");
      return true;
    } else {
      Serial.println("Connection failed.");
    }
  }

  Serial.println("No known networks could be connected.");
  return false;
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
