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
 *  
 *  Modified by Djordje Herceg
 *  15.8.2025.
 *  - Added styles for input field labels and placeholders
 *  - Added the InputField and InputGroup structs to help group settings into WiFi and Other
 *  - Removed user_config_html. Use the InputField instead to add new fields to the Web form. 
 *  - Added pre-loading existing settings into the Config page
 *  - The sketch uses almost 1Mb of flash, you might want to increase the firmware partition size in ESP32
 */

volatile bool configDone = false;

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


#define JSON_BUFFER_LENGTH 3072
#define JSON_CHAR_LENGTH 1024
StaticJsonDocument<JSON_BUFFER_LENGTH> cfg;

WebServer webServer(80);
const int RESET_PIN = 0;

char cfgFile[] = "/config.json";

extern char* ssid_pfix;

String html_begin = ""
                    "<html><head>"
                    "<meta charset='UTF-8'>"
                    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<title>IOT Device Setup</title>"
                    "<style>"
                    "body { font-family:sans-serif; margin:0; padding:1em; text-align:center; }"
                    ".container { width:100%; max-width:500px; margin:auto; }"
                    "button { border:0; border-radius:0.3rem; background-color:#1fa3ec;"
                    "color:#fff; line-height:1.5em; font-size:1.2em; width:90%; margin:1em 0; }"
                    ""
                    ".form-group{position:relative;margin:2em 0;}"
                    ".form-group input{font-size:1.2em;padding:0.8em 0.5em;width:90%;border:1px solid #ccc;border-radius:0.3em;background:none;}"
                    ".form-group label{position:absolute;left:1em;top:0.9em;color:#999;font-size:1.2em;pointer-events:none;transition:0.2s ease all;}"
                    ".form-group input:focus+label,"
                    ".form-group input:not(:placeholder-shown)+label{top:-0.8em;left:1em;font-size:0.9em;color:#1fa3ec;background:#fff;padding:0 0.3em;}"
                    ".form-group input[type=\"color\"]{height:3em;padding:0.8em 0.5em;border:1px solid #ccc;border-radius:0.3em;background:none;appearance:none;-webkit-appearance:none;cursor:pointer;}"
                    "</style>"
                    "</head><body>"
                    "<div class='container'>"
                    "<h1>Device Setup Page</h1>"
                    "<form action='/save'>";
//                    "<p><input type='text' name='ssid' placeholder='SSID'>"
//                    "<p><input type='text' name='w_pw'placeholder='Password'>";



String html_end = ""
                  "<p><button type='submit'>Save</button>"
                  "</form>"
                  "</body></html>";

String postSave_html_default = ""
                               "<html><head><title>Reboot Device</title></head>"
                               "<body><center><h5>Device Configuration Finished</h5><h5>Click the Reboot Button</h5>"
                               "<p>The WiFi connection to the device will be closed.</p>"
                               "<p>Please reconnect to <strong>ESP32</strong> manually if needed.</p>"
                               "<p><button type='button' onclick=\"location.href='/reboot'\">Reboot</button>"
                               "</center></body></html>";

String redirect_html = ""
                       "<html><head><meta http-equiv='refresh' content='0; URL=http:/pre_boot' /></head>"
                       "<body><p>Redirecting</body></html>";

String postSave_html;


/*
  Za zadavanje dodatnih polja u unosu
*/
struct InputField {
  const char* type;
  const char* name;
  const char* placeholder;
  const char* value;  // optional
  bool checked;       // optional
};

struct InputGroup {
  InputField* fields;
  size_t count;
};

// Declare externally assigned input group
extern InputGroup userInputs;


void (*userConfigLoop)() = NULL;

void byte2buff(char* msg, byte* payload, unsigned int len) {
  unsigned int i, j;
  for (i = j = 0; i < len;) {
    msg[j++] = payload[i++];
  }
  msg[j] = '\0';
}

void save_config_json() {
  //saveNewNetwork(webServer.arg("ssid").c_str(), webServer.arg("w_pw").c_str())
  File f = LittleFS.open(cfgFile, "w");
  serializeJson(cfg, f);
  f.close();
}

void reset_config() {
  deserializeJson(cfg, "{meta:{}}");
  save_config_json();
}

void maskConfig(char* buff) {
  JsonDocument temp_cfg;
  temp_cfg.set(cfg);  // copy cfg to temp_cfg
  if (cfg.containsKey("w_pw")) temp_cfg["w_pw"] = "********";
  if (cfg.containsKey("token")) temp_cfg["token"] = "********";
  serializeJson(temp_cfg, buff, JSON_CHAR_LENGTH);
}


