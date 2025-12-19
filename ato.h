#ifndef ATO_H
#define ATO_H

#include "config.h"
#include "pin_definitions.h"

#ifndef CFG_ATO_FLOAT_ACTIVE_LOW
#define CFG_ATO_FLOAT_ACTIVE_LOW true
#endif

// External references
extern unsigned long atoStartTime;
extern unsigned long atoLastRunTime;
extern bool atoRunning;
extern bool atoTimeoutAlarm;
extern bool atoReservoirAlarm;

// Function declarations
void handleATO();
void resetATOAlarm();

// ═══════════════════════════════════════════════════════════════
// ATO FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void handleATO() {
  bool lowTriggered  = (digitalRead(ATO_FLOAT_LOW)  == (CFG_ATO_FLOAT_ACTIVE_LOW ? LOW : HIGH));
  bool highTriggered = (digitalRead(ATO_FLOAT_HIGH) == (CFG_ATO_FLOAT_ACTIVE_LOW ? LOW : HIGH));
  bool resEmpty      = (digitalRead(ATO_RESERVOIR_EMPTY) == (CFG_ATO_FLOAT_ACTIVE_LOW ? LOW : HIGH));

  if (resEmpty) {
    if (!atoReservoirAlarm) {
      Serial.println("🚨 ATO RESERVOIR EMPTY!");
      Serial.println("   Please refill ATO reservoir");
      soundAlarm(3);
      atoReservoirAlarm = true;
    }
    if (atoRunning) {
      setRelay(RELAY_ATO_PUMP, false);
      atoRunning = false;
      Serial.println("   ATO pump stopped (reservoir empty)");
    }
    return;
  } else {
    if (atoReservoirAlarm) {
      Serial.println("✓ ATO reservoir refilled");
      atoReservoirAlarm = false;
      // Optional: allow immediate top-off after refill
      atoLastRunTime = 0;
    }
  }

  if (millis() - atoLastRunTime < ATO_COOLDOWN && !atoRunning) return;

  if (atoTimeoutAlarm) {
    if (atoRunning) {
      setRelay(RELAY_ATO_PUMP, false);
      atoRunning = false;
    }
    return;
  }

  if (lowTriggered) {
    if (!atoRunning) {
      setRelay(RELAY_ATO_PUMP, true);
      atoRunning = true;
      atoStartTime = millis();
      Serial.println("💧 ATO pump ON");
    } else {
      unsigned long runtime = millis() - atoStartTime;
      if (runtime > ATO_TIMEOUT) {
        setRelay(RELAY_ATO_PUMP, false);
        atoRunning = false;
        atoTimeoutAlarm = true;
        Serial.println("🚨 ATO TIMEOUT ALARM!");
        Serial.print("   Pump ran for ");
        Serial.print(runtime / 1000);
        Serial.println(" seconds");
        soundAlarm(5);
      }
    }
  } else if (highTriggered) {
    if (atoRunning) {
      unsigned long runtime = millis() - atoStartTime;
      if (runtime >= ATO_MIN_RUNTIME) {
        setRelay(RELAY_ATO_PUMP, false);
        atoRunning = false;
        atoLastRunTime = millis();
        Serial.print("💧 ATO pump OFF (ran ");
        Serial.print(runtime / 1000);
        Serial.println(" sec)");
      }
    }
  }
}

void resetATOAlarm() {
  if (atoTimeoutAlarm || atoReservoirAlarm) {
    atoTimeoutAlarm = false;
    atoReservoirAlarm = false;
    atoStartTime = 0;
    atoLastRunTime = millis();
    Serial.println("✓ ATO alarms reset");
    tone(BUZZER_PIN, 2000, 100);
  }
}

#endif