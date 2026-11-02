// Kamay BLE Demo - fuente del nivel de bateria (Battery Level 0x2A19).
//
// Dos modos, elegidos en tiempo de compilacion:
//
//   FIXED  (por defecto) Publica siempre KAMAY_BAT_FIXED (0 a 100, defecto 100).
//                        Util cuando la placa va por USB y no hay bateria.
//   ADC    Se activa definiendo -DKAMAY_BAT_ADC_PIN=<gpio>. Lee la tension de
//          la bateria a traves de un divisor resistivo (2:1 por defecto,
//          ajustable con -DKAMAY_BAT_DIVIDER=x.x) y mapea 3.30 V = 0 %,
//          4.20 V = 100 % (celda Li-ion / LiPo de 1S).
#pragma once

#include <stdint.h>

// "FIXED" o "ADC".
const char* kamayBatteryMode();

// Configura el ADC si aplica. Llamar una vez en setup().
void kamayBatteryBegin();

// Nivel de bateria en porcentaje (0 a 100).
uint8_t kamayBatteryRead();

// Ultima tension de bateria estimada en mV (solo ADC; -1 en FIXED).
int kamayBatteryMilliVolts();
