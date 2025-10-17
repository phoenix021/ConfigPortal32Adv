
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
#include "ConfigPortalUtils.h"

char* ssid_pfix = (char*)"CaptivePortal";
String user_config_html = "";

StaticJsonDocument<JSON_BUFFER_LENGTH> cfg;
volatile bool configDone = false;
char cfgFile[] = "/config.json";
WebServer webServer(80); 

void save_config_json() {
  File f = LittleFS.open(cfgFile, "w");
  serializeJson(cfg, f);
  f.close();
}


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

  // set the pin mode through interface
  set_boot_pin_mode();

  init_display();
  display_hello_world();
  testLittleFS();

  //loadConfig();

  // TODO: the configDevice() call needs to be removed in production
  //configDevice();

  setConfigReference(&cfg);
  loadConfigWithSavedNetworks();

  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    // Enter config mode (start AP and web portal)
    Serial.println("Entered config mode...");
    display_message("Config Mode", "...", 2000);
    configDevice();
  }

  bool connected = false;
  while (WiFi.status() != WL_CONNECTED) {
      if (!connect_to_known_networks()) {
        display_message_extended("WiFi Failed", "No known networks available", "Please configure...", 2000);
        configDevice();  // fallback to AP config portal
        connected = wifiConnect(cfg["ssid"], cfg["w_pw"],50000);
        return;
      }
    }
 
  // main setup
  Serial.printf("\nIP address : ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("miniwifi")) {
    Serial.println("MDNS responder started");
  }

  startTelnetServer();
  
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
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
  //Serial.println("Loop is running...");
  //delay(500);

  handleSerialCommands();
  handleBootButton();

  readTelnetStream();
  
  ArduinoCloud.update();
}
