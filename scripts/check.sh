#!/usr/bin/env sh
# Verifica completa: test della logica UI + config/compile per ogni driver display.
# Uso: sh scripts/check.sh [driver...]   (default: mipi ili9xxx)
set -eu
cd "$(dirname "$0")/.."
if [ -x .venv/Scripts/esphome.exe ]; then ESPHOME=.venv/Scripts/esphome.exe
elif [ -x .venv/bin/esphome ]; then ESPHOME=.venv/bin/esphome
else ESPHOME=esphome; fi

# Git Bash su Windows: l'installer ESP-IDF rifiuta MSYS e serve un percorso corto per la toolchain.
if [ -n "${MSYSTEM:-}" ]; then
  ESPHOME_ESP_IDF_PREFIX="${ESPHOME_ESP_IDF_PREFIX:-C:/ESPHome/idf}"
  export ESPHOME_ESP_IDF_PREFIX
  unset MSYSTEM
fi

sh scripts/test-logic.sh

DRIVERS="${*:-mipi ili9xxx}"
for driver in $DRIVERS; do
  echo "== ESPHome: display_driver=$driver =="
  "$ESPHOME" -s display_driver "$driver" config esphome/display-garage.yaml > /dev/null
  "$ESPHOME" -s display_driver "$driver" compile esphome/display-garage.yaml
done
echo "check OK: $DRIVERS"
