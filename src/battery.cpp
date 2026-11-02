// Kamay BLE Demo - implementacion del nivel de bateria.

#include "battery.h"

#include <Arduino.h>

#ifdef KAMAY_BAT_ADC_PIN
// ---------------------------------------------------------------------------
// ADC: divisor resistivo entre Vbat y GND, punto medio al pin ADC.
//
//   Vbat ---[ R1 ]---+---[ R2 ]--- GND        con R1 = R2 (2:1) Vadc = Vbat / 2
//                    |
//                  GPIO KAMAY_BAT_ADC_PIN
//
// Usa un pin de ADC1 (ESP32: GPIO32 a 39; C3: GPIO0 a 4; S3: GPIO1 a 10).
// Con atenuacion de 11 dB el rango util del ADC llega a unos 2.5 V, asi que
// 4.2 V / 2 = 2.1 V queda dentro. analogReadMilliVolts() aplica la
// calibracion de fabrica (eFuse) cuando existe.
// ---------------------------------------------------------------------------

#ifndef KAMAY_BAT_DIVIDER
#define KAMAY_BAT_DIVIDER 2.0f
#endif

static const int kEmptyMv   = 3300;
static const int kFullMv    = 4200;
static const int kSamples   = 8;
static int       s_lastMv   = -1;

const char* kamayBatteryMode() { return "ADC"; }

void kamayBatteryBegin() {
    analogReadResolution(12);
    analogSetPinAttenuation(KAMAY_BAT_ADC_PIN, ADC_11db);
}

uint8_t kamayBatteryRead() {
    uint32_t acc = 0;
    for (int i = 0; i < kSamples; i++) {
        acc += analogReadMilliVolts(KAMAY_BAT_ADC_PIN);
    }
    int vbat = (int)((acc / kSamples) * (float)KAMAY_BAT_DIVIDER);
    s_lastMv = vbat;
    long pct = map(vbat, kEmptyMv, kFullMv, 0, 100);
    return (uint8_t)constrain(pct, 0L, 100L);
}

int kamayBatteryMilliVolts() { return s_lastMv; }

#else
// ---------------------------------------------------------------------------
// FIXED: valor constante definido en build_flags.
// ---------------------------------------------------------------------------

#ifndef KAMAY_BAT_FIXED
#define KAMAY_BAT_FIXED 100
#endif

#if KAMAY_BAT_FIXED < 0 || KAMAY_BAT_FIXED > 100
#error "KAMAY_BAT_FIXED debe estar entre 0 y 100"
#endif

const char* kamayBatteryMode() { return "FIXED"; }

void kamayBatteryBegin() {}

uint8_t kamayBatteryRead() { return (uint8_t)KAMAY_BAT_FIXED; }

int kamayBatteryMilliVolts() { return -1; }

#endif
