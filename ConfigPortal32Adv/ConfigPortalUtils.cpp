#include <ArduinoOTA.h>

#include "ConfigPortalUtils.h"
#include <Arduino.h>

//#include <HTTPUpdate.h>

//#include <Update.h>


U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

extern void format_config_json();
bool connectToKnownNetworks(void);
extern char cfgFile[];
extern String lastUpdated;
extern String lastConnectedIp;
extern bool triggerOtaUpdate;

static StaticJsonDocument<JSON_BUFFER_LENGTH>* cfgPtr = nullptr;

void setConfigReference(StaticJsonDocument<JSON_BUFFER_LENGTH>* externalCfg) {
   cfgPtr = externalCfg;
    
  if (!(*cfgPtr).containsKey("savedNetworks") || !(*cfgPtr)["savedNetworks"].is<JsonArray>() ) {
    (*cfgPtr)["savedNetworks"] = JsonArray();
  }

  if (!(*cfgPtr).containsKey("lastUpdated")) {
    (*cfgPtr)["lastUpdated"] = "";
  }

  if (!(*cfgPtr)["lastConnectedIp"]) {
    (*cfgPtr)["lastConnectedIp"] = "";
  }

  if (!(*cfgPtr)["triggerOtaUpdate"]) {
    (*cfgPtr)["triggerOtaUpdate"] = false;
  }
}

void populateCloudProps(){
  lastUpdated = (String) (*cfgPtr)["lastUpdated"];
  lastConnectedIp = (String) (*cfgPtr)["lastConnectedIp"];
  triggerOtaUpdate = (String) (*cfgPtr)["triggerOtaUpdate"];
}

extern void configDevice();
extern void doOTAUpdate();

bool otaRequested = false;

void startTelnetServer(void){
  TelnetStream.begin();
}


String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

HTTPClient http;

void performHttpOTA(const char* bin_url) {
  lastUpdated = "Update started at: " + getFormattedTime();
  ArduinoCloud.update();
  delay(2000);

  http.begin(bin_url);  // e.g., "http://your-server.com/firmware.bin"
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    if (!Update.begin(len)) {
      Serial.println("Update failed to begin");
      lastUpdated = "Update failed to begin at: " + getFormattedTime();
      ArduinoCloud.update();
      return;
    }

    size_t written = Update.writeStream(*stream);
    if (written == len) {
      Serial.println("Update written successfully");
      lastUpdated = "Update written successfully at: " + getFormattedTime();
      ArduinoCloud.update();
    } else {
      Serial.printf("Only %d/%d bytes written\n", written, len);
    }
    delay(500);

    if (Update.end()) {
      Serial.println("Update complete");
      lastUpdated = "Update complete at: " + getFormattedTime();
      (*cfgPtr)["lastUpdated"] = lastUpdated;
      save_config_json();
      delay(2000);
      if (Update.isFinished()) {
        Serial.println("Rebooting...");
        lastUpdated = "Rebooting at: " + getFormattedTime();
        otaRequested = false;
        triggerOtaUpdate = false;
        (*cfgPtr)["triggerOtaUpdate"] = triggerOtaUpdate;
        save_config_json();
        ArduinoCloud.update();
        delay(5000);
        ESP.restart();
      } else {
        Serial.println("Update not finished?");
        lastUpdated = "Update not finished at: " + getFormattedTime();
        ArduinoCloud.update();
      }
    } else {
      Serial.printf("Update failed. Error #: %d\n", Update.getError());
      lastUpdated = "Update failed at: " + getFormattedTime();
      ArduinoCloud.update();
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    lastUpdated = "HTTP error occured at: " + getFormattedTime();
    ArduinoCloud.update();
  }

  http.end();
  delay(1000);
  otaRequested = false;
  triggerOtaUpdate = false;
  ArduinoCloud.update();
  save_config_json();
}

void readTelnetStream() {
  if (TelnetStream.available() ) {
    String cmd = TelnetStream.readStringUntil('\n');
    cmd.trim(); // removes \r and whitespace

    if (cmd.length() > 0) {
      char dev_id[32];
      sprintf(dev_id, "d-%012llX", ESP.getEfuseMac());
      if (Serial){
        Serial.println("[Telnet] Command: " + cmd);
      }
      TelnetStream.println("Received: " + cmd);
      TelnetStream.println("Dev id:: " + String(dev_id));

      if (cmd == "OtaAvailable"){
        //doOTAUpdate();
      }

      if (cmd == "startota") {
        TelnetStream.println("Triggering OTA update... now use Arduino IDE to upload.");
        otaRequested = true;
        perform_ota_update();
      } else {
        TelnetStream.println("Unknown command: " + cmd);
      }
    }
  }
}

void set_boot_pin_mode(void) {
    pinMode(BOOT_PIN, INPUT_PULLUP);
}

void set_boot_pin_mode(int pin) {
    pinMode(pin, INPUT_PULLUP);
}


void u8g2_prepare(void) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontRefHeightExtendedText();
    u8g2.setDrawColor(1);
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);
}

void init_display(void) {
    u8g2.begin();
    u8g2_prepare();
}

void display_hello_world(void) {
    u8g2.clearBuffer();                          // Clear internal buffer
    u8g2.setFont(u8g2_font_ncenB08_tr);          // Optional: set specific font
    u8g2.drawStr(0, 10, "Hello World!");         // Write string
    u8g2.sendBuffer();                           // Push buffer to display
}

void display_message(const char* line1, const char* line2, unsigned long pause_ms) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    if (line1 != nullptr) {
        u8g2.drawStr(0, 10, line1);
    }
    if (line2 != nullptr) {
        u8g2.drawStr(0, 30, line2);
    }
    u8g2.sendBuffer();

    if (pause_ms > 0) {
        delay(pause_ms);
    }
}

void display_message_extended(const char* line1, const char* line2, const char* line3, unsigned long pause_ms) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    if (line1 != nullptr) {
        u8g2.drawStr(0, 10, line1);
    }
    if (line2 != nullptr) {
        u8g2.drawStr(0, 30, line2);
    }
    if (line3 != nullptr) {
        u8g2.drawStr(0, 50, line3);
    }

    u8g2.sendBuffer();

    if (pause_ms > 0) {
        delay(pause_ms);
    }
}

// Wrappers for external interface
void format_config_json_interface() {
    format_config_json();
}

extern bool connect_to_known_networks() {
    return connectToKnownNetworks();
}

void enter_config_mode() {
    configDevice();
}

void perform_ota_update() {
    performHttpOTA("http://192.168.1.136:8083/ConfigPortal32Adv.ino.esp32.bin");
}



void saveNewNetwork(const char* ssid, const char* password) {
  if (!(*cfgPtr).containsKey("savedNetworks")) {
    (*cfgPtr)["savedNetworks"] = JsonArray();
  }
  JsonArray savedNetworks = (*cfgPtr)["savedNetworks"].as<JsonArray>();

  // Check if SSID already exists and update
  for (JsonObject network : savedNetworks) {
    if (strcmp(network["ssid"], ssid) == 0) {
      network["password"] = password;
      save_config_json();
      return;
    }
  }

  // Append new network
  JsonObject newNet = savedNetworks.createNestedObject();
  newNet["ssid"] = ssid;
  newNet["password"] = password;

  save_config_json();
}

bool wifiConnect(const char* ssid, const char* password, uint16_t timeoutMs = 100000) {
  Serial.printf("Attempting to connect to SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < timeoutMs) {
    display_message(ssid, "Connecting...",5000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected to %s. IP address: %s\n", ssid, WiFi.localIP().toString().c_str());
    display_message_extended("WiFi connected", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), 3000);
    // Update last connected SSID in config'
    (*cfgPtr)["lastConnectedSSID"] = ssid;
    lastConnectedIp = WiFi.localIP().toString();
    (*cfgPtr)["lastConnectedIp"] = lastConnectedIp;
    ArduinoCloud.update();
    
    saveNewNetwork(ssid, password);
    save_config_json();
    loadConfigWithSavedNetworks();

    return true;
  } else {
      display_message("Failed to connect to:", ssid ,5000);
      return false;
  }
}


