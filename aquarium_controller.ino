/*
 * ═══════════════════════════════════════════════════════════════
 *  AQUARIUM CONTROLLER v3.0 - MODULAR VERSION
 * ═══════════════════════════════════════════════════════════════
 */

#include <Wire.h>
#include <PCF8574.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <IRremote.hpp>
#include <WiFi.h>

const char* WIFI_SSID = "Marian's G-Fiber";
const char* WIFI_PASS = "8012507321";

// Include all module headers
#include "config.h"
#include "pin_definitions.h"
#include "relays.h"
#include "alarms.h"
#include "temperature.h"
#include "ato.h"
#include "lighting.h"
#include "buttons.h"
#include "display.h"

// ═══════════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════════

PCF8574 buttonBox(BUTTON_BOX_ADDR);
PCF8574 relayBox(RELAY_BOX_ADDR);

OneWire oneWireSump(TEMP_SUMP_PIN);
OneWire oneWireDisplay(TEMP_DISPLAY_PIN);
DallasTemperature sensorSump(&oneWireSump);
DallasTemperature sensorDisplay(&oneWireDisplay);

RTC_DS3231 rtc;

// Button instances
DebouncedButton btnYellow(&buttonBox, BTN_YELLOW);
DebouncedButton btnBlue(&buttonBox, BTN_BLUE);
DebouncedButton btnGreen(&buttonBox, BTN_GREEN);

// ═══════════════════════════════════════════════════════════════
// GLOBAL STATE VARIABLES
// ═══════════════════════════════════════════════════════════════

// Temperature
float tempSump = 0;
float tempDisplay = 0;
bool heaterPrimaryOn = false;
bool heaterBackupOn = false;

// System modes
bool emergencyStop = false;
bool feedModeActive = false;
bool photoModeActive = false;
bool alarmSilenced = false;
bool manualEstopLatched = false;
bool developerModeEnabled = false;

// ATO
unsigned long atoStartTime = 0;
unsigned long atoLastRunTime = 0;
bool atoRunning = false;
bool atoTimeoutAlarm = false;
bool atoReservoirAlarm = false;

// Feed mode
unsigned long feedModeStartTime = 0;

// Relays and LEDs
uint8_t relayStates = 0xFF;

// Lighting
LightMode currentLightMode = NIGHT_MODE;
bool scheduledLightsEnabled = true;
bool lightsOn = false;
unsigned long rampStartTime = 0;
int currentRampStep = 0;

// Clouds
CloudState cloudState = NO_CLOUD;
unsigned long nextCloudTime = 0;
unsigned long cloudStartTime = 0;
unsigned long cloudDuration = 0;
int cloudDimSteps = 0;
int cloudBrightenSteps = 0;
unsigned long lastCloudStepTime = 0;

// ─────────────────────────────────────────────
// Safety / Fault model
// ─────────────────────────────────────────────

bool hasActiveCriticalFaults() {
  // Critical faults that should block manual override (unless dev mode)
  bool overTemp = (tempSump >= TEMP_EMERGENCY_HIGH) || (tempDisplay >= TEMP_EMERGENCY_HIGH);
  return overTemp || atoTimeoutAlarm || atoReservoirAlarm;
}

// Gyre wiring choice: set true if outlet is wired NC (recommended)
constexpr bool GYRE_WIRED_NC = CFG_GYRE_WIRED_NC;

// Intent-based gyre control (so you don't invert logic everywhere)
void setGyreRunning(bool run) {
  if (GYRE_WIRED_NC) {
    setRelay(RELAY_GYRE, !run);   // relay ON cuts power, relay OFF allows power through NC
  } else {
    setRelay(RELAY_GYRE, run);    // relay ON provides power via NO
  }
}

// One place that actually performs the "stop outputs" behavior.
// NOTE: This does NOT set manualEstopLatched; that's done by manual stop only.
void enterEstopState() {
  if (emergencyStop) return;

  emergencyStop = true;
  alarmSilenced = false; // new stop re-arms audible alarm

  Serial.println("\n🔴 ═══════════════════════════════");
  Serial.println("   EMERGENCY STOP ACTIVATED");
  Serial.println("   ═══════════════════════════════");

  setRelay(RELAY_RETURN_PUMP, false);
  setRelay(RELAY_HEATER_PRIMARY, false);
  setRelay(RELAY_HEATER_BACKUP, false);
  setRelay(RELAY_ATO_PUMP, false);

  setNightMode();

  Serial.println("   Return pump: OFF");
  Serial.println("   Heaters: OFF");
  Serial.println("   ATO: OFF");
  Serial.println("   Lights: NIGHT MODE");

  // Keep circulation running during stop
  setGyreRunning(true);
  Serial.println("   Gyre: ON (circulation)");
  Serial.println("   (Blue-hold + Red tap to override/reset)");
  Serial.println("   ═══════════════════════════════\n");

  soundAlarm(5);
}

