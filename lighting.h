#ifndef LIGHTING_H
#define LIGHTING_H

#include <IRremote.hpp>
#include <RTClib.h>
#include "config.h"

// Light modes
enum LightMode {
  NIGHT_MODE,
  SUNRISE_RAMPING,
  FULL_DAYLIGHT,
  SUNSET_RAMPING
};

// Cloud states
enum CloudState {
  NO_CLOUD,
  CLOUD_DIMMING,
  CLOUD_HOLDING,
  CLOUD_BRIGHTENING
};

// External references
extern LightMode currentLightMode;
extern CloudState cloudState;
extern bool scheduledLightsEnabled;
extern bool lightsOn;
extern unsigned long rampStartTime;
extern int currentRampStep;
extern unsigned long nextCloudTime;
extern unsigned long cloudStartTime;
extern unsigned long cloudDuration;
extern int cloudDimSteps;
extern int cloudBrightenSteps;
extern unsigned long lastCloudStepTime;
extern RTC_DS3231 rtc;

// Function declarations
void sendIRCommand(uint8_t command, int repeats = 0);
void setLightPower(bool on);
void setLightMode(uint8_t mode);
void adjustChannel(uint8_t channel, int steps);
void setNightMode();
void lightsFullBright();
void lightsPhotoMode();
void lightsNormalMode();
void handleLightingSchedule();
void startSunrise();
void updateSunrise();
void completeSunrise();
void startSunset();
void updateSunset();
void completeSunset();
void handleClouds();
void startCloud();
void startCloudBrighten();
void updateCloudBrighten();
void triggerManualCloud();
void setInitialLightingFromTime()

// ═══════════════════════════════════════════════════════════════
// IR FUNCTIONS
// ═══════════════════════════════════════════════════════════════

void sendIRCommand(uint8_t command, int repeats) {
  IrSender.sendNEC(IR_ADDRESS, command, repeats);
  delay(100);
}

void setLightPower(bool on) {
  sendIRCommand(CMD_POWER);
  lightsOn = on;
  delay(500);
}

void setLightMode(uint8_t mode) {
  sendIRCommand(mode);
}

void adjustChannel(uint8_t channel, int steps) {
  uint8_t upCmd, downCmd;
  
  switch(channel) {
    case 1: upCmd = CMD_CH1_UP; downCmd = CMD_CH1_DOWN; break;
    case 2: upCmd = CMD_CH2_UP; downCmd = CMD_CH2_DOWN; break;
    case 3: upCmd = CMD_CH3_UP; downCmd = CMD_CH3_DOWN; break;
    case 4: upCmd = CMD_CH4_UP; downCmd = CMD_CH4_DOWN; break;
    default: return;
  }
  
  uint8_t cmd = (steps > 0) ? upCmd : downCmd;
  int absSteps = abs(steps);
  
  for (int i = 0; i < absSteps; i++) {
    sendIRCommand(cmd);
    delay(150);
  }
}

void setNightMode() {
  if (!lightsOn) {
    setLightPower(true);
  }
  setLightMode(CMD_NIGHT);
  Serial.println("🌙 Night mode set (dim blues)");
}

void lightsFullBright() {
  if (!lightsOn) {
    setLightPower(true);
    delay(500);
  }
  setLightMode(CMD_FULL_BRIGHT);
}

void lightsPhotoMode() {
  Serial.println("📸 Photo mode: Dimming blues, maxing whites");
  
  if (!lightsOn) {
    setLightPower(true);
    delay(500);
  }
  
  setLightMode(CMD_FULL_BRIGHT);
  delay(1000);
  adjustChannel(3, -10);
}

void lightsNormalMode() {
  Serial.println("📸 Returning to normal lighting");
  setLightMode(CMD_FULL_BRIGHT);
}

// ═══════════════════════════════════════════════════════════════
// SCHEDULING & RAMPING
// ═══════════════════════════════════════════════════════════════

void setInitialLightingFromTime() {
  DateTime now = rtc.now();
  int nowMinutes = now.hour() * 60 + now.minute();
  int sunriseStart = SUNRISE_START_HOUR * 60 + SUNRISE_START_MINUTE;
  int sunriseEnd   = SUNRISE_END_HOUR * 60 + SUNRISE_END_MINUTE;
  int sunsetStart  = SUNSET_START_HOUR * 60 + SUNSET_START_MINUTE;
  int sunsetEnd    = SUNSET_END_HOUR * 60 + SUNSET_END_MINUTE;

  if (nowMinutes >= sunriseEnd && nowMinutes < sunsetStart) {
    lightsFullBright();
    currentLightMode = FULL_DAYLIGHT;
    Serial.println("☀️  Boot lighting: DAY mode");
  }
  else if (nowMinutes >= sunriseStart && nowMinutes < sunriseEnd) {
    startSunrise();
    Serial.println("🌅 Boot lighting: SUNRISE ramp");
  }
  else if (nowMinutes >= sunsetStart && nowMinutes < sunsetEnd) {
    startSunset();
    Serial.println("🌆 Boot lighting: SUNSET ramp");
  }
  else {
    setNightMode();
    currentLightMode = NIGHT_MODE;
    Serial.println("🌙 Boot lighting: NIGHT mode");
  }
}


