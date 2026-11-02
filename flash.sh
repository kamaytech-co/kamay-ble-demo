#!/usr/bin/env bash
# Kamay BLE Demo - flashea los binarios precompilados con esptool.
#
# Uso:
#   ./flash.sh [env] [puerto] [baudios]
#
#   env      esp32dev (por defecto) | esp32-c3-devkitm-1 | esp32-s3-devkitc-1
#   puerto   /dev/ttyUSB0, /dev/ttyACM0, COM5 ...  (vacio = esptool lo detecta)
#   baudios  460800 por defecto; prueba 115200 si el puerto es inestable
#
# Busca bootloader.bin, partitions.bin, boot_app0.bin y firmware.bin en este orden:
#   1. la carpeta donde esta este script (layout de los zips de release)
#   2. <carpeta del script>/<env>/ y ./<env>/ (layout del artefacto dist/ de CI)
#   3. .pio/build/<env>/ (compilacion local con pio run)
# boot_app0.bin es opcional: sin el, el bootloader arranca igual la app en ota_0.
#
# Requiere esptool 4.x o 5.x:  pip install esptool
set -euo pipefail

ENV_NAME="${1:-esp32dev}"
PORT="${2:-}"
BAUD="${3:-460800}"

# Mismos parametros que usa PlatformIO al subir (platform espressif32 7.x):
# bootloader en 0x1000 en ESP32 clasico y en 0x0 en C3 / S3; flash_mode dio en
# la linea de comandos aunque la placa sea QIO (el header del bootloader manda).
case "$ENV_NAME" in
  esp32dev)           CHIP=esp32;   BOOT_OFFSET=0x1000; FLASH_FREQ=40m ;;
  esp32-c3-devkitm-1) CHIP=esp32c3; BOOT_OFFSET=0x0;    FLASH_FREQ=80m ;;
  esp32-s3-devkitc-1) CHIP=esp32s3; BOOT_OFFSET=0x0;    FLASH_FREQ=80m ;;
  *)
    echo "env desconocido: $ENV_NAME (usa esp32dev, esp32-c3-devkitm-1 o esp32-s3-devkitc-1)" >&2
    exit 1 ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR=""
for d in "$SCRIPT_DIR" "$SCRIPT_DIR/$ENV_NAME" "$PWD/$ENV_NAME" \
         "$SCRIPT_DIR/.pio/build/$ENV_NAME" "$PWD/.pio/build/$ENV_NAME"; do
  if [ -f "$d/firmware.bin" ] && [ -f "$d/bootloader.bin" ] && [ -f "$d/partitions.bin" ]; then
    BIN_DIR="$d"
    break
  fi
done
if [ -z "$BIN_DIR" ]; then
  echo "No encontre firmware.bin, bootloader.bin y partitions.bin para $ENV_NAME" >&2
  echo "Descarga el zip del release para tu placa o compila con: pio run -e $ENV_NAME" >&2
  exit 1
fi

# esptool puede estar instalado como esptool.py, esptool o solo como modulo.
if command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL=(esptool.py)
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL=(esptool)
elif python3 -c 'import esptool' >/dev/null 2>&1; then
  ESPTOOL=(python3 -m esptool)
else
  echo "esptool no encontrado. Instalalo con: pip install esptool" >&2
  exit 1
fi

PORT_ARGS=()
if [ -n "$PORT" ]; then
  PORT_ARGS=(--port "$PORT")
fi

IMAGES=("$BOOT_OFFSET" "$BIN_DIR/bootloader.bin" 0x8000 "$BIN_DIR/partitions.bin")
if [ -f "$BIN_DIR/boot_app0.bin" ]; then
  IMAGES+=(0xe000 "$BIN_DIR/boot_app0.bin")
fi
IMAGES+=(0x10000 "$BIN_DIR/firmware.bin")

echo "Flasheando $ENV_NAME ($CHIP) desde $BIN_DIR"
"${ESPTOOL[@]}" --chip "$CHIP" ${PORT_ARGS[@]+"${PORT_ARGS[@]}"} --baud "$BAUD" \
  --before default_reset --after hard_reset \
  write_flash -z --flash_mode dio --flash_freq "$FLASH_FREQ" --flash_size detect \
  "${IMAGES[@]}"

echo
echo "Listo. Abre el monitor serie a 115200 baudios, por ejemplo: pio device monitor"