void saveNewNetwork(const char* ssid, const char* password) {
  if (!cfg.containsKey("savedNetworks")) {
    cfg["savedNetworks"] = JsonArray();
  }
  JsonArray savedNetworks = cfg["savedNetworks"].as<JsonArray>();

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

IRAM_ATTR void reboot() {
  WiFi.disconnect();
  ESP.restart();
}


void loadConfigWithSavedNetworks() {
    if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  if (LittleFS.exists(cfgFile)) {
    File f = LittleFS.open(cfgFile, "r");
    DeserializationError error = deserializeJson(cfg, f.readString());
    f.close();

    if (error) {
      Serial.println("Deserialization error, using default config");
      deserializeJson(cfg, "{\"meta\":{}, \"savedNetworks\":[]}");
    } else {
      Serial.println("CONFIG JSON Successfully loaded");
    }
  } else {
    Serial.println("Config file not found, using default config");
    deserializeJson(cfg, "{\"meta\":{}, \"savedNetworks\":[]}");
  }

  if (!cfg["savedNetworks"] || !cfg["savedNetworks"].is<JsonArray>()) {
    Serial.println("Fixing savedNetworks (was null or invalid)");
    cfg["savedNetworks"] = cfg.createNestedArray("savedNetworks");
    save_config_json(); // Optional: Persist fix to disk immediately
  }
}


void loadConfig() {
  // check Factory Reset Request and reset if requested or load the config
  if (!LittleFS.begin()) { LittleFS.format(); }  // before the reset_config and reading

  pinMode(RESET_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_PIN) == 0) {
    unsigned long t1 = millis();
    while (digitalRead(RESET_PIN) == 0) {
      delay(500);
      Serial.print(".");
    }
    if (millis() - t1 > 5000) {
      reset_config();  // Factory Reset
    }
  }
  attachInterrupt(RESET_PIN, reboot, FALLING);

  if (LittleFS.exists(cfgFile)) {
    String buff;
    File f = LittleFS.open(cfgFile, "r");
    DeserializationError error = deserializeJson(cfg, f.readString());
    f.close();

    if (error) {
      deserializeJson(cfg, "{meta:{}}");
    } else {
      Serial.println("CONFIG JSON Successfully loaded");
      char maskBuffer[JSON_CHAR_LENGTH];
      maskConfig(maskBuffer);
      Serial.println(String(maskBuffer));
    }
  } else {
    deserializeJson(cfg, "{meta:{}}");
  }
}



bool wifiConnect(const char* ssid, const char* password, uint16_t timeoutMs = 10000) {
  Serial.printf("Attempting to connect to SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < timeoutMs) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Connecting ... ");
    u8g2.sendBuffer();
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected to %s. IP address: %s\n", ssid, WiFi.localIP().toString().c_str());
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Connected to:");
    u8g2.drawStr(0, 30, ssid);
    u8g2.drawStr(0, 50, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    // Update last connected SSID in config'
    cfg["lastConnectedSSID"] = ssid;
    
    saveNewNetwork(webServer.arg("ssid").c_str(), webServer.arg("w_pw").c_str());
    //save_config_json();

    return true;
  } else {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 10, "Failed to connect to :");
      u8g2.drawStr(0, 30, ssid);
      u8g2.sendBuffer();
      Serial.printf("\nFailed to connect to %s within %d ms.\n", ssid, timeoutMs);
      return false;
  }
}

void saveEnv() {
  int args = webServer.args();
  for (int i = 0; i < args; i++) {
    if (webServer.argName(i).indexOf(String("meta.")) == 0) {
      String temp = webServer.arg(i);
      temp.trim();
      cfg["meta"][webServer.argName(i).substring(5)] = temp;
    } else {
      String temp = webServer.arg(i);
      temp.trim();
      cfg[webServer.argName(i)] = temp;
    }
  }

  //if (webServer.hasArg("ssid") && webServer.hasArg("w_pw")) {
  //  saveNewNetwork(webServer.arg("ssid").c_str(), webServer.arg("w_pw").c_str());
  //}

  bool connected = wifiConnect(webServer.arg("ssid").c_str(), webServer.arg("w_pw").c_str());

  if (connected) {
    webServer.send(200, "text/html", redirect_html);
    delay(500);
    configDone = true;
    cfg["config"] = "done";
  }
}

void pre_reboot() {
  int args = webServer.args();
  for (int i = 0; i < args; i++) {
    Serial.printf("%s -> %s\n", webServer.argName(i).c_str(), webServer.arg(i).c_str());
  }
  webServer.send(200, "text/html", postSave_html);
}

bool getHTML(String* html, char* fname) {
  if (LittleFS.exists(fname)) {
    String buff;
    File f = LittleFS.open(fname, "r");
    buff = f.readString();
    buff.trim();
    f.close();
    *html = buff;
    return true;
  } else {
    return false;
  }
}

/**
 * @brief Appends an HTML <input> element to the provided HTML string.
 *
 * Supported input types: "text", "password", "checkbox", "radio", "email", "number", "date".
 * This function dynamically adds an <input> field to the HTML string,
 * allowing customization of the field's name, placeholder text, default value,
 * and optionally the "checked" attribute for checkbox or radio inputs.
 *
 * @param html         Reference to the HTML string to append to.
 * @param type         "text", "password", "checkbox", "radio", "email", "number", "date".
 * @param fieldName    Name attribute of the input field.
 * @param placeholder  Placeholder text shown inside the input field (optional).
 * @param fieldValue   Default value of the input field (optional).
 * @param checked      Whether the input should be marked as checked (for checkbox/radio).
 */