// Manual stop entry point (red button)
void triggerEmergencyStop() {
  manualEstopLatched = true;
  enterEstopState();
}

// Fault-driven stop entry point (overtemp, etc.)
void triggerFaultStop() {
  // DO NOT set manualEstopLatched here
  enterEstopState();
}

// Leaves stop state and restores a known-good RUN baseline
void exitEstopToRunDefaults() {
  emergencyStop = false;
  alarmSilenced = false;

  // Ensure modes don't keep things off
  feedModeActive = false;
  photoModeActive = false;

  setRelay(RELAY_RETURN_PUMP, true);
  setGyreRunning(true);

  Serial.println("✓ System resumed (RUN defaults)");
  tone(BUZZER_PIN, 2500, 120);
  delay(140);
  tone(BUZZER_PIN, 3000, 120);
}

// This is used by combo logic (fault-gated override)
bool attemptOverrideEstop() {
  if (hasActiveCriticalFaults() && !developerModeEnabled) {
    Serial.println("🚫 Override blocked: active fault present");
    tone(BUZZER_PIN, 400, 120); delay(140); tone(BUZZER_PIN, 400, 120);
    return false;
  }

  // Clear ONLY the manual latch
  manualEstopLatched = false;

  // If faults are clear (or dev mode), exit stop state
  exitEstopToRunDefaults();
  return true;
}

// Keeps your old name, but now it restores deterministically
void resetEmergencyStop() {
  if (!emergencyStop) return;
  exitEstopToRunDefaults();
}

void toggleFeedMode() {
  if (feedModeActive) {
    feedModeActive = false;
    setRelay(RELAY_RETURN_PUMP, true);
    setGyreRunning(true);
    Serial.println("🐟 Feed mode OFF (manual)");
  } else {
    feedModeActive = true;
    feedModeStartTime = millis();
    setRelay(RELAY_RETURN_PUMP, false);
    setGyreRunning(false);
    Serial.println("🐟 Feed mode ON (10 minutes)");
    tone(BUZZER_PIN, 1500, 100);
  }
}

void handleBlueRedCombo() {
  if (emergencyStop) {
    attemptOverrideEstop();
  } else {
    Serial.println("🔧 Reset combo pressed (normal mode) — no action configured");
  }
}

void handleFeedMode() {
  if (feedModeActive) {
    if (millis() - feedModeStartTime >= FEED_MODE_DURATION) {
      feedModeActive = false;
      setRelay(RELAY_RETURN_PUMP, true);
      setGyreRunning(true);
      Serial.println("🐟 Feed mode ended (timeout)");
      tone(BUZZER_PIN, 2000, 100);
      delay(150);
      tone(BUZZER_PIN, 2000, 100);
    }
  }
}

void togglePhotoMode() {
  photoModeActive = !photoModeActive;
  
  if (photoModeActive) {
    lightsPhotoMode();
  } else {
    lightsNormalMode();
  }
}

// ═══════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔═══════════════════════════════════════╗");
  Serial.println("║   AQUARIUM CONTROLLER v3.0            ║");
  Serial.println("║   Modular Version                     ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
  delay(500);
  Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected");
  } else {
    Serial.println("⚠️ WiFi failed, continuing offline");
  }

#include <time.h>

static const char* TZ = "MST7MDT,M3.2.0/2,M11.1.0/2"; // Utah, auto DST

void syncTimeFromNTP() {
  configTzTime(TZ, "pool.ntp.org", "time.nist.gov");

  struct tm t;
  if (getLocalTime(&t, 5000)) {
    rtc.adjust(DateTime(
      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
      t.tm_hour, t.tm_min, t.tm_sec
    ));
    Serial.println("🕒 Time synced from NTP");
  } else {
    Serial.println("⚠️ NTP failed");
  }
}
  static bool booted = false;
