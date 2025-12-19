#ifndef ALARMS_H
#define ALARMS_H

#include "pin_definitions.h"

// External references
extern bool alarmSilenced;
extern bool emergencyStop;

// ═══════════════════════════════════════════════════════════════
// ALARM FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void soundAlarm(int beeps) {
  if (alarmSilenced) return;
  
  for (int i = 0; i < beeps; i++) {
    tone(BUZZER_PIN, 2000, 200);
    delay(300);
  }
}

void silenceAlarm() {
  alarmSilenced = true;
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("🔇 Alarm silenced");
  tone(BUZZER_PIN, 1500, 50);
}

#endif