void appendInputField(const char* type, const char* fieldName, const char* placeholder, const char* fieldValue, bool checked = false) {
  bool isTextual = strcmp(type, "checkbox") != 0 && strcmp(type, "radio") != 0;
  bool isCheckbox = strcmp(type, "checkbox");

  if (isTextual) {
    webServer.sendContent("<div class='form-group'>");
    webServer.sendContent("<input type='");
    webServer.sendContent(type);
    webServer.sendContent("' name='");
    webServer.sendContent(fieldName);
    webServer.sendContent("' id='");
    webServer.sendContent(fieldName);
    webServer.sendContent("' placeholder=' '");

    if (fieldValue && strlen(fieldValue) > 0) {
      webServer.sendContent(" value='");
      webServer.sendContent(fieldValue);
      webServer.sendContent("'");
    }

    webServer.sendContent(">");
    if (placeholder && strlen(placeholder) > 0) {
      webServer.sendContent("<label for='");
      webServer.sendContent(fieldName);
      webServer.sendContent("'>");
      webServer.sendContent(placeholder);
      webServer.sendContent("</label>");
    }
    webServer.sendContent("</div>");
  } else {
    // Fallback for checkbox/radio

    webServer.sendContent("<p>");
    // If checkbox, then send a hidden input to provide the unchecked valu
    if (isCheckbox) {
      webServer.sendContent("<input type='hidden' name='");
      webServer.sendContent(fieldName);
      webServer.sendContent("' value='0'>");
    }
    webServer.sendContent("<input type='");
    webServer.sendContent(type);
    webServer.sendContent("' name='");
    webServer.sendContent(fieldName);
    webServer.sendContent("'");

    if (checked) {
      webServer.sendContent(" value='1'");
    }

    webServer.sendContent(">");
    if (placeholder && strlen(placeholder) > 0) {
      webServer.sendContent(" ");
      webServer.sendContent(placeholder);
    }
  }
}




void beginGroup(const char* label = nullptr) {
  webServer.sendContent("<fieldset style='margin-top:1em; padding:0.5em; border:1px solid #ccc;'>");

  if (label && strlen(label) > 0) {
    webServer.sendContent("<legend style='font-weight:bold;'>");
    webServer.sendContent(label);
    webServer.sendContent("</legend>");
  }
}

void endGroup() {
  webServer.sendContent("</fieldset>");
}



void sendConfigPage() {
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");

  webServer.sendContent(html_begin);

  // read the config file (if it exists) and populate the web input controls
  loadConfig();
  bool cfgOK = false;
  // *** If no "config" is found or "config" is not "done", run configDevice ***
  if (cfg.containsKey("config") && (strcmp((const char*)cfg["config"], "done")) == 0) {
    cfgOK = true;
  }

  beginGroup("WiFi config");
  appendInputField("text", "ssid", "SSID", cfgOK ? (const char*)cfg["ssid"] : "wifissid", false);
  appendInputField("text", "w_pw", "Password", cfgOK ? (const char*)cfg["w_pw"] : "wifipwd", false);
  endGroup();

  if (userInputs.fields && userInputs.count > 0) {
    beginGroup("Extra Settings");
    for (size_t i = 0; i < userInputs.count; ++i) {
      InputField& f = userInputs.fields[i];
      appendInputField(f.type, f.name, f.placeholder, cfgOK ? (const char*)cfg[f.name] : f.value, f.checked);
    }
    endGroup();
  }

  webServer.sendContent(html_end);
}



void configDevice() {
  configDone = false;
  DNSServer dnsServer;
  const byte DNS_PORT = 53;
  IPAddress apIP(192, 168, 1, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  char ap_name[100];
  sprintf(ap_name, "%s_%08X", ssid_pfix, (unsigned int)ESP.getEfuseMac());
  WiFi.softAP(ap_name);
  WiFi.setTxPower(WIFI_POWER_2dBm);  // lowest value to prevent overheating
  //WiFi.setTxPower(WIFI_POWER_5dBm);

  dnsServer.start(DNS_PORT, "*", apIP);

  if (getHTML(&postSave_html, (char*)"/postSave.html")) {
    // argument redirection
  } else {
    postSave_html = postSave_html_default;
  }

  webServer.on("/save", saveEnv);
  webServer.on("/reboot", reboot);
  webServer.on("/pre_boot", pre_reboot);

  webServer.onNotFound([]() {
    sendConfigPage();
  });

  webServer.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Config Mode");
  u8g2.sendBuffer();
  Serial.println("starting the config");
  while (!configDone){
    yield();
    dnsServer.processNextRequest();
    webServer.handleClient();
    if (userConfigLoop != NULL) {
      (*userConfigLoop)();
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // RTOS-friendly delay
  }
}
