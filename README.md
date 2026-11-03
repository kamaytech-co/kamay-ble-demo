# Kamay BLE Demo

Firmware abierto para ESP32 que se comporta como un sensor Bluetooth Low Energy
estándar. Es el dispositivo de prueba de la demo Web Bluetooth de
[kamaytech.com.co/lab](https://kamaytech.com.co/lab/) y un punto de partida
para cualquier periférico BLE con perfiles del Bluetooth SIG.

- Se anuncia como `Kamay-Demo-XX` (XX = último byte de la MAC BLE) con los
  servicios 0x181A y 0x180F dentro del paquete de advertising.
- Expone temperatura, humedad, batería e información del dispositivo con UUIDs
  estándar. Sin emparejamiento ni bonding: lectura y notificación planas.
- Refresca los valores cada segundo y notifica cuando hay un cliente conectado.
- Compila para ESP32, ESP32-C3, ESP32-S3 y ESP32-C6 con PlatformIO. CI en GitHub Actions
  con binarios listos para flashear en cada release.

## Placas soportadas

| Env de PlatformIO | Placa | Sensor por defecto (AUTO) |
|---|---|---|
| `esp32dev` (por defecto) | ESP32 DevKit (WROOM-32 y similares) | SYNTH |
| `esp32-c3-devkitm-1` | ESP32-C3-DevKitM-1 | INTERNAL (temperatura del die) |
| `esp32-s3-devkitc-1` | ESP32-S3-DevKitC-1 | INTERNAL (temperatura del die) |
| `esp32-c6-devkitc-1` | ESP32-C6-DevKitC-1 | INTERNAL (temperatura del die) |

Cualquier placa con esos chips debería funcionar; si la tuya tiene otro nombre
en PlatformIO, agrega un `[env:...]` en `platformio.ini` con su `board`.

## Flashear desde el código

Necesitas Python 3 y el cable USB de la placa. Todo lo demás (toolchain,
framework, librerías) lo descarga PlatformIO la primera vez.

```bash
pip install platformio
pio run -t upload          # compila y sube al esp32dev
pio device monitor         # monitor serie a 115200 baudios
```

Para otra placa indica el env:

```bash
pio run -e esp32-c3-devkitm-1 -t upload
pio run -e esp32-s3-devkitc-1 -t upload
pio run -e esp32-c6-devkitc-1 -t upload
```

Notas:

- Si PlatformIO no encuentra el puerto, pasa `--upload-port /dev/ttyUSB0`
  (o `/dev/ttyACM0`, `COM5`). En Linux tu usuario necesita pertenecer al
  grupo `dialout` (o `uucp` según la distribución).
- En la ESP32-S3-DevKitC-1 el monitor serie sale por el conector marcado
  `UART`. Si tu placa S3 solo tiene el USB nativo, agrega
  `-DARDUINO_USB_CDC_ON_BOOT=1` a `build_flags`.
- En la ESP32-C6-DevKitC-1 pasa lo mismo: se flashea por cualquiera de los dos
  conectores, pero los mensajes del firmware salen por el marcado `UART`.
- El C6 usa la plataforma [pioarduino](https://github.com/pioarduino/platform-espressif32)
  (Arduino core 3.3.12), porque la oficial de PlatformIO trae Arduino 2.0.17,
  que no soporta ese chip. PlatformIO la descarga sola la primera vez.
  Las dos plataformas usan los mismos nombres de paquetes y se pisan entre
  sí: si compilas el C6 y otra placa en la misma instalación, cada cambio
  vuelve a descargar el framework. Para no mezclarlas, compila el C6 con un
  PlatformIO aparte:

  ```bash
  python3 -m venv ~/.pioarduino && ~/.pioarduino/bin/pip install platformio
  PLATFORMIO_CORE_DIR=~/.pioarduino/core ~/.pioarduino/bin/pio run -e esp32-c6-devkitc-1 -t upload
  ```
- Si la placa no entra en modo de carga, manten presionado `BOOT` mientras
  esptool intenta conectar.

## Flashear binarios precompilados

Cada release en GitHub trae un zip por placa con `bootloader.bin`,
`partitions.bin`, `boot_app0.bin`, `firmware.bin` y el script `flash.sh`.

```bash
pip install esptool
unzip kamay-ble-demo-v0.2.1-esp32dev.zip
cd esp32dev
./flash.sh esp32dev /dev/ttyUSB0
```

`flash.sh [env] [puerto] [baudios]` usa las mismas direcciones y parámetros
que PlatformIO: bootloader en 0x1000 (ESP32) o 0x0 (C3, S3 y C6), tabla de
particiones en 0x8000, `boot_app0` en 0xe000 y la aplicacion en 0x10000.
También sirve con una compilación local: desde la raíz del repo,
`./flash.sh esp32-c3-devkitm-1` toma los binarios de `.pio/build/`.

## Verificar

Al arrancar, el monitor serie muestra el nombre, la MAC, los bytes del
advertising y luego una linea por segundo con los valores publicados:

```
[boot] Kamay BLE Demo v0.2.1 placa=esp32dev
[boot] sensor=SYNTH bateria=FIXED
[ble] nombre=Kamay-Demo-3C mac=A0:B7:65:12:34:3C
[ble] adv (28 bytes): 02 01 06 05 03 1A 18 0F 18 03 19 40 05 0E 09 4B 61 6D 61 79 2D 44 65 6D 6F 2D 33 43
[ble] anunciando cada 100-150 ms
[tick 1] temp=24.31C hum=54.20% bat=100% clientes=0
[ble] conectado    peer=5A:1F:...  handle=0 clientes=1
[tick 7] temp=24.28C hum=54.61% bat=100% clientes=1 notify
```

### Con nRF Connect (Android o iOS)

1. Abre el escaner y busca `Kamay-Demo-XX`. En los datos de advertising
   deberias ver Flags (0x06), Complete List of 16-bit Service UUIDs con
   0x181A y 0x180F, Appearance 0x0540 (Generic Sensor) y el nombre completo.
2. Conecta. Aparecen tres servicios: Environmental Sensing, Battery Service y
   Device Information. No pide PIN ni emparejamiento.
3. En Environmental Sensing activa las notificaciones de Temperature (0x2A6E).
   El valor cambia cada segundo. Es un `sint16` little-endian en centesimas
   de grado: `5C 09` es 0x095C = 2396, es decir 23.96 C.
4. Humidity (0x2A6F) es un `uint16` en centesimas de porcentaje: `2E 15` es
   0x152E = 5422, es decir 54.22 %. Si el backend no mide humedad (modo
   INTERNAL) veras `FF FF`, que el perfil define como "valor desconocido".
5. Battery Level (0x2A19) es un `uint8` en porcentaje.

### Con la demo web

1. Abre https://kamaytech.com.co/lab/ en Chrome, Edge u Opera (escritorio
   o Android). Safari no implementa Web Bluetooth; en iOS puedes usar Bluefy.
2. Pulsa el botón de conectar y elige `Kamay-Demo-XX` en el selector del
   navegador. El filtro de la página acepta nombres que empiezan por `Kamay`
   o dispositivos que anuncian el servicio 0x181A, así que aparece por ambas
   vias.
3. La página se suscribe a la temperatura y lee la batería; la gráfica se
   actualiza cada segundo.

Si en Linux el navegador no muestra ningún dispositivo, revisa que BlueZ esté
activo (`bluetoothctl show`) y que el navegador tenga acceso a Bluetooth: el
Chromium empaquetado como snap suele no tenerlo. Chrome oficial o Chromium
por apt funcionan.

## Tabla GATT

| Servicio | Característica | UUID | Propiedades | Formato |
|---|---|---|---|---|
| Environmental Sensing 0x181A | Temperature | 0x2A6E | READ, NOTIFY | `sint16` LE, 0.01 C, 0x8000 = desconocido |
| | Humidity | 0x2A6F | READ, NOTIFY | `uint16` LE, 0.01 %, 0xFFFF = desconocido |
| Battery Service 0x180F | Battery Level | 0x2A19 | READ, NOTIFY | `uint8`, 0 a 100 % |
| Device Information 0x180A | Manufacturer Name String | 0x2A29 | READ | `KamayTech` |
| | Model Number String | 0x2A24 | READ | `BLE-DEMO` |
| | Firmware Revision String | 0x2A26 | READ | `KAMAY_FW_VERSION`, por ejemplo `0.2.1` |
| | Hardware Revision String | 0x2A27 | READ | nombre del env, por ejemplo `esp32dev` |

Ademas de estos, NimBLE publica los servicios obligatorios GAP (0x1800) y
GATT (0x1801). La apariencia GAP es 0x0540 (Generic Sensor). A propósito no
se expone Serial Number String (0x2A25): Web Bluetooth la bloquea y hace
fallar la lectura del servicio.

Cada característica con NOTIFY trae su descriptor CCCD (0x2902); el cliente
lo escribe con `startNotifications()` en Web Bluetooth o con el botón de
notificaciones en nRF Connect.

## Bytes del advertising

Todo cabe en el paquete principal (28 de 31 bytes), así que no se usa scan
response: cualquier escaner pasivo ve el nombre y los servicios.

```
02 01 06                                        Flags: LE General Discoverable, BR/EDR no soportado
05 03 1A 18 0F 18                               Complete List of 16-bit Service UUIDs: 0x181A, 0x180F
03 19 40 05                                     Appearance: 0x0540 Generic Sensor
0E 09 4B 61 6D 61 79 2D 44 65 6D 6F 2D 58 58    Complete Local Name: "Kamay-Demo-XX"
```

Los dos últimos bytes del nombre son el último byte de la MAC en hexadecimal
mayuscula. Advertising conectable no dirigido (ADV_IND), intervalo entre 100
y 150 ms, sin lista blanca. Al desconectarse un cliente el firmware vuelve a
anunciar de inmediato; `loop()` además revisa cada segundo que el advertising
siga activo cuando no hay clientes.

## Sensores

El backend se elige al compilar con `-DKAMAY_SENSOR=...`:

| Modo | Que publica | Hardware |
|---|---|---|
| `AUTO` (por defecto) | INTERNAL si el chip tiene sensor de die (C3, S2, S3, C6); si no, SYNTH | ninguno |
| `SYNTH` | Paseo aleatorio con reversion a la media: 24 C y 55 % con ruido suave, acotado a 18 a 30 C y 35 a 75 % | ninguno |
| `INTERNAL` | Temperatura del die via `temperatureRead()`. Humedad = desconocido (0xFFFF) | ninguno |
| `DHT22` | Temperatura y humedad reales | DHT22 / AM2302 |
| `SHT31` | Temperatura y humedad reales | SHT31 por I2C |

Sobre INTERNAL: mide el silicio, no el aire. Con el radio activo suele leer
entre 10 y 25 C por encima del ambiente, y en el ESP32 clasico no existe una
lectura calibrada, así que ahí INTERNAL cae a SYNTH con un aviso del
compilador.

### Conectar un DHT22 más adelante

`src/sensors.cpp` ya trae el esqueleto; falta la libreria y el pin.

1. En `platformio.ini`, en el env que uses:

   ```ini
   lib_deps =
       ${env.lib_deps}
       adafruit/DHT sensor library@^1.4.6
       adafruit/Adafruit Unified Sensor@^1.1.14
   build_flags =
       ${env.build_flags}
       -DKAMAY_SENSOR=DHT22
       -DKAMAY_DHT_PIN=4
   ```

2. Cableado:

   | DHT22 | Placa |
   |---|---|
   | VCC | 3V3 |
   | GND | GND |
   | DATA | GPIO `KAMAY_DHT_PIN` (4 por defecto), con pull-up de 10 k a 3V3 |

   Los modulos de tres pines ya traen la resistencia; el sensor pelado de
   cuatro pines no. Evita los pines de strapping (GPIO0, 2, 12 y 15 en ESP32)
   y los de solo entrada (GPIO34 a 39): el DHT necesita un pin bidireccional.
   El sensor admite una lectura cada 2 s; el firmware cachea entre lecturas.

### Conectar un SHT31 más adelante

1. En `platformio.ini`:

   ```ini
   lib_deps =
       ${env.lib_deps}
       adafruit/Adafruit SHT31 Library@^2.2.2
       adafruit/Adafruit BusIO@^1.16.1
   build_flags =
       ${env.build_flags}
       -DKAMAY_SENSOR=SHT31
       -DKAMAY_SHT31_ADDR=0x44
   ```

2. Cableado:

   | SHT31 | Placa |
   |---|---|
   | VIN | 3V3 |
   | GND | GND |
   | SDA | SDA (ESP32: GPIO21; C3 y S3 DevKit: GPIO8; C6 DevKit: GPIO23) |
   | SCL | SCL (ESP32: GPIO22; C3 y S3 DevKit: GPIO9; C6 DevKit: GPIO22) |
   | ADDR | GND para 0x44, 3V3 para 0x45 |

   Si tu placa usa otros pines I2C, agrega `-DKAMAY_I2C_SDA=.. -DKAMAY_I2C_SCL=..`.

Ninguno de los dos esqueletos se ha probado con hardware todavia; si lo haces
y algo falla, abre un issue.

## Batería

| Modo | Como se activa | Que publica |
|---|---|---|
| FIXED (por defecto) | `-DKAMAY_BAT_FIXED=100` (0 a 100) | siempre ese valor; ideal con la placa por USB |
| ADC | `-DKAMAY_BAT_ADC_PIN=34` | lectura real de una celda Li-ion o LiPo de 1S |

En modo ADC la batería se conecta por un divisor resistivo 2:1 (dos
resistencias iguales, por ejemplo 100 k) y el firmware mapea 3.30 V a 0 % y
4.20 V a 100 % de forma lineal, promediando 8 muestras con la calibración de
fábrica del ADC.

```
Vbat ---[ 100k ]---+---[ 100k ]--- GND
                   |
              GPIO KAMAY_BAT_ADC_PIN
```

Usa un pin de ADC1 (ESP32: GPIO32 a 39; C3: GPIO0 a 4; S3: GPIO1 a 10; C6: GPIO0 a 6). Si
tu divisor no es 2:1, ajusta `-DKAMAY_BAT_DIVIDER=x.x` con la relacion
Vbat / Vadc. Con `KAMAY_BAT_ADC_PIN` definido, `KAMAY_BAT_FIXED` se ignora.
El monitor serie imprime los mV estimados para que calibres.

## Opciones de compilación

Todas van en `build_flags` de `platformio.ini`.

| Flag | Por defecto | Efecto |
|---|---|---|
| `KAMAY_FW_VERSION` | `"0.2.1"` | Firmware Revision (0x2A26) y linea de arranque |
| `KAMAY_BOARD` | `"$PIOENV"` | Hardware Revision (0x2A27); PlatformIO pone el nombre del env |
| `KAMAY_SENSOR` | `AUTO` | backend de sensor (ver arriba) |
| `KAMAY_BAT_FIXED` | `100` | nivel de batería en modo FIXED |
| `KAMAY_BAT_ADC_PIN` | sin definir | activa el modo ADC en ese GPIO |
| `KAMAY_BAT_DIVIDER` | `2.0` | relacion del divisor en modo ADC |
| `KAMAY_DHT_PIN` | `4` | pin de datos del DHT22 |
| `KAMAY_SHT31_ADDR` | `0x44` | dirección I2C del SHT31 |

El stack NimBLE se compila solo con el rol periférico
(`CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED`, `CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED`)
para ahorrar flash y RAM.

## Estructura del repo

```
platformio.ini            envs esp32dev, esp32-c3-devkitm-1, esp32-s3-devkitc-1, esp32-c6-devkitc-1
src/main.cpp              BLE, tabla GATT, advertising, tick de 1 s
src/sensors.h .cpp        backends SYNTH, INTERNAL, DHT22, SHT31
src/battery.h .cpp        batería FIXED o ADC
flash.sh                  flasheo de binarios precompilados con esptool
.github/workflows/        CI: compila los cuatro envs y publica releases en tags v*
```

## Integracion continua y releases

Cada push y cada pull request compilan los cuatro envs y dejan los `.bin` como
artefacto del workflow. Para publicar un release con binarios:

```bash
git tag v0.2.1
git push --tags
```

El workflow crea el release en GitHub con un zip por placa y el `flash.sh`.
Recuerda actualizar `KAMAY_FW_VERSION` en `platformio.ini` para que el
Firmware Revision coincida con el tag.

## Dependencias

- PlatformIO Core 6.x con `platformio/espressif32@7.0.1` (Arduino core 2.0.17)
  para ESP32, C3 y S3.
- ESP32-C6: pioarduino `55.03.312-1` (Arduino core 3.3.12, ESP-IDF 5.5.5).
- `h2zero/NimBLE-Arduino@2.5.1`.

## Licencia

MIT. Copyright (c) 2026 Kamaytech SAS. Ver `LICENSE`.

---

## English summary

Open ESP32 firmware (PlatformIO, Arduino core 2.0.17 or 3.3.12 on the C6, NimBLE-Arduino 2.5.1)
that acts as a standard BLE sensor peripheral for the Web Bluetooth demo at
https://kamaytech.com.co/lab/. It advertises as `Kamay-Demo-XX` (XX = last
byte of the BLE MAC) with service UUIDs 0x181A and 0x180F in the advertising
payload and exposes Environmental Sensing (Temperature 0x2A6E, Humidity
0x2A6F; read and notify), Battery Level 0x2A19 (read and notify) and Device
Information (manufacturer, model, firmware and hardware revisión). No pairing.
Values refresh every second.

Flash from source with `pip install platformio && pio run -t upload`
(envs: `esp32dev`, `esp32-c3-devkitm-1`, `esp32-s3-devkitc-1`, `esp32-c6-devkitc-1`), or flash a
release zip with `./flash.sh <env> <port>` (requires `pip install esptool`).
Sensor backends are selected at compile time (`-DKAMAY_SENSOR=AUTO|SYNTH|INTERNAL|DHT22|SHT31`);
battery level is fixed by default or read through a 2:1 divider on an ADC pin
(`-DKAMAY_BAT_ADC_PIN=34`). MIT licensed.