bool connectToKnownNetworks() {
  Serial.println("connectToKnownNetworks() called");

  if (!(*cfgPtr).containsKey("savedNetworks")) {
    Serial.println("No saved networks in config.");
    return false;
  }
  JsonArray savedNetworks = (*cfgPtr)["savedNetworks"].as<JsonArray>();
  Serial.printf("Found %d saved networks.\n", savedNetworks.size());
  
  const char* lastSSID = (*cfgPtr)["lastConnectedSSID"] | "";
  if (strlen(lastSSID) > 0) {
    // Try connecting to last connected SSID first
   
    for (JsonObject net : savedNetworks) {
      if (strcmp(lastSSID, net["ssid"])== 0 ){
        if (wifiConnect(lastSSID, net["password"])) {
            Serial.println("Connected to last connected network!");
            return true;
        }
      }
    }
      
  }

  // Scan available SSIDs
  int n = WiFi.scanNetworks();
  Serial.printf("Scan complete. Found %d networks.\n", n);

  std::vector<String> visibleSSIDs;
  for (int i = 0; i < n; i++) {
    visibleSSIDs.push_back(WiFi.SSID(i));
    Serial.printf(" - %s (RSSI: %d)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  const char* lastSsid = "";
  const char* lastPsw = "";
  
  // Try to connect only to visible saved networks
  for (JsonObject net : savedNetworks) {
    const char* ssid = net["ssid"];
    const char* password = net["password"];

    if (std::find(visibleSSIDs.begin(), visibleSSIDs.end(), ssid) == visibleSSIDs.end()) {
      Serial.printf("Skipping %s: Not visible.\n", ssid);
      continue;
    }

    if (wifiConnect(ssid, password)) {
        Serial.println("Successfully connected.");
        (*cfgPtr)["lastConnectedSSID"] = WiFi.SSID();
        lastConnectedIp = WiFi.localIP().toString();
        (*cfgPtr)["lastConnectedIp"] = lastConnectedIp;
        ArduinoCloud.update();
        save_config_json();
        saveNewNetwork((*cfgPtr)["ssid"], (*cfgPtr)["w_pw"]);
        serializeJsonPretty((*cfgPtr), Serial);
        loadConfigWithSavedNetworks();
        return true;
    } 
  }
  
  Serial.println("No known networks could be connected.");
  return false;
}

void printConfigInfoToSerial(){
  serializeJsonPretty((*cfgPtr), Serial);
}



void loadConfigWithSavedNetworks() {
    if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  if (LittleFS.exists(cfgFile)) {
    File f = LittleFS.open(cfgFile, "r");
    DeserializationError error = deserializeJson(*cfgPtr, f.readString());
    f.close();

    if (error) {
      Serial.println("Deserialization error, using default config");
      deserializeJson(*cfgPtr, "{\"meta\":{}, \"savedNetworks\":[]}");
    } else {
      Serial.println("CONFIG JSON Successfully loaded");
    }
  } else {
    Serial.println("Config file not found, using default config");
    deserializeJson(*cfgPtr, "{\"meta\":{}, \"savedNetworks\":[]}");
  }

  if (!(*cfgPtr)["savedNetworks"] || !(*cfgPtr)["savedNetworks"].is<JsonArray>()) {
    Serial.println("Fixing savedNetworks (was null or invalid)");
    (*cfgPtr)["savedNetworks"] = (*cfgPtr).createNestedArray("savedNetworks");
    save_config_json(); // Optional: Persist fix to disk immediately
  }
}

void handleSerialCommands(){
    if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "eraseconfig") {
      format_config_json();
      Serial.println("Restarting...");
      delay(500);
      ESP.restart();
    }else if (input == "reboot") { 
       ESP.restart(); 
    }
    else if (input == "info") {
       
      //printDeviceInfo(); 
    } else {
      Serial.print("Unknown command: ");
      Serial.println(input);
    }
  }  
}

void handleBootButton() {
  if (digitalRead(BOOT_PIN) == LOW) {
    if (Serial){
      Serial.println("Boot button pressed");
    }
    display_message("BOOT button pressed", "Configure device", 2000);
    enter_config_mode();

    bool connected = wifiConnect((*cfgPtr)["ssid"], (*cfgPtr)["w_pw"], 5000);
    loadConfigWithSavedNetworks();
  }
}

void setupScanRoute() {
  register_server_route("/scan");
}

/*
  Since LastUpdated is READ_WRITE variable, onLastUpdatedChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onLastUpdatedChange()  {
  
}

/*
  Since LastUpdated is READ_WRITE variable, onLastUpdatedChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onLastConnectedIp()  {
  
}


/*
  Since TriggerOtaUpdate is READ_WRITE variable, onTriggerOtaUpdateChange() is
  executed every time a new value is received from IoT Cloud.
*/

void onTriggerOtaUpdateChange()  { 
 if (triggerOtaUpdate) {
    Serial.println("[Cloud OTA] Trigger received from Arduino IoT Cloud Dashboard");

    // Optional: debounce to avoid multiple triggers
    triggerOtaUpdate = false;
    ArduinoCloud.update();

    // Start OTA update
    performHttpOTA("http://192.168.1.136:8083/ConfigPortal32Adv.ino.esp32.bin");
  }
}
  
void register_server_route(char* route){
  webServer.on(route, HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.createNestedArray("networks");
  
    for (int i = 0; i < n; i++) {
      JsonObject net = arr.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
    }
  
    String jsonStr;
    serializeJson(doc, jsonStr);
    webServer.send(200, "application/json", jsonStr);
  });
}

String wifi_scanner_html = R"rawliteral(
  <h3>WiFi Scanner</h3>
  <button type=button onclick="scanNetworks()">Scan WiFi</button>
  <ul id="wifi-list"></ul>

  <script>
    function scanNetworks() {
      fetch('/scan')
        .then(response => response.json())
        .then(data => {
          let list = document.getElementById('wifi-list');
          list.innerHTML = '';
          data.networks.forEach(net => {
            let li = document.createElement('li');
            li.textContent = net.ssid + " (RSSI: " + net.rssi + ")";
            list.appendChild(li);
          });
        })
        .catch(err => {
          alert("Error scanning WiFi: " + err);
        });
    }
  </script>
)rawliteral";
