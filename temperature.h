#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "pin_definitions.h"

// External references
extern OneWire oneWireSump;
extern OneWire oneWireDisplay;
extern DallasTemperature sensorSump;
extern DallasTemperature sensorDisplay;
extern float tempSump;
extern float tempDisplay;
extern bool heaterPrimaryOn;
extern bool heaterBackupOn;
extern bool emergencyStop;

// Forward declarations
void triggerFaultStop();  // NEW
bool overTempFaultActive(); // NEW
void setRelay(uint8_t relay, bool state);
void soundAlarm(int beeps);

// Function declarations
void readTemperatures();
void checkTemperatureDifferential();
void checkEmergencyShutoff();
void controlHeaters();

// ═══════════════════════════════════════════════════════════════
// TEMPERATURE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void readTemperatures() {
  sensorSump.requestTemperatures();
  sensorDisplay.requestTemperatures();
  
  float tempSumpC = sensorSump.getTempCByIndex(0);
  float tempDisplayC = sensorDisplay.getTempCByIndex(0);
  
  tempSump = (tempSumpC * 9.0 / 5.0) + 32.0;
  tempDisplay = (tempDisplayC * 9.0 / 5.0) + 32.0;
  
  if (tempSump < -100 || tempSump > 150) {
    Serial.println("⚠️  WARNING: Sump temp sensor error!");
  }
  if (tempDisplay < -100 || tempDisplay > 150) {
    Serial.println("⚠️  WARNING: Display temp sensor error!");
  }
}

void checkTemperatureDifferential() {
  static unsigned long lastAlert = 0;
  
  float diff = abs(tempSump - tempDisplay);
  
  if (diff > TEMP_DIFFERENTIAL_ALERT) {
    if (millis() - lastAlert > 60000) {
      Serial.println("⚠️  WARNING: Temperature differential!");
      Serial.print("   Sump: ");
      Serial.print(tempSump, 1);
      Serial.print("°F  Display: ");
      Serial.print(tempDisplay, 1);
      Serial.print("°F  Diff: ");
      Serial.print(diff, 1);
      Serial.println("°F");
      soundAlarm(2);
      lastAlert = millis();
    }
  }
}

bool overTempFaultActive() {
  return (tempSump >= TEMP_EMERGENCY_HIGH) || (tempDisplay >= TEMP_EMERGENCY_HIGH);
}

void checkEmergencyShutoff() {
  if (overTempFaultActive()) {
    if (!emergencyStop) {
      Serial.println("🚨 EMERGENCY - Temperature too high!");
      Serial.print("   Sump: ");
      Serial.print(tempSump, 1);
      Serial.print("°F  Display: ");
      Serial.print(tempDisplay, 1);
      Serial.println("°F");
      triggerFaultStop();
    }
  }
}

void controlHeaters() {
  if (emergencyStop) {
    if (heaterPrimaryOn) {
      setRelay(RELAY_HEATER_PRIMARY, false);
      heaterPrimaryOn = false;
    }
    if (heaterBackupOn) {
      setRelay(RELAY_HEATER_BACKUP, false);
      heaterBackupOn = false;
    }
    return;
  }
  
  float controlTemp = tempSump;
  
  if (controlTemp < (TARGET_TEMP - TEMP_HYSTERESIS)) {
    if (!heaterPrimaryOn) {
      setRelay(RELAY_HEATER_PRIMARY, true);
      heaterPrimaryOn = true;
      Serial.print("✓ Primary heater ON (");
      Serial.print(controlTemp, 1);
      Serial.println("°F)");
    }
  } else if (controlTemp > (TARGET_TEMP + TEMP_HYSTERESIS)) {
    if (heaterPrimaryOn) {
      setRelay(RELAY_HEATER_PRIMARY, false);
      heaterPrimaryOn = false;
      Serial.print("✓ Primary heater OFF (");
      Serial.print(controlTemp, 1);
      Serial.println("°F)");
    }
  }
  
  if (heaterBackupOn) {
    setRelay(RELAY_HEATER_BACKUP, false);
    heaterBackupOn = false;
  }
}

#endif