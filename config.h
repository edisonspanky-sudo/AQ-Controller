#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════════
// TEMPERATURE SETTINGS
// ═══════════════════════════════════════════════════════════════
const float TARGET_TEMP = 78.0;
const float TEMP_HYSTERESIS = 0.5;
const float TEMP_DIFFERENTIAL_ALERT = 1.0;
const float TEMP_EMERGENCY_HIGH = 82.0;

// ═══════════════════════════════════════════════════════════════
// ATO SETTINGS
// ═══════════════════════════════════════════════════════════════
const unsigned long ATO_TIMEOUT = 300000;        // 5 min
const unsigned long ATO_COOLDOWN = 60000;        // 1 min
const unsigned long ATO_MIN_RUNTIME = 2000;      // 2 sec
// Float switch polarity (with INPUT_PULLUP, most float switches are ACTIVE-LOW when closed)
#define CFG_ATO_FLOAT_ACTIVE_LOW true

// ═══════════════════════════════════════════════════════════════
// LIGHTING SCHEDULE
// ═══════════════════════════════════════════════════════════════
const uint8_t SUNRISE_START_HOUR = 9;
const uint8_t SUNRISE_START_MINUTE = 30;
const uint8_t SUNRISE_END_HOUR = 10;
const uint8_t SUNRISE_END_MINUTE = 0;

const uint8_t SUNSET_START_HOUR = 21;
const uint8_t SUNSET_START_MINUTE = 30;
const uint8_t SUNSET_END_HOUR = 22;
const uint8_t SUNSET_END_MINUTE = 0;

const int RAMP_STEPS = 20;
const unsigned long RAMP_DURATION = 1800000;  // 30 min
const unsigned long STEP_INTERVAL = RAMP_DURATION / RAMP_STEPS;

// ═══════════════════════════════════════════════════════════════
// CLOUD SIMULATION
// ═══════════════════════════════════════════════════════════════
const unsigned long CLOUD_MIN_INTERVAL = 600000;   // 10 min
const unsigned long CLOUD_MAX_INTERVAL = 1800000;  // 30 min
const int CLOUD_MIN_DIM_STEPS = 3;
const int CLOUD_MAX_DIM_STEPS = 6;
const unsigned long CLOUD_MIN_DURATION = 20000;    // 20 sec
const unsigned long CLOUD_MAX_DURATION = 60000;    // 60 sec
const unsigned long CLOUD_FADE_TIME = 15000;       // 15 sec

// ═══════════════════════════════════════════════════════════════
// FEED MODE
// ═══════════════════════════════════════════════════════════════
const unsigned long FEED_MODE_DURATION = 600000;  // 10 min
// Gyre outlet wiring: true = use NC (recommended for "always on" devices)
#define CFG_GYRE_WIRED_NC true

// ═══════════════════════════════════════════════════════════════
// IR LIGHT COMMANDS
// ═══════════════════════════════════════════════════════════════
const uint16_t IR_ADDRESS = 0xEF00;

const uint8_t CMD_POWER = 0x0;
const uint8_t CMD_FULL_BRIGHT = 0x2;
const uint8_t CMD_NIGHT = 0x3;
const uint8_t CMD_CH1_UP = 0x8;
const uint8_t CMD_CH1_DOWN = 0x10;
const uint8_t CMD_CH2_UP = 0x9;
const uint8_t CMD_CH2_DOWN = 0x11;
const uint8_t CMD_CH3_UP = 0xA;
const uint8_t CMD_CH3_DOWN = 0x12;
const uint8_t CMD_CH4_UP = 0xB;
const uint8_t CMD_CH4_DOWN = 0x13;

#endif