if (booted) return;
booted = true;

  // Initialize I2C
  Wire.begin(21, 22);
  Serial.println("→ Initializing I2C bus...");
  
  // Initialize expanders
  buttonBox.begin();
  relayBox.begin();
  Serial.println("✓ PCF8574 expanders initialized");
  
  // Initialize IR
  IrSender.begin(IR_SEND_PIN);
  Serial.println("✓ IR transmitter initialized");
  
  // Scan I2C
  Serial.println("\n→ Scanning I2C devices:");
  int deviceCount = 0;
  for (byte addr = 0x20; addr <= 0x70; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  ✓ Found device at: 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      deviceCount++;
    }
  }
  Serial.print("  Total devices found: ");
  Serial.println(deviceCount);
  
  // Initialize buttons
  btnYellow.begin();
  btnBlue.begin();
  btnGreen.begin();
  pinMode(ESTOP_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("✓ Buttons initialized");
  
  // Initialize LEDs
  buttonBox.pinMode(LED_RED, OUTPUT);
  buttonBox.pinMode(LED_YELLOW, OUTPUT);
  buttonBox.pinMode(LED_BLUE, OUTPUT);
  buttonBox.pinMode(LED_GREEN, OUTPUT);
  buttonBox.digitalWrite(LED_RED, LOW);
  buttonBox.digitalWrite(LED_YELLOW, LOW);
  buttonBox.digitalWrite(LED_BLUE, LOW);
  buttonBox.digitalWrite(LED_GREEN, LOW);
  Serial.println("✓ LEDs initialized");
  
  // Initialize relays
  for (int i = 0; i < 8; i++) {
    relayBox.pinMode(i, OUTPUT);
  }
 // Initialize all relays OFF
for (int i = 0; i < 8; i++) {
  relayBox.digitalWrite(i, HIGH);  // Active-LOW = OFF
}
  Serial.println("✓ Relays initialized");
  
  // Initialize sensors
  sensorSump.begin();
  sensorDisplay.begin();
  Serial.println("✓ Temperature sensors initialized");
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("✗ ERROR: RTC not found!");
  } else {
    Serial.println("✓ RTC initialized");
    if (rtc.lostPower()) {
      Serial.println("  → Setting time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime now = rtc.now();
    Serial.print("  Current time: ");
    if (now.hour() < 10) Serial.print("0");
    Serial.print(now.hour());
    Serial.print(":");
    if (now.minute() < 10) Serial.print("0");
    Serial.print(now.minute());
    Serial.print(":");
    if (now.second() < 10) Serial.print("0");
    Serial.println(now.second());
  }
  
  // Initialize other GPIO
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_RECV_PIN, INPUT);
  pinMode(ATO_FLOAT_LOW, INPUT_PULLUP);
  pinMode(ATO_FLOAT_HIGH, INPUT_PULLUP);
  pinMode(ATO_RESERVOIR_EMPTY, INPUT_PULLUP);
  pinMode(PH_PROBE_PIN, INPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✓ GPIO pins configured");
  
  // Set initial light mode
  Serial.println("\n→ Setting initial light mode...");
  setInitialLightingFromTime()
  
  // RUN defaults after boot (so we don't come up "all off")
  emergencyStop = false;
  manualEstopLatched = false;
  alarmSilenced = false;
  feedModeActive = false;
  photoModeActive = false;

  setRelay(RELAY_RETURN_PUMP, true);
  setGyreRunning(true);

  // Initialize cloud timing
  nextCloudTime = millis() + random(CLOUD_MIN_INTERVAL, CLOUD_MAX_INTERVAL);
  
  Serial.println("\n=== SYSTEM CONFIG ===");
Serial.printf("Gyre wired NC: %s\n", CFG_GYRE_WIRED_NC ? "YES" : "NO");
Serial.printf("ATO floats active-low: %s\n", CFG_ATO_FLOAT_ACTIVE_LOW ? "YES" : "NO");
Serial.printf("Schedule enabled: %s\n", scheduledLightsEnabled ? "YES" : "NO");
Serial.println("=====================\n");

  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║   INITIALIZATION COMPLETE             ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  // Startup melody
  tone(BUZZER_PIN, 2000, 100);
  delay(150);
  tone(BUZZER_PIN, 2500, 100);
  delay(150);
  tone(BUZZER_PIN, 3000, 150);
  
  delay(2000);
}

// ═══════════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════════

void loop() {
  handleButtons();

 static uint32_t last500 = 0;
  uint32_t now = millis();

  if (now - last500 >= 500) {
    last500 = now;

    readTemperatures();
    checkTemperatureDifferential();
    checkEmergencyShutoff();
    controlHeaters();
    handleFeedMode();
    handleATO();
    handleLightingSchedule();
    handleClouds();
    updateLEDs();
    printStatus();
  }
}
