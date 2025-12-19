#ifndef BUTTONS_H
#define BUTTONS_H

#include <PCF8574.h>
#include "pin_definitions.h"

// External references
extern PCF8574 buttonBox;

// Debounced button class
class DebouncedButton {
private:
  PCF8574* expander;
  uint8_t pin;
  bool lastReading;
  bool currentState;
  unsigned long lastChangeTime;
  const unsigned long debounceDelay = 50;

public:
  DebouncedButton(PCF8574* exp, uint8_t p) : expander(exp), pin(p) {
    lastReading = HIGH;
    currentState = HIGH;
    lastChangeTime = 0;
  }

  void begin() {
    expander->pinMode(pin, INPUT_PULLUP);
  }

  bool read() {
    bool reading = expander->digitalRead(pin);

    if (reading != lastReading) {
      lastChangeTime = millis();
    }

    if (millis() - lastChangeTime > debounceDelay) {
      if (reading != currentState) {
        currentState = reading;
      }
    }

    lastReading = reading;
    return currentState;
  }
};

// External button instances
extern DebouncedButton btnYellow;
extern DebouncedButton btnBlue;
extern DebouncedButton btnGreen;

// External references
extern bool emergencyStop;
extern bool feedModeActive;
extern bool photoModeActive;
extern bool atoTimeoutAlarm;
extern bool atoReservoirAlarm;
extern LightMode currentLightMode;
extern bool scheduledLightsEnabled;

// Forward declarations (everything we call from this header)
void handleButtons();

void toggleFeedMode();        // main
void togglePhotoMode();       // main
void triggerEmergencyStop();  // main
void resetEmergencyStop();    // main

void silenceAlarm();          // alarms.h
void resetATOAlarm();         // ato.h

void triggerManualCloud();    // lighting/clouds
void lightsFullBright();      // lighting
void setNightMode();          // lighting

void toggleLightsManual();
void toggleLightSchedule();

// Your combo handler implemented in main
void handleBlueRedCombo();

// ═══════════════════════════════════════════════════════════════
// BUTTON HANDLING
// ═══════════════════════════════════════════════════════════════

void handleButtons() {
  // E-STOP (Direct GPIO) - trigger IMMEDIATELY on press
  static bool estopLast = HIGH;
  bool estop = digitalRead(ESTOP_BUTTON_PIN);

  // Track blue hold-to-arm
  static bool btnBlueLast = HIGH;
  static unsigned long btnBluePressTime = 0;
  static bool resetArmed = false;

  // Read expander buttons early (blue needed for combo gating)
  bool yellow = btnYellow.read();
  bool blue   = btnBlue.read();
  bool green  = btnGreen.read();

  // RED pressed (falling edge)
  if (estopLast == HIGH && estop == LOW) {
    bool blueHeld = (blue == LOW);
    if (resetArmed && blueHeld) {
      handleBlueRedCombo();   // override/reset action
      resetArmed = false;     // prevent repeated combo triggers while still holding blue
    } else {
      triggerEmergencyStop(); // instant manual stop
    }
  }
  estopLast = estop;

  // YELLOW - Feed mode
  static bool btnYellowLast = HIGH;
  if (yellow == LOW && btnYellowLast == HIGH) {
    toggleFeedMode();
  }
  btnYellowLast = yellow;

  // BLUE - hold to arm reset/override; short press does normal actions
  if (blue == LOW && btnBlueLast == HIGH) {
    btnBluePressTime = millis();
    resetArmed = false;
  }

  if (blue == LOW && btnBlueLast == LOW) {
    if (!resetArmed && (millis() - btnBluePressTime >= 2000)) { // 2s to arm
      resetArmed = true;
      tone(BUZZER_PIN, 1800, 80); // optional feedback
    }
  }

  if (blue == HIGH && btnBlueLast == LOW) {
    unsigned long pressDuration = millis() - btnBluePressTime;

    if (pressDuration < 2000) {
      // Short press behavior
      if (atoTimeoutAlarm || atoReservoirAlarm) {
        resetATOAlarm();
      } else if (currentLightMode == FULL_DAYLIGHT) {
        triggerManualCloud();
      } else {
        toggleLightsManual();
      }
    } else {
      // Long press release: silence/ack
      silenceAlarm();
    }

    resetArmed = false;
  }
  btnBlueLast = blue;

  // GREEN - Photo mode / Schedule
  static bool btnGreenLast = HIGH;
  static unsigned long btnGreenPressTime = 0;

  if (green == LOW && btnGreenLast == HIGH) {
    btnGreenPressTime = millis();
  }
  if (green == HIGH && btnGreenLast == LOW) {
    unsigned long pressDuration = millis() - btnGreenPressTime;
    if (pressDuration < 3000) {
      togglePhotoMode();
    } else {
      toggleLightSchedule();
    }
  }
  btnGreenLast = green;
}

void toggleLightsManual() {
  if (currentLightMode == NIGHT_MODE) {
    lightsFullBright();
  } else {
    setNightMode();
  }
}

void toggleLightSchedule() {
  scheduledLightsEnabled = !scheduledLightsEnabled;
  Serial.print("⏰ Light schedule: ");
  Serial.println(scheduledLightsEnabled ? "ENABLED" : "DISABLED");
  tone(BUZZER_PIN, scheduledLightsEnabled ? 2500 : 1500, 200);
}

#endif
