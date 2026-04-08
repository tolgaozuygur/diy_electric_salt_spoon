#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <math.h> 

extern "C" {
  #include "user_interface.h" 
}

#define DEBUG_MODE 0 

// --- ACCESS POINT CONFIGURATION ---
const char* ap_ssid = "SaltSpoon_Config";
const char* ap_password = "12345678";

// --- HARDWARE PINS ---
const int BUTTON_PIN = D3; 

Adafruit_MCP4725 dac;
static const uint8_t DAC_ADDR = 0x61; 
ESP8266WebServer server(80);

// --- HARDWARE CALIBRATION & EEPROM LIMITS ---
static const uint16_t DAC_MID = 1883;

// Master limits and defaults
static const float DEFAULT_CATHODAL_MA   = 0.50;
static const float MAX_CATHODAL_MA       = 0.57;

static const float DEFAULT_ANODAL_MA     = 0.67;
static const float MAX_ANODAL_MA         = 0.67;

static const uint16_t DEFAULT_REPEATS    = 1;
static const uint16_t MAX_REPEATS        = 20;

static const float DEFAULT_WAVE_SPEED    = 1.0;
static const float MIN_WAVE_SPEED        = 0.5;
static const float MAX_WAVE_SPEED        = 2.0;

static const uint16_t DEFAULT_ANODAL_MULT = 1;
static const uint16_t MIN_ANODAL_MULT     = 1;
static const uint16_t MAX_ANODAL_MULT     = 4;

// EEPROM Memory Addresses (Explicit byte mapping for safety)
const int addr_cathodal    = 0;   // float (4 bytes)
const int addr_anodal      = 4;   // float (4 bytes)
const int addr_repeats     = 8;   // uint16_t (2 bytes)
const int addr_speed       = 10;  // float (4 bytes)
const int addr_anodal_mult = 14;  // uint16_t (2 bytes)

// These will be loaded from EEPROM on boot
float target_cathodal_ma   = DEFAULT_CATHODAL_MA; 
float target_anodal_ma     = DEFAULT_ANODAL_MA;   
uint16_t target_repeats    = DEFAULT_REPEATS;
float wave_speed           = DEFAULT_WAVE_SPEED;
uint16_t anodal_multiplier = DEFAULT_ANODAL_MULT;

uint16_t intensity_cathodal = 0;
uint16_t intensity_anodal = 0;

// Update the step calculation whenever sliders change
void updateIntensities() {
  uint32_t c_steps = target_cathodal_ma * 3263.0;
  intensity_cathodal = constrain(c_steps, 0, 1859); 
  
  uint32_t a_steps = target_anodal_ma * 3263.0;
  intensity_anodal = constrain(a_steps, 0, 2187);   
}

// ── Base Waveform Timing (ms) ────────────────────────────────────────────────
static const uint32_t T_EASE_IN    = 300;  
static const uint32_t T_CATHODAL   = 500;  
static const uint32_t T_TRANSITION = 400;  
static const uint32_t T_ANODAL     = 500;  
static const uint32_t T_EASE_OUT   = 100;
static const uint32_t T_REST       = 100;

static const uint16_t EASE_STEPS   = 30;   
static const uint16_t RAMP_STEPS   = 40;   
// ─────────────────────────────────────────────────────────────────────────────

bool isConfigMode = false;

void setDAC(uint16_t value) {
  dac.setVoltage(constrain(value, 0, 4095), false);
}

