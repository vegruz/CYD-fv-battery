#!/usr/bin/env sh
# Compila ed esegue i test della logica UI sul PC (g++ locale, altrimenti Docker gcc:14).
set -eu
cd "$(dirname "$0")/.."
CMD='mkdir -p build && g++ -std=c++17 -Wall -Wextra -Werror -Iesphome tests/test_ui_logic.cpp -o build/test_ui_logic && ./build/test_ui_logic'
if command -v g++ >/dev/null 2>&1; then
  sh -c "$CMD"
else
  SRC="$(pwd -W 2>/dev/null || pwd)"
  MSYS_NO_PATHCONV=1 docker run --rm -v "$SRC:/src" -w /src gcc:14 sh -c "$CMD"
fi
