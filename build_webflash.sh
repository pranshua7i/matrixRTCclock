#!/usr/bin/env zsh
set -e

PIO="$HOME/.platformio/penv/bin/pio"
PYTHON="$HOME/.platformio/penv/bin/python3"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
BOOT_APP="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
DOCS="docs"

merge() {
  local env=$1
  local out=$2
  $PYTHON "$ESPTOOL" --chip esp32 merge_bin \
    --flash_mode dio --flash_freq 40m --flash_size 4MB \
    -o "$DOCS/$out" \
    0x1000  ".pio/build/$env/bootloader.bin" \
    0x8000  ".pio/build/$env/partitions.bin" \
    0xe000  "$BOOT_APP" \
    0x10000 ".pio/build/$env/firmware.bin"
}

echo "==> Building blink-on..."
$PIO run -e blink-on
merge blink-on merged-firmware-blink-on.bin
echo "    -> docs/merged-firmware-blink-on.bin"

echo "==> Building blink-off..."
$PIO run -e blink-off
merge blink-off merged-firmware-blink-off.bin
echo "    -> docs/merged-firmware-blink-off.bin"

echo ""
echo "==> Build complete. Deploy with:"
echo "    npx surge docs"
