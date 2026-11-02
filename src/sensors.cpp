// Kamay BLE Demo - implementacion de los backends de sensor.
// La seleccion (KAMAY_SENSOR_ID) se resuelve en sensors.h antes de cualquier
// otro include. Ver la cabecera para la lista de modos.

#include "sensors.h"

#include <Arduino.h>
#include <esp_system.h>
#include <math.h>

// ---------------------------------------------------------------------------
// SYNTH: paseo aleatorio con reversion a la media.
// Se usa como backend principal y tambien como respaldo de INTERNAL en el
// ESP32 clasico. Genera valores plausibles de interior sin hardware extra.
// ---------------------------------------------------------------------------
#if KAMAY_SENSOR_ID == KAMAY_SENSOR_SYNTH

static const float kTempMean = 24.0f;   // C
static const float kHumMean  = 55.0f;   // %
static const float kPull     = 0.05f;   // fuerza de reversion por tick
static float s_temp = kTempMean;
static float s_hum  = kHumMean;

static float frand(float lo, float hi) {
    return lo + (hi - lo) * (float)esp_random() / 4294967295.0f;
}

const char* kamaySensorName() { return "SYNTH"; }

void kamaySensorBegin() {
    // Arranque desplazado para que dos placas vecinas no muestren lo mismo.
    s_temp = kTempMean + frand(-1.5f, 1.5f);
    s_hum  = kHumMean  + frand(-6.0f, 6.0f);
}

KamayReading kamaySensorRead() {
    s_temp += (kTempMean - s_temp) * kPull + frand(-0.12f, 0.12f);
    s_hum  += (kHumMean  - s_hum)  * kPull + frand(-0.60f, 0.60f);
    s_temp = constrain(s_temp, 18.0f, 30.0f);
    s_hum  = constrain(s_hum,  35.0f, 75.0f);
    return KamayReading{true, s_temp, true, s_hum};
}

// ---------------------------------------------------------------------------
// INTERNAL: sensor de temperatura del die (S2 / S3 / C3).
// Ojo: mide el silicio, no el aire. Suele estar 10 a 25 C por encima del
// ambiente con el radio BLE activo. No hay humedad: se publica "desconocido".
// ---------------------------------------------------------------------------
#elif KAMAY_SENSOR_ID == KAMAY_SENSOR_INTERNAL

const char* kamaySensorName() { return "INTERNAL"; }

void kamaySensorBegin() {}

KamayReading kamaySensorRead() {
    float t = temperatureRead();   // esp32-hal-misc.c: driver temp_sensor del IDF
    bool ok = !isnan(t) && t > -40.0f && t < 125.0f;
    return KamayReading{ok, ok ? t : 0.0f, false, 0.0f};
}

// ---------------------------------------------------------------------------
// DHT22 / AM2302 (esqueleto, sin probar en hardware).
//
// TODO para activarlo:
//   1. En platformio.ini, en el env que uses, agrega:
//        lib_deps =
//            ${env.lib_deps}
//            adafruit/DHT sensor library@^1.4.6
//            adafruit/Adafruit Unified Sensor@^1.1.14
//        build_flags =
//            ${env.build_flags}
//            -DKAMAY_SENSOR=DHT22
//            -DKAMAY_DHT_PIN=4
//   2. Cableado (modulo de 3 pines o sensor de 4 pines):
//        VCC   -> 3V3
//        GND   -> GND
//        DATA  -> GPIO KAMAY_DHT_PIN (por defecto 4), con pull-up de 10 k a 3V3
//                 (los modulos de 3 pines ya la traen; el sensor pelado no)
//      Evita los pines de strapping (GPIO0, 2, 12, 15 en ESP32) y los solo
//      entrada (GPIO34 a 39) porque el DHT necesita un pin bidireccional.
//   3. El DHT22 admite una lectura cada 2 s; aqui se cachea entre lecturas.
// ---------------------------------------------------------------------------
#elif KAMAY_SENSOR_ID == KAMAY_SENSOR_DHT22

#if !__has_include(<DHT.h>)
#error "KAMAY_SENSOR=DHT22 necesita 'adafruit/DHT sensor library' y 'adafruit/Adafruit Unified Sensor' en lib_deps (ver README)"
#endif
#include <DHT.h>

#ifndef KAMAY_DHT_PIN
#define KAMAY_DHT_PIN 4
#endif

static DHT           s_dht(KAMAY_DHT_PIN, DHT22);
static KamayReading  s_last = {false, 0.0f, false, 0.0f};
static unsigned long s_lastMs = 0;

const char* kamaySensorName() { return "DHT22"; }

void kamaySensorBegin() {
    s_dht.begin();
}

KamayReading kamaySensorRead() {
    unsigned long now = millis();
    if (now - s_lastMs >= 2000 || s_lastMs == 0) {
        s_lastMs = now;
        float t = s_dht.readTemperature();
        float h = s_dht.readHumidity();
        s_last.tempValid = !isnan(t);
        s_last.tempC     = s_last.tempValid ? t : 0.0f;
        s_last.humValid  = !isnan(h);
        s_last.humPct    = s_last.humValid ? h : 0.0f;
    }
    return s_last;
}

// ---------------------------------------------------------------------------
// SHT31 por I2C (esqueleto, sin probar en hardware).
//
// TODO para activarlo:
//   1. En platformio.ini, en el env que uses, agrega:
//        lib_deps =
//            ${env.lib_deps}
//            adafruit/Adafruit SHT31 Library@^2.2.2
//            adafruit/Adafruit BusIO@^1.16.1
//        build_flags =
//            ${env.build_flags}
//            -DKAMAY_SENSOR=SHT31
//            -DKAMAY_SHT31_ADDR=0x44
//   2. Cableado (modulo tipo Adafruit / GY-SHT31):
//        VIN   -> 3V3
//        GND   -> GND
//        SDA   -> SDA de tu placa (ESP32: GPIO21, C3 y S3 DevKit: GPIO8)
//        SCL   -> SCL de tu placa (ESP32: GPIO22, C3 y S3 DevKit: GPIO9)
//        ADDR  -> GND = 0x44 (por defecto), 3V3 = 0x45
//      Si tu placa usa otros pines, define -DKAMAY_I2C_SDA=.. -DKAMAY_I2C_SCL=..
// ---------------------------------------------------------------------------
#elif KAMAY_SENSOR_ID == KAMAY_SENSOR_SHT31

#if !__has_include(<Adafruit_SHT31.h>)
#error "KAMAY_SENSOR=SHT31 necesita 'adafruit/Adafruit SHT31 Library' y 'adafruit/Adafruit BusIO' en lib_deps (ver README)"
#endif
#include <Wire.h>
#include <Adafruit_SHT31.h>

#ifndef KAMAY_SHT31_ADDR
#define KAMAY_SHT31_ADDR 0x44
#endif

static Adafruit_SHT31 s_sht;
static bool           s_ok = false;

const char* kamaySensorName() { return "SHT31"; }

void kamaySensorBegin() {
#if defined(KAMAY_I2C_SDA) && defined(KAMAY_I2C_SCL)
    Wire.begin(KAMAY_I2C_SDA, KAMAY_I2C_SCL);
#else
    Wire.begin();
#endif
    s_ok = s_sht.begin(KAMAY_SHT31_ADDR);
}

KamayReading kamaySensorRead() {
    if (!s_ok) {
        s_ok = s_sht.begin(KAMAY_SHT31_ADDR);   // reintento si no respondio al inicio
        if (!s_ok) return KamayReading{false, 0.0f, false, 0.0f};
    }
    float t = s_sht.readTemperature();
    float h = s_sht.readHumidity();
    bool tv = !isnan(t);
    bool hv = !isnan(h);
    return KamayReading{tv, tv ? t : 0.0f, hv, hv ? h : 0.0f};
}

#else
#error "KAMAY_SENSOR_ID sin backend (bug en sensors.h)"
#endif