void handleLightingSchedule() {
static int lastDay = -1;
static bool sunriseStartedToday = false;
static bool sunsetStartedToday  = false;

DateTime now = rtc.now();
if (now.day() != lastDay) {
  lastDay = now.day();
  sunriseStartedToday = false;
  sunsetStartedToday = false;
}

  if (!scheduledLightsEnabled) return;
  
  DateTime now = rtc.now();
  int nowMinutes = now.hour() * 60 + now.minute();
  int sunriseStart = SUNRISE_START_HOUR * 60 + SUNRISE_START_MINUTE;
  int sunriseEnd = SUNRISE_END_HOUR * 60 + SUNRISE_END_MINUTE;
  int sunsetStart = SUNSET_START_HOUR * 60 + SUNSET_START_MINUTE;
  int sunsetEnd = SUNSET_END_HOUR * 60 + SUNSET_END_MINUTE;
  
  switch (currentLightMode) {
    case NIGHT_MODE:
    if (!sunriseStartedToday && nowMinutes >= sunriseStart) {
       sunriseStartedToday = true;
       startSunrise();
      }
      break;
      
    case SUNRISE_RAMPING:
      updateSunrise();
      if (nowMinutes >= sunriseEnd) {
        completeSunrise();
      }
      break;
      
    case FULL_DAYLIGHT:
      if (!sunsetStartedToday && nowMinutes >= sunsetStart) {
        sunsetStartedToday = true;
        startSunset();
      }
      break;
      
    case SUNSET_RAMPING:
      updateSunset();
      if (nowMinutes >= sunsetEnd) {
        completeSunset();
      }
      break;
  }
}

void startSunrise() {
  Serial.println("\n🌅 SUNRISE STARTING (30 min ramp)");
  currentLightMode = SUNRISE_RAMPING;
  rampStartTime = millis();
  currentRampStep = 0;
}

void updateSunrise() {
  unsigned long elapsed = millis() - rampStartTime;
  int targetStep = elapsed / STEP_INTERVAL;
  
  if (targetStep > currentRampStep && targetStep <= RAMP_STEPS) {
    currentRampStep = targetStep;
    
    Serial.print("🌅 Sunrise step ");
    Serial.print(currentRampStep);
    Serial.print(" / ");
    Serial.println(RAMP_STEPS);
    
    adjustChannel(3, 1);
    delay(200);
    adjustChannel(1, 1);
  }
}

void completeSunrise() {
  Serial.println("☀️  SUNRISE COMPLETE - Full daylight");
  lightsFullBright();
  currentLightMode = FULL_DAYLIGHT;
  nextCloudTime = millis() + random(CLOUD_MIN_INTERVAL, CLOUD_MAX_INTERVAL);
}

void startSunset() {
  Serial.println("\n🌆 SUNSET STARTING (30 min ramp)");
  currentLightMode = SUNSET_RAMPING;
  rampStartTime = millis();
  currentRampStep = 0;
}

void updateSunset() {
  unsigned long elapsed = millis() - rampStartTime;
  int targetStep = elapsed / STEP_INTERVAL;
  
  if (targetStep > currentRampStep && targetStep <= RAMP_STEPS) {
    currentRampStep = targetStep;
    
    Serial.print("🌆 Sunset step ");
    Serial.print(currentRampStep);
    Serial.print(" / ");
    Serial.println(RAMP_STEPS);
    
    adjustChannel(1, -1);
    delay(200);
    adjustChannel(3, -1);
  }
}

void completeSunset() {
  Serial.println("🌙 SUNSET COMPLETE - Night mode");
  setNightMode();
  currentLightMode = NIGHT_MODE;
}

// ═══════════════════════════════════════════════════════════════
// CLOUD SIMULATION
// ═══════════════════════════════════════════════════════════════

void handleClouds() {
  if (currentLightMode != FULL_DAYLIGHT) {
    cloudState = NO_CLOUD;
    return;
  }
  
  unsigned long now = millis();
  
  switch (cloudState) {
    case NO_CLOUD:
      if (now >= nextCloudTime) {
        startCloud();
      }
      break;
      
    case CLOUD_DIMMING:
      cloudState = CLOUD_HOLDING;
      cloudStartTime = now;
      break;
      
    case CLOUD_HOLDING:
      if (now - cloudStartTime >= cloudDuration) {
        startCloudBrighten();
      }
      break;
      
    case CLOUD_BRIGHTENING:
      updateCloudBrighten();
      break;
  }
}

void startCloud() {
  Serial.println("☁️  Cloud passing...");
  
  cloudDimSteps = random(CLOUD_MIN_DIM_STEPS, CLOUD_MAX_DIM_STEPS + 1);
  cloudDuration = random(CLOUD_MIN_DURATION, CLOUD_MAX_DURATION);
  
  for (int i = 0; i < cloudDimSteps; i++) {
    adjustChannel(1, -1);
    delay(100);
    adjustChannel(3, -1);
    delay(100);
  }
  
  cloudState = CLOUD_DIMMING;
}

void startCloudBrighten() {
  cloudState = CLOUD_BRIGHTENING;
  cloudBrightenSteps = 0;
  lastCloudStepTime = millis();
}

void updateCloudBrighten() {
  unsigned long now = millis();
  unsigned long stepInterval = CLOUD_FADE_TIME / cloudDimSteps;
  
  if (now - lastCloudStepTime >= stepInterval && cloudBrightenSteps < cloudDimSteps) {
    adjustChannel(1, 1);
    delay(100);
    adjustChannel(3, 1);
    
    cloudBrightenSteps++;
    lastCloudStepTime = now;
  }
  
  if (cloudBrightenSteps >= cloudDimSteps) {
    cloudState = NO_CLOUD;
    unsigned long interval = random(CLOUD_MIN_INTERVAL, CLOUD_MAX_INTERVAL);
    nextCloudTime = millis() + interval;
    
    Serial.print("☁️  Next cloud in ");
    Serial.print(interval / 60000);
    Serial.println(" minutes");
  }
}

void triggerManualCloud() {
  if (currentLightMode == FULL_DAYLIGHT && cloudState == NO_CLOUD) {
    Serial.println("☁️  Manual cloud triggered!");
    startCloud();
  }
}

#endif