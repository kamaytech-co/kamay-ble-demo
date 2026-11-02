// Kamay BLE Demo - backend de sensor (temperatura / humedad)
//
// El backend se elige en tiempo de compilacion con -DKAMAY_SENSOR=<modo>:
//
//   AUTO      INTERNAL si el chip tiene sensor de temperatura del die
//             (ESP32-S2 / S3 / C3), si no SYNTH. Es el valor por defecto.
//   SYNTH     Paseo aleatorio con reversion a la media alrededor de 24 C / 55 %.
//             Valores plausibles de interior, sin hardware adicional.
//   INTERNAL  Temperatura del die del chip via temperatureRead(). Sin humedad.
//             En ESP32 clasico cae a SYNTH: su sensor legado no esta calibrado.
//   DHT22     Sensor DHT22 / AM2302 en KAMAY_DHT_PIN (esqueleto, ver sensors.cpp).
//   SHT31     Sensor SHT31 por I2C en KAMAY_SHT31_ADDR (esqueleto, ver sensors.cpp).
//
// IMPORTANTE: la resolucion del modo pedido a KAMAY_SENSOR_ID se hace aqui,
// antes de incluir cualquier libreria, porque DHT.h define la macro DHT22 y
// rompe la comparacion de tokens si se hiciera despues.
#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include <soc/soc_caps.h>

#define KAMAY_SENSOR_AUTO     1
#define KAMAY_SENSOR_SYNTH    2
#define KAMAY_SENSOR_INTERNAL 3
#define KAMAY_SENSOR_DHT22    4
#define KAMAY_SENSOR_SHT31    5

#ifndef KAMAY_SENSOR
#define KAMAY_SENSOR AUTO
#endif

#define KAMAY_CAT_(a, b) a##b
#define KAMAY_CAT(a, b)  KAMAY_CAT_(a, b)
#define KAMAY_SENSOR_REQUESTED KAMAY_CAT(KAMAY_SENSOR_, KAMAY_SENSOR)

// Sensor de temperatura del die utilizable: S2 / S3 / C3 (driver temp_sensor).
// El ESP32 clasico expone temprature_sens_read(), pero no esta calibrado.
#if defined(SOC_TEMP_SENSOR_SUPPORTED) && SOC_TEMP_SENSOR_SUPPORTED && !defined(CONFIG_IDF_TARGET_ESP32)
#define KAMAY_HAS_DIE_TEMP 1
#else
#define KAMAY_HAS_DIE_TEMP 0
#endif

#if KAMAY_SENSOR_REQUESTED == KAMAY_SENSOR_AUTO
#  if KAMAY_HAS_DIE_TEMP
#    define KAMAY_SENSOR_ID KAMAY_SENSOR_INTERNAL
#  else
#    define KAMAY_SENSOR_ID KAMAY_SENSOR_SYNTH
#  endif
#elif KAMAY_SENSOR_REQUESTED == KAMAY_SENSOR_SYNTH
#  define KAMAY_SENSOR_ID KAMAY_SENSOR_SYNTH
#elif KAMAY_SENSOR_REQUESTED == KAMAY_SENSOR_INTERNAL
#  if KAMAY_HAS_DIE_TEMP
#    define KAMAY_SENSOR_ID KAMAY_SENSOR_INTERNAL
#  else
#    warning "KAMAY_SENSOR=INTERNAL no es utilizable en este chip; usando SYNTH"
#    define KAMAY_SENSOR_ID KAMAY_SENSOR_SYNTH
#  endif
#elif KAMAY_SENSOR_REQUESTED == KAMAY_SENSOR_DHT22
#  define KAMAY_SENSOR_ID KAMAY_SENSOR_DHT22
#elif KAMAY_SENSOR_REQUESTED == KAMAY_SENSOR_SHT31
#  define KAMAY_SENSOR_ID KAMAY_SENSOR_SHT31
#else
#  error "KAMAY_SENSOR debe ser AUTO, SYNTH, INTERNAL, DHT22 o SHT31"
#endif

// Lectura fisica. Si un canal no esta disponible (por ejemplo humedad en
// INTERNAL, o fallo de lectura del DHT22), su bandera *Valid queda en false y
// main.cpp publica el valor "desconocido" del perfil GATT.
struct KamayReading {
    bool  tempValid;
    float tempC;
    bool  humValid;
    float humPct;
};

// Nombre del backend efectivo ("SYNTH", "INTERNAL", "DHT22", "SHT31").
const char* kamaySensorName();

// Inicializa el backend. Llamar una vez en setup().
void kamaySensorBegin();

// Devuelve la lectura mas reciente. Pensada para llamarse cada 1 s.
KamayReading kamaySensorRead();
