#ifndef CONFIG_PORTAL_INTERFACE_H
#define CONFIG_PORTAL_INTERFACE_H

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TelnetStream.h>
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
extern WebServer webServer;

#include <stdbool.h>

// Default BOOT pin number
#define BOOT_PIN 00

#define JSON_BUFFER_LENGTH 3072

extern void save_config_json();

void startTelnetServer(void);

void readTelnetStream(void);

void handleSerialCommands(void);

void handleBootButton(void);

void saveNewNetwork(const char* ssid, const char* password);

void loadConfigWithSavedNetworks(void);

// Pointer to config passed externally
void setConfigReference(StaticJsonDocument<JSON_BUFFER_LENGTH>* cfg);

// Configure the boot pin mode (INPUT_PULLUP) for the currently set boot pin
void set_boot_pin_mode(void);

// Configure the boot pin mode (INPUT_PULLUP) for the a custom pin
void set_boot_pin_mode(int pin);

// Erase the config JSON file stored in LittleFS
void format_config_json(void);

// Attempt to connect to known WiFi networks
bool connect_to_known_networks(void);

// Enter configuration mode (start AP + captive portal)
void enter_config_mode(void);

// Perform Over-The-Air firmware update
void perform_ota_update(void);

// OLED display initialization
void init_display(void);

// Display "Hello World" message on OLED
void display_hello_world(void);

// Display two lines of message on OLED, pausing for pause_ms milliseconds
void display_message(const char* line1, const char* line2, unsigned long pause_ms);

// Display three lines of message on OLED, pausing for pause_ms milliseconds
void display_message_extended(const char* line1, const char* line2, const char* line3, unsigned long pause_ms);

bool wifiConnect(const char* ssid, const char* password, uint16_t timeoutMs);

void doOTAUpdate(void);

void setupScanRoute();

void register_server_route(char* route);

/*
    Since Temperature is READ_WRITE variable, onTemperatureChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onTemperatureChange(void);
  
  /*
    Since Humidity is READ_WRITE variable, onHumidityChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onHumidityChange(void);
  
  /*
    Since LightLevel is READ_WRITE variable, onLightLevelChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onLightLevelChange(void);
  
  /*
    Since SoilMoisture is READ_WRITE variable, onSoilMoistureChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onSoilMoistureChange(void);
  
  /*
    Since PumpState is READ_WRITE variable, onPumpStateChange() is
    executed every time a new value is received from IoT Cloud.
  */
  void onPumpStateChange(void);
  

#endif // CONFIG_PORTAL_INTERFACE_H