// --- HTML WEB PAGE & SERVER LOGIC ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: sans-serif; background-color: #f4f4f9; color: #333; padding: 20px; text-align: center; }";
  html += "h2 { color: #0056b3; }";
  html += ".card { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }";
  html += "input[type=range] { width: 100%; margin: 15px 0; }";
  html += "button { border: none; padding: 15px 20px; font-size: 16px; border-radius: 5px; cursor: pointer; width: 100%; font-weight: bold; margin-bottom: 10px; color: white;}";
  html += ".btn-save { background-color: #28a745; }";
  html += ".btn-save:active { background-color: #218838; }";
  html += ".btn-reset { background-color: #dc3545; }";
  html += ".btn-reset:active { background-color: #c82333; }";
  html += ".value-label { font-weight: bold; color: #d9534f; font-size: 1.2em; }";
  html += "</style></head><body>";
  
  html += "<h2>DIY Electric Salt Spoon</h2>";
  html += "<form action='/save' method='POST'>";
  
  // Cathodal Slider
  html += "<div class='card'>";
  html += "<label>Cathodal Phase (Negative Pull)</label><br>";
  html += "<span>Current: </span><span id='c_val' class='value-label'>" + String(target_cathodal_ma, 2) + "</span><span class='value-label'> mA</span>";
  html += "<input type='range' id='c_slider' name='cathodal' min='0.0' max='" + String(MAX_CATHODAL_MA, 2) + "' step='0.01' value='" + String(target_cathodal_ma, 2) + "' oninput=\"document.getElementById('c_val').innerText = this.value\">";
  html += "<small>Max limit: " + String(MAX_CATHODAL_MA, 2) + " mA</small>";
  html += "</div>";

  // Anodal Slider
  html += "<div class='card'>";
  html += "<label>Anodal Phase (Positive Push)</label><br>";
  html += "<span>Current: </span><span id='a_val' class='value-label'>" + String(target_anodal_ma, 2) + "</span><span class='value-label'> mA</span>";
  html += "<input type='range' id='a_slider' name='anodal' min='0.0' max='" + String(MAX_ANODAL_MA, 2) + "' step='0.01' value='" + String(target_anodal_ma, 2) + "' oninput=\"document.getElementById('a_val').innerText = this.value\">";
  html += "<small>Max limit: " + String(MAX_ANODAL_MA, 2) + " mA</small>";
  html += "</div>";

  // Repeats Slider
  html += "<div class='card'>";
  html += "<label>Waveform Repeats</label><br>";
  html += "<span>Cycles: </span><span id='r_val' class='value-label'>" + String(target_repeats) + "</span><span class='value-label'>x</span>";
  html += "<input type='range' id='r_slider' name='repeats' min='1' max='" + String(MAX_REPEATS) + "' step='1' value='" + String(target_repeats) + "' oninput=\"document.getElementById('r_val').innerText = this.value\">";
  html += "<small>Continuous loops</small>";
  html += "</div>";

  // Speed Slider
  html += "<div class='card'>";
  html += "<label>Global Wave Speed</label><br>";
  html += "<span>Speed: </span><span id='s_val' class='value-label'>" + String(wave_speed, 1) + "</span><span class='value-label'>x</span>";
  html += "<input type='range' id='s_slider' name='speed' min='" + String(MIN_WAVE_SPEED, 1) + "' max='" + String(MAX_WAVE_SPEED, 1) + "' step='0.1' value='" + String(wave_speed, 1) + "' oninput=\"document.getElementById('s_val').innerText = this.value\">";
  html += "<small>1.0x is normal, 2.0x is double speed</small>";
  html += "</div>";

  // Anodal Hold Multiplier Slider
  html += "<div class='card'>";
  html += "<label>Anodal Hold Duration</label><br>";
  html += "<span>Multiplier: </span><span id='am_val' class='value-label'>" + String(anodal_multiplier) + "</span><span class='value-label'>x</span>";
  html += "<input type='range' id='am_slider' name='anodal_mult' min='" + String(MIN_ANODAL_MULT) + "' max='" + String(MAX_ANODAL_MULT) + "' step='1' value='" + String(anodal_multiplier) + "' oninput=\"document.getElementById('am_val').innerText = this.value\">";
  html += "<small>Extends the positive push phase duration (1x to 4x)</small>";
  html += "</div>";

  // Action Buttons
  html += "<button type='submit' name='action' value='save' class='btn-save'>Save to Device Memory</button>";
  html += "<button type='submit' name='action' value='reset' class='btn-reset'>Reset to Defaults</button>";
  html += "</form>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    
    if (action == "reset") {
      // Apply Defaults
      target_cathodal_ma = DEFAULT_CATHODAL_MA;
      target_anodal_ma   = DEFAULT_ANODAL_MA;
      target_repeats     = DEFAULT_REPEATS;
      wave_speed         = DEFAULT_WAVE_SPEED;
      anodal_multiplier  = DEFAULT_ANODAL_MULT;
    } 
    else if (action == "save" && server.hasArg("cathodal") && server.hasArg("anodal") && server.hasArg("repeats") && server.hasArg("speed") && server.hasArg("anodal_mult")) {
      // Apply custom slider values with safety clamps
      target_cathodal_ma = constrain(server.arg("cathodal").toFloat(), 0.0, MAX_CATHODAL_MA);
      target_anodal_ma   = constrain(server.arg("anodal").toFloat(), 0.0, MAX_ANODAL_MA);
      target_repeats     = constrain(server.arg("repeats").toInt(), 1, MAX_REPEATS);
      wave_speed         = constrain(server.arg("speed").toFloat(), MIN_WAVE_SPEED, MAX_WAVE_SPEED);
      anodal_multiplier  = constrain(server.arg("anodal_mult").toInt(), MIN_ANODAL_MULT, MAX_ANODAL_MULT);
    }

    // Write everything to EEPROM
    EEPROM.put(addr_cathodal, target_cathodal_ma);
    EEPROM.put(addr_anodal, target_anodal_ma);
    EEPROM.put(addr_repeats, target_repeats); 
    EEPROM.put(addr_speed, wave_speed);
    EEPROM.put(addr_anodal_mult, anodal_multiplier);
    EEPROM.commit();
    
    // Recalculate DAC steps
    updateIntensities();
    
    // Success feedback
    String message = (action == "reset") ? "Factory Reset Complete!" : "Saved Successfully!";
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><meta http-equiv='refresh' content='1;url=/'></head>";
    html += "<body style='font-family: sans-serif; text-align: center; padding: 50px;'>";
    html += "<h2 style='color: #28a745;'>" + message + "</h2><p>Returning to dashboard...</p></body></html>";
    server.send(200, "text/html", html);
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// --- WAVEFORM EXECUTION ---
void fireWaveform() {
  const uint16_t val_rest     = DAC_MID;
  const uint16_t val_cathodal = DAC_MID - intensity_cathodal;
  const uint16_t val_anodal   = DAC_MID + intensity_anodal;
  
  // Apply global wave_speed multiplier to step timings
  uint32_t ease_step_us  = ((T_EASE_IN * 1000) / EASE_STEPS) / wave_speed;
  uint32_t trans_step_us = ((T_TRANSITION * 1000) / RAMP_STEPS) / wave_speed;

  for (uint16_t i = 0; i <= EASE_STEPS; i++) {
    float t = (float)i / EASE_STEPS; 
    float curve = 1.0 - cos(t * PI / 2.0); 
    uint16_t val = val_rest - (intensity_cathodal * curve);
    setDAC(val);
    delayMicroseconds(ease_step_us);
  }

  setDAC(val_cathodal);
  delay(T_CATHODAL / wave_speed);

  uint16_t total_swing = intensity_cathodal + intensity_anodal;
  for (uint16_t i = 0; i <= RAMP_STEPS; i++) {
    float t = (float)i / RAMP_STEPS;
    uint16_t val = val_cathodal + (total_swing * t); 
    setDAC(val);
    delayMicroseconds(trans_step_us);
  }

  setDAC(val_anodal);
  
  // Apply BOTH the global wave speed and the isolated Anodal Multiplier
  delay((T_ANODAL * anodal_multiplier) / wave_speed);

  for (uint16_t i = 0; i <= EASE_STEPS; i++) {
    float t = (float)i / EASE_STEPS; 
    float curve = cos(t * PI / 2.0); 
    uint16_t val = val_rest + (intensity_anodal * curve);
    setDAC(val);
    delayMicroseconds(ease_step_us); 
  }

  setDAC(val_rest);
  delay(T_REST / wave_speed);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(LED_BUILTIN, LOW); 

  // --- INITIALIZE EEPROM ---
  EEPROM.begin(512); 
  EEPROM.get(addr_cathodal, target_cathodal_ma);
  EEPROM.get(addr_anodal, target_anodal_ma);
  EEPROM.get(addr_repeats, target_repeats);
  EEPROM.get(addr_speed, wave_speed);
  EEPROM.get(addr_anodal_mult, anodal_multiplier);
  
  // Clean sanity check
  if (isnan(target_cathodal_ma) || target_cathodal_ma < 0.0 || target_cathodal_ma > MAX_CATHODAL_MA) {
    target_cathodal_ma = DEFAULT_CATHODAL_MA; 
  }
  if (isnan(target_anodal_ma) || target_anodal_ma < 0.0 || target_anodal_ma > MAX_ANODAL_MA) {
    target_anodal_ma = DEFAULT_ANODAL_MA;
  }
  if (target_repeats < 1 || target_repeats > MAX_REPEATS) {
    target_repeats = DEFAULT_REPEATS;
  }
  if (isnan(wave_speed) || wave_speed < MIN_WAVE_SPEED || wave_speed > MAX_WAVE_SPEED) {
    wave_speed = DEFAULT_WAVE_SPEED;
  }
  if (anodal_multiplier < MIN_ANODAL_MULT || anodal_multiplier > MAX_ANODAL_MULT) {
    anodal_multiplier = DEFAULT_ANODAL_MULT;
  }
  
  updateIntensities();
  
  Serial.println("\n--- BOOTING ---");
  Serial.println("Press button now for Config Mode...");

  // Delayed Boot Timer
  uint32_t bootTimer = millis();
  while (millis() - bootTimer < 3000) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      isConfigMode = true;
      break; 
    }
    delay(10);
  }

  if (isConfigMode) {
    Serial.println("\nConfig Mode Activated.");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password); 
    
    Serial.print("Access Point Started: ");
    Serial.println(ap_ssid);
    Serial.print("Permanent IP Address: ");
    Serial.println(WiFi.softAPIP()); 

    // Route web traffic
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    
  } else {
    Serial.println("\nTaste Mode Activated. Entering Light Sleep.");
    digitalWrite(LED_BUILTIN, HIGH); 

    Wire.begin();  
    Wire.setClock(400000); 
    if (!dac.begin(DAC_ADDR)) {
      while (true) { 
        digitalWrite(LED_BUILTIN, LOW); delay(100);
        digitalWrite(LED_BUILTIN, HIGH); delay(100);
      } 
    }
    setDAC(DAC_MID); 

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1); 

    wifi_set_opmode(NULL_MODE);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
  }
}

void loop() {
  if (isConfigMode) {
    server.handleClient();

    static uint32_t lastBlink = 0;
    static bool ledState = false;
    if (millis() - lastBlink >= 1000) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
    }

  } else {
    gpio_pin_wakeup_enable(GPIO_ID_PIN(0), GPIO_PIN_INTR_LOLEVEL);
    
    wifi_fpm_do_sleep(0xFFFFFFF); 
    delay(10); 
    
    Serial.print("Button Pressed! Firing Waveform ");
    Serial.print(target_repeats);
    Serial.println(" times...");
    
    digitalWrite(LED_BUILTIN, LOW); 

    for (uint16_t r = 0; r < target_repeats; r++) {
      fireWaveform();
      
      if (r < target_repeats - 1) {
        digitalWrite(LED_BUILTIN, HIGH); 
        delay(100);                      
        digitalWrite(LED_BUILTIN, LOW);  
      }
    }

    digitalWrite(LED_BUILTIN, HIGH); 
    Serial.println("Sequence Complete. Going back to sleep...");
    
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10); 
    }
    delay(250); 
  }
}