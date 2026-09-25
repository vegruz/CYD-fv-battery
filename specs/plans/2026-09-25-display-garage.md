# Display Garage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware ESPHome per il CYD 2,8" che mostra lo stato dell'accumulo SolarEdge letto da Home Assistant, più i file HA (template dell'età del dato, automazione della retroilluminazione), la CI e la documentazione.

**Architecture:** Config ESPHome divisa in package con un solo compito ciascuno (core, data, ui, display). Il driver del display si sceglie con la substitution `display_driver` (`mipi` di default, `ili9xxx` come fallback). Il rendering è in C++: `ui_logic.h` contiene la logica pura (stati, soglie, calcoli) testata sul PC con g++, `ui.h` contiene il disegno. L'età del dato si calcola in HA.

**Tech Stack:** ESPHome 2026.9.0 (ESP-IDF), C++17, Home Assistant YAML, GitHub Actions, g++ (locale o via Docker `gcc:14`).

**Spec:** `specs/2026-09-25-display-garage-design.md`

## Global Constraints

- Scheda: solo Freenove CYD 2,8" (ESP32, ILI9341, 320×240, orizzontale).
- ESPHome fissato a `esphome==2026.9.0` in `requirements.txt`; framework `esp-idf`.
- Driver default `mipi_spi` con `model: ESP32-2432S028`; fallback `ili9xxx` con `model: ILI9341`.
- Parametri solo come `substitutions` in `esphome/display-garage.yaml`: `soc_entity`, `power_entity`, `age_entity`, `power_invert` (default `"false"`), `battery_capacity_kwh` (30), `soc_green` (60), `soc_yellow` (30), `power_deadband_w` (50), `stale_after_min` (30), `backlight_pin` (GPIO21), `display_driver` (`mipi`).
- Convenzione di segno: dopo `power_invert`, **potenza > 0 = carica**.
- Soglie: verde se SOC ≥ `soc_green`, giallo se ≥ `soc_yellow`, altrimenti rosso. Ferma se |P| ≤ deadband. Dato vecchio se età > `stale_after_min` (strettamente maggiore).
- Priorità degli stati: WAITING/UNAVAILABLE > DISCONNECTED > STALE > NORMAL.
- Testi sullo schermo solo in ASCII (il font di default non ha lettere accentate né "…").
- `esphome/secrets.yaml` MUST NOT essere committato.
- Scritture su HA solo dopo approvazione esplicita del proprietario.

**Deviazioni dalla spec, da riportare nella spec al Task 6:**
1. Nessun `display-garage-ili9xxx.yaml`: il fallback si seleziona con `-s display_driver ili9xxx`.
2. Logica pura separata in `ui_logic.h` con test sul PC.
3. Colori definiti in `ui.h` e non in `ui.yaml`.
4. Riga in basso: "26.1 kWh" invece di "26.1 kWh disponibili", perché la versione lunga si sovrappone ad "agg. N min fa" su 280 px.
5. Frecce e pallini disegnati con primitive grafiche (`filled_triangle`, `filled_circle`) invece che con glyph.

## Review Focus

1. Entity_id sbagliato: API connessa ma SOC mai ricevuto → "In attesa dati..." con il pallino HA verde (così si capisce che il problema è l'entità, non la connessione). Test: Task 1, `screen_state` con `soc_received=false` e `api_ok=true` → WAITING.
2. Template dell'età non creato in HA (`age_min` NaN) → nessuno stato vecchio e nessuna età mostrata, il resto normale. Test: Task 1.
3. Disconnessione mentre il dato è già vecchio → vince DISCONNECTED ("HA non connesso"). Test: Task 1.
4. Potenza NaN con SOC valido → riga di sinistra vuota, SOC normale. Test: Task 1 (`power_flow(NaN)` → NONE).
5. SOC fuori range (101, −1) da un glitch dell'integrazione → barra e kWh limitati a 0–100 senza overflow grafico. Test: Task 1 (`clamp_soc`, `bar_fill_px`, `available_kwh`).

---

## File map

| File | Responsabilità |
|---|---|
| `esphome/ui_logic.h` | logica pura: tipi `UiConfig`/`UiInputs`, enum, soglie, calcoli (nessuna dipendenza da ESPHome) |
| `esphome/ui.h` | disegno su `display::Display` a partire da `ui_logic.h` |
| `esphome/display-garage.yaml` | entry point: substitutions + package |
| `esphome/packages/core.yaml` | esp32, wifi, api, ota, logger, retroilluminazione |
| `esphome/packages/data.yaml` | sensori `homeassistant` |
| `esphome/packages/ui.yaml` | include C++ + font |
| `esphome/packages/display-mipi.yaml` | bus SPI + display `mipi_spi` |
| `esphome/packages/display-ili9xxx.yaml` | bus SPI + display `ili9xxx` (fallback) |
| `esphome/secrets.yaml.example` | segreti d'esempio (validi per la CI) |
| `tests/test_ui_logic.cpp` | test sul PC di `ui_logic.h` |
| `scripts/test-logic.sh` | compila ed esegue i test (g++ locale o Docker) |
| `scripts/check.sh` | verifica completa: test + config/compile di entrambi i driver |
| `requirements.txt` | versione di ESPHome |
| `homeassistant/templates/solaredge_data_age.yaml` | template sensor dell'età del dato |
| `homeassistant/automations/display-garage-backlight.yaml` | automazione retroilluminazione |
| `.github/workflows/esphome.yaml` | CI |

---

### Task 1: Logica UI pura con test sul PC

**Files:**
- Create: `esphome/ui_logic.h`
- Create: `tests/test_ui_logic.cpp`
- Create: `scripts/test-logic.sh`
- Modify: `.gitignore` (aggiunge `build/`)

**Interfaces:**
- Consumes: niente.
- Produces (namespace `ui`, header-only):
  - `struct UiConfig { float soc_green; float soc_yellow; float power_deadband_w; bool power_invert; float battery_capacity_kwh; float stale_after_min; };`
  - `struct UiInputs { float soc; bool soc_received; float power_w; float age_min; bool wifi_ok; bool api_ok; };`
  - `enum class Screen { WAITING, UNAVAILABLE, DISCONNECTED, STALE, NORMAL };`
  - `enum class Level { GREEN, YELLOW, RED };`
  - `enum class Flow { NONE, CHARGING, DISCHARGING, IDLE };`
  - `float clamp_soc(float soc);`
  - `Screen screen_state(const UiInputs &in, const UiConfig &cfg);`
  - `Level soc_level(float soc, const UiConfig &cfg);`
  - `Flow power_flow(float power_w, const UiConfig &cfg);`
  - `float power_kw_abs(float power_w);`
  - `float available_kwh(float soc, const UiConfig &cfg);`
  - `int bar_fill_px(float soc, int inner_width);`
  - `bool has_age(const UiInputs &in);`

- [ ] **Step 1: Script dei test**

`scripts/test-logic.sh`:

```sh
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
```

Aggiungere in fondo a `.gitignore`:

```
# Test build
build/
```

- [ ] **Step 2: Test che fallisce**

`tests/test_ui_logic.cpp`:

```cpp
// Test su PC della logica UI (esphome/ui_logic.h). Eseguire con scripts/test-logic.sh.
#include <cmath>
#include <cstdio>

#include "ui_logic.h"

using namespace ui;

static int failures = 0;
#define CHECK(cond)                                                      \
  do {                                                                   \
    if (!(cond)) {                                                       \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                        \
    }                                                                    \
  } while (0)

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static const UiConfig CFG{60.0f, 30.0f, 50.0f, false, 30.0f, 30.0f};

static UiInputs normal_inputs() { return UiInputs{80.0f, true, 0.0f, 5.0f, true, true}; }

static void test_soc_level() {
  struct { float soc; Level expected; } cases[] = {
      {100.0f, Level::GREEN}, {60.0f, Level::GREEN}, {59.9f, Level::YELLOW},
      {30.0f, Level::YELLOW}, {29.9f, Level::RED},   {0.0f, Level::RED},
  };
  for (const auto &c : cases) CHECK(soc_level(c.soc, CFG) == c.expected);
}

static void test_power_flow() {
  struct { float power_w; bool invert; Flow expected; } cases[] = {
      {0.0f, false, Flow::IDLE},          {50.0f, false, Flow::IDLE},
      {-50.0f, false, Flow::IDLE},        {51.0f, false, Flow::CHARGING},
      {-51.0f, false, Flow::DISCHARGING}, {2000.0f, true, Flow::DISCHARGING},
      {-2000.0f, true, Flow::CHARGING},   {NAN, false, Flow::NONE},
  };
  for (const auto &c : cases) {
    UiConfig cfg = CFG;
    cfg.power_invert = c.invert;
    CHECK(power_flow(c.power_w, cfg) == c.expected);
  }
  CHECK(near(power_kw_abs(-2400.0f), 2.4f));
  CHECK(near(power_kw_abs(51.0f), 0.051f));
}

static void test_clamp_and_derived() {
  struct { float soc; float clamped; } cases[] = {
      {-1.0f, 0.0f}, {0.0f, 0.0f}, {50.0f, 50.0f}, {100.0f, 100.0f}, {101.0f, 100.0f},
  };
  for (const auto &c : cases) CHECK(near(clamp_soc(c.soc), c.clamped));
  CHECK(near(available_kwh(87.0f, CFG), 26.1f));
  CHECK(near(available_kwh(101.0f, CFG), 30.0f));
  CHECK(near(available_kwh(-1.0f, CFG), 0.0f));
  CHECK(bar_fill_px(0.0f, 276) == 0);
  CHECK(bar_fill_px(100.0f, 276) == 276);
  CHECK(bar_fill_px(150.0f, 276) == 276);
  CHECK(bar_fill_px(-5.0f, 276) == 0);
  CHECK(bar_fill_px(50.0f, 276) == 138);
}

static void test_screen_state() {
  UiInputs in = normal_inputs();
  CHECK(screen_state(in, CFG) == Screen::NORMAL);

  // Mai ricevuto (es. entity_id errato) anche con API connessa
  in = normal_inputs();
  in.soc_received = false;
  in.soc = NAN;
  CHECK(screen_state(in, CFG) == Screen::WAITING);

  // Entità unavailable in HA
  in = normal_inputs();
  in.soc = NAN;
  CHECK(screen_state(in, CFG) == Screen::UNAVAILABLE);

  // Disconnessioni
  in = normal_inputs();
  in.api_ok = false;
  CHECK(screen_state(in, CFG) == Screen::DISCONNECTED);
  in = normal_inputs();
  in.wifi_ok = false;
  CHECK(screen_state(in, CFG) == Screen::DISCONNECTED);

  // Età: 30 normale, 31 vecchio (soglia 30, strettamente maggiore)
  in = normal_inputs();
  in.age_min = 30.0f;
  CHECK(screen_state(in, CFG) == Screen::NORMAL);
  in.age_min = 31.0f;
  CHECK(screen_state(in, CFG) == Screen::STALE);

  // Disconnesso e vecchio: vince DISCONNECTED
  in.api_ok = false;
  CHECK(screen_state(in, CFG) == Screen::DISCONNECTED);

  // Età sconosciuta (template assente): niente STALE, età non mostrata
  in = normal_inputs();
  in.age_min = NAN;
  CHECK(screen_state(in, CFG) == Screen::NORMAL);
  CHECK(!has_age(in));
  CHECK(has_age(normal_inputs()));

  // Potenza sconosciuta non influisce sullo stato
  in = normal_inputs();
  in.power_w = NAN;
  CHECK(screen_state(in, CFG) == Screen::NORMAL);
}

int main() {
  test_soc_level();
  test_power_flow();
  test_clamp_and_derived();
  test_screen_state();
  if (failures) {
    std::printf("%d FAILED\n", failures);
    return 1;
  }
  std::printf("all passed\n");
  return 0;
}
```

- [ ] **Step 3: Verificare che fallisca**

Run: `sh scripts/test-logic.sh`
Expected: errore di compilazione `ui_logic.h: No such file or directory`.

- [ ] **Step 4: Implementazione**

`esphome/ui_logic.h`:

```cpp
#pragma once
// Logica pura del display garage: nessuna dipendenza da ESPHome, testata su PC
// (tests/test_ui_logic.cpp). Convenzione: dopo power_invert, potenza > 0 = carica.
#include <cmath>

namespace ui {

struct UiConfig {
  float soc_green;             // % minima per il verde
  float soc_yellow;            // % minima per il giallo
  float power_deadband_w;      // |P| <= deadband -> ferma
  bool power_invert;           // true se l'integrazione riporta la scarica come positiva
  float battery_capacity_kwh;  // capacita' utile
  float stale_after_min;       // eta' oltre cui il dato e' vecchio
};

struct UiInputs {
  float soc;          // %, NaN se unavailable
  bool soc_received;  // almeno uno stato ricevuto da HA
  float power_w;      // W, NaN se sconosciuta
  float age_min;      // minuti dall'ultima lettura, NaN se sconosciuta
  bool wifi_ok;
  bool api_ok;
};

enum class Screen { WAITING, UNAVAILABLE, DISCONNECTED, STALE, NORMAL };
enum class Level { GREEN, YELLOW, RED };
enum class Flow { NONE, CHARGING, DISCHARGING, IDLE };

inline float clamp_soc(float soc) {
  if (soc < 0.0f) return 0.0f;
  if (soc > 100.0f) return 100.0f;
  return soc;
}

inline bool has_age(const UiInputs &in) { return !std::isnan(in.age_min); }

inline Screen screen_state(const UiInputs &in, const UiConfig &cfg) {
  if (!in.soc_received) return Screen::WAITING;
  if (std::isnan(in.soc)) return Screen::UNAVAILABLE;
  if (!in.wifi_ok || !in.api_ok) return Screen::DISCONNECTED;
  if (has_age(in) && in.age_min > cfg.stale_after_min) return Screen::STALE;
  return Screen::NORMAL;
}

inline Level soc_level(float soc, const UiConfig &cfg) {
  if (soc >= cfg.soc_green) return Level::GREEN;
  if (soc >= cfg.soc_yellow) return Level::YELLOW;
  return Level::RED;
}

inline Flow power_flow(float power_w, const UiConfig &cfg) {
  if (std::isnan(power_w)) return Flow::NONE;
  const float p = cfg.power_invert ? -power_w : power_w;
  if (p > cfg.power_deadband_w) return Flow::CHARGING;
  if (p < -cfg.power_deadband_w) return Flow::DISCHARGING;
  return Flow::IDLE;
}

inline float power_kw_abs(float power_w) { return std::fabs(power_w) / 1000.0f; }

inline float available_kwh(float soc, const UiConfig &cfg) {
  return cfg.battery_capacity_kwh * clamp_soc(soc) / 100.0f;
}

inline int bar_fill_px(float soc, int inner_width) {
  return static_cast<int>(inner_width * clamp_soc(soc) / 100.0f + 0.5f);
}

}  // namespace ui
```

- [ ] **Step 5: Verificare che passi**

Run: `sh scripts/test-logic.sh`
Expected: `all passed`, exit 0. La prima esecuzione via Docker scarica l'immagine `gcc:14`.

- [ ] **Step 6: Commit**

```bash
git add esphome/ui_logic.h tests/test_ui_logic.cpp scripts/test-logic.sh .gitignore
git commit -m "feat: add pure UI logic with host tests"
```

---

### Task 2: Firmware ESPHome con driver mipi_spi

**Files:**
- Create: `requirements.txt`, `esphome/secrets.yaml.example`, `esphome/display-garage.yaml`
- Create: `esphome/packages/core.yaml`, `esphome/packages/data.yaml`, `esphome/packages/ui.yaml`, `esphome/packages/display-mipi.yaml`
- Create: `esphome/ui.h`
- Create: `scripts/check.sh`
- Create (locale, NON committato): `esphome/secrets.yaml`

**Interfaces:**
- Consumes: tutto `ui_logic.h` (Task 1).
- Produces:
  - `void ui::draw_ui(esphome::display::Display &it, const ui::UiInputs &in, const ui::UiConfig &cfg, esphome::font::Font *big, esphome::font::Font *small);`
  - id ESPHome: `batt_soc`, `batt_power`, `data_age` (sensor), `font_big`, `font_small` (font), `disp` (display), `backlight` (light), `backlight_pwm` (output).
  - Substitution `display_driver` che seleziona `packages/display-${display_driver}.yaml`.
  - Entità HA create dal device: `light.display_garage_retroilluminazione`.

- [ ] **Step 1: Tooling**

`requirements.txt`:

```
esphome==2026.9.0
```

Run (Git Bash):
```bash
py -0                       # elenca i Python installati
py -3.14 -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/esphome.exe version
```
Expected: `Version: 2026.9.0`. Se l'installazione fallisce su 3.14 (dipendenze senza wheel), ricreare il venv con `py -3.13` o `py -3.12`.

- [ ] **Step 2: Segreti**

`esphome/secrets.yaml.example`:

```yaml
# Copiare in secrets.yaml (non committato) e sostituire i valori.
# Chiave API: generare con  python -c "import base64,os;print(base64.b64encode(os.urandom(32)).decode())"
# I valori sotto sono fittizi ma validi, così la CI può compilare.
wifi_ssid: "NOME_RETE"
wifi_password: "PASSWORD_WIFI"
ap_password: "fallback-display"
api_encryption_key: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
ota_password: "CAMBIAMI"
```

Run:
```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
KEY=$(.venv/Scripts/python.exe -c "import base64,os;print(base64.b64encode(os.urandom(32)).decode())")
sed -i "s|^api_encryption_key: .*|api_encryption_key: \"$KEY\"|" esphome/secrets.yaml
OTA=$(.venv/Scripts/python.exe -c "import secrets;print(secrets.token_urlsafe(16))")
sed -i "s|^ota_password: .*|ota_password: \"$OTA\"|" esphome/secrets.yaml
git status --short esphome/   # secrets.yaml NON deve comparire
```
Nota per il proprietario: prima del flash inserire SSID e password Wi-Fi reali in `esphome/secrets.yaml`.

- [ ] **Step 3: Package core**

`esphome/packages/core.yaml`:

```yaml
# Piattaforma, rete, API/OTA e retroilluminazione.
esphome:
  name: ${device_name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "${friendly_name} Fallback"
    password: !secret ap_password

captive_portal:

output:
  - platform: ledc
    pin: ${backlight_pin}
    id: backlight_pwm

light:
  - platform: monochromatic
    output: backlight_pwm
    name: "Retroilluminazione"
    id: backlight
    restore_mode: RESTORE_DEFAULT_ON
```

- [ ] **Step 4: Package data**

`esphome/packages/data.yaml`:

```yaml
# Valori letti da Home Assistant. NaN se l'entità è unavailable/unknown.
sensor:
  - platform: homeassistant
    id: batt_soc
    entity_id: ${soc_entity}
  - platform: homeassistant
    id: batt_power
    entity_id: ${power_entity}
  - platform: homeassistant
    id: data_age
    entity_id: ${age_entity}
```

- [ ] **Step 5: Package ui**

`esphome/packages/ui.yaml`:

```yaml
# Codice di rendering e font. I percorsi degli include sono relativi a esphome/.
esphome:
  includes:
    - ui_logic.h
    - ui.h

font:
  - file:
      type: gfonts
      family: Roboto
      weight: 700
    id: font_big
    size: 90
    glyphs: "0123456789%"
  - file:
      type: gfonts
      family: Roboto
      weight: 400
    id: font_small
    size: 20
```

- [ ] **Step 6: Rendering**

`esphome/ui.h`:

```cpp
#pragma once
// Disegno del display garage (320x240, orizzontale). La logica sta in ui_logic.h.
#include "esphome/components/display/display.h"
#include "esphome/components/font/font.h"
#include "ui_logic.h"

namespace ui {

using esphome::Color;
using esphome::display::Display;
using esphome::display::TextAlign;
using esphome::font::Font;

static const Color C_TEXT(0xE5, 0xE5, 0xE5);
static const Color C_MUTED(0x73, 0x73, 0x73);  // valori non affidabili
static const Color C_FRAME(0x40, 0x40, 0x40);
static const Color C_GREEN(0x22, 0xC5, 0x5E);
static const Color C_YELLOW(0xEA, 0xB3, 0x08);
static const Color C_RED(0xEF, 0x44, 0x44);

static const int ROW_TOP_Y = 16;
static const int SOC_Y = 95;
static const int BAR_X = 20;
static const int BAR_Y = 148;
static const int BAR_W = 280;
static const int BAR_H = 30;
static const int ROW_BOTTOM_Y = 218;

inline Color level_color(Level level) {
  switch (level) {
    case Level::GREEN:
      return C_GREEN;
    case Level::YELLOW:
      return C_YELLOW;
    default:
      return C_RED;
  }
}

inline void draw_links(Display &it, const UiInputs &in, Font *small) {
  it.filled_circle(226, ROW_TOP_Y, 5, in.wifi_ok ? C_GREEN : C_RED);
  it.print(234, ROW_TOP_Y, small, C_TEXT, TextAlign::CENTER_LEFT, "WiFi");
  it.filled_circle(284, ROW_TOP_Y, 5, in.api_ok ? C_GREEN : C_RED);
  it.print(292, ROW_TOP_Y, small, C_TEXT, TextAlign::CENTER_LEFT, "HA");
}

inline void draw_flow(Display &it, const UiInputs &in, const UiConfig &cfg, Font *small, bool muted) {
  const float kw = power_kw_abs(in.power_w);
  switch (power_flow(in.power_w, cfg)) {
    case Flow::CHARGING: {
      const Color c = muted ? C_MUTED : C_GREEN;
      it.filled_triangle(18, 8, 10, 24, 26, 24, c);
      it.printf(34, ROW_TOP_Y, small, c, TextAlign::CENTER_LEFT, "In carica %.1f kW", kw);
      break;
    }
    case Flow::DISCHARGING: {
      const Color c = muted ? C_MUTED : C_YELLOW;
      it.filled_triangle(10, 8, 26, 8, 18, 24, c);
      it.printf(34, ROW_TOP_Y, small, c, TextAlign::CENTER_LEFT, "In scarica %.1f kW", kw);
      break;
    }
    case Flow::IDLE:
      it.print(10, ROW_TOP_Y, small, muted ? C_MUTED : C_TEXT, TextAlign::CENTER_LEFT, "Ferma");
      break;
    case Flow::NONE:
      break;
  }
}

inline void draw_ui(Display &it, const UiInputs &in, const UiConfig &cfg, Font *big, Font *small) {
  const int w = it.get_width();
  const int h = it.get_height();
  const Screen screen = screen_state(in, cfg);
  draw_links(it, in, small);

  if (screen == Screen::WAITING || screen == Screen::UNAVAILABLE) {
    it.print(w / 2, h / 2, small, C_TEXT, TextAlign::CENTER,
             screen == Screen::WAITING ? "In attesa dati..." : "Dati non disponibili");
    return;
  }

  const bool muted = screen != Screen::NORMAL;
  const float soc = clamp_soc(in.soc);
  const Color main = muted ? C_MUTED : level_color(soc_level(soc, cfg));

  draw_flow(it, in, cfg, small, muted);
  it.printf(w / 2, SOC_Y, big, main, TextAlign::CENTER, "%.0f%%", soc);
  it.rectangle(BAR_X, BAR_Y, BAR_W, BAR_H, C_FRAME);
  it.filled_rectangle(BAR_X + 2, BAR_Y + 2, bar_fill_px(soc, BAR_W - 4), BAR_H - 4, main);
  it.printf(BAR_X, ROW_BOTTOM_Y, small, muted ? C_MUTED : C_TEXT, TextAlign::CENTER_LEFT, "%.1f kWh",
            available_kwh(soc, cfg));

  if (screen == Screen::DISCONNECTED) {
    it.print(w - BAR_X, ROW_BOTTOM_Y, small, C_RED, TextAlign::CENTER_RIGHT,
             in.wifi_ok ? "HA non connesso" : "WiFi non connesso");
  } else if (has_age(in)) {
    it.printf(w - BAR_X, ROW_BOTTOM_Y, small, screen == Screen::STALE ? C_YELLOW : C_TEXT,
              TextAlign::CENTER_RIGHT, "agg. %.0f min fa", in.age_min);
  }
}

}  // namespace ui
```

- [ ] **Step 7: Package driver mipi**

`esphome/packages/display-mipi.yaml`:

```yaml
# Driver default: preset CYD 2,8" (ILI9341, CS 15, DC 2 già nel preset).
spi:
  clk_pin: GPIO14
  mosi_pin: GPIO13

display:
  - platform: mipi_spi
    id: disp
    model: ESP32-2432S028
    rotation: 90
    update_interval: 5s
    lambda: |-
      ui::draw_ui(it,
          ui::UiInputs{id(batt_soc).state, id(batt_soc).has_state(), id(batt_power).state,
                       id(data_age).state, wifi::global_wifi_component->is_connected(),
                       api::global_api_server->is_connected()},
          ui::UiConfig{${soc_green}, ${soc_yellow}, ${power_deadband_w}, ${power_invert},
                       ${battery_capacity_kwh}, ${stale_after_min}},
          id(font_big), id(font_small));
```

- [ ] **Step 8: Entry point**

`esphome/display-garage.yaml`:

```yaml
# Display Garage – stato accumulo SolarEdge su CYD 2,8".
# Tutti i parametri modificabili sono qui. Spec: specs/2026-09-25-display-garage-design.md
substitutions:
  device_name: display-garage
  friendly_name: Display Garage

  # Entità Home Assistant (verificare in Strumenti per sviluppatori → Stati)
  soc_entity: sensor.solaredge_storage_level
  power_entity: sensor.solaredge_storage_power
  age_entity: sensor.solaredge_data_age   # template in homeassistant/templates/

  # "true" se l'integrazione riporta la SCARICA come potenza positiva
  power_invert: "false"

  battery_capacity_kwh: "30.0"
  soc_green: "60.0"          # % minima per il verde
  soc_yellow: "30.0"         # % minima per il giallo
  power_deadband_w: "50.0"   # sotto questa potenza (assoluta) la batteria è "Ferma"
  stale_after_min: "30.0"    # oltre questa età il dato è mostrato in grigio

  backlight_pin: GPIO21
  display_driver: mipi       # fallback: ili9xxx (packages/display-ili9xxx.yaml)

packages:
  core: !include packages/core.yaml
  data: !include packages/data.yaml
  ui: !include packages/ui.yaml
  display: !include packages/display-${display_driver}.yaml
```

- [ ] **Step 9: Script di verifica**

`scripts/check.sh`:

```sh
#!/usr/bin/env sh
# Verifica completa: test della logica UI + config/compile per ogni driver display.
# Uso: sh scripts/check.sh [driver...]   (default: mipi ili9xxx)
set -eu
cd "$(dirname "$0")/.."
if [ -x .venv/Scripts/esphome.exe ]; then ESPHOME=.venv/Scripts/esphome.exe
elif [ -x .venv/bin/esphome ]; then ESPHOME=.venv/bin/esphome
else ESPHOME=esphome; fi

sh scripts/test-logic.sh

DRIVERS="${*:-mipi ili9xxx}"
for driver in $DRIVERS; do
  echo "== ESPHome: display_driver=$driver =="
  "$ESPHOME" -s display_driver "$driver" config esphome/display-garage.yaml > /dev/null
  "$ESPHOME" -s display_driver "$driver" compile esphome/display-garage.yaml
done
echo "check OK: $DRIVERS"
```

- [ ] **Step 10: Config**

Run: `.venv/Scripts/esphome.exe config esphome/display-garage.yaml`
Expected: exit 0 e config stampata, con `display_driver: mipi` risolto e il package mipi incluso.
Se l'errore riguarda la substitution nel percorso di `!include`: sostituire con `display: !include packages/display-mipi.yaml` e, nel Task 3, selezionare il fallback con un secondo file d'ingresso. In quel caso fermarsi e segnalarlo prima di procedere.
Se `rotation: 90` viene rifiutato: usare `rotation: 90°`.

- [ ] **Step 11: Compilazione**

Run: `sh scripts/check.sh mipi`
Expected: `all passed`, poi `INFO Successfully compiled program.` e `check OK: mipi`.
Se la toolchain nativa ESP-IDF non funziona su Windows, fare il fallback in Docker e annotarlo per il README:
`MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W)/esphome:/config" ghcr.io/esphome/esphome:2026.9.0 compile display-garage.yaml`

- [ ] **Step 12: Commit**

```bash
git add requirements.txt esphome/secrets.yaml.example esphome/display-garage.yaml esphome/packages esphome/ui.h scripts/check.sh
git status --short   # verificare che esphome/secrets.yaml NON sia in stage
git commit -m "feat: add ESPHome firmware with mipi_spi driver"
```

---

### Task 3: Driver di fallback ili9xxx

**Files:**
- Create: `esphome/packages/display-ili9xxx.yaml`

**Interfaces:**
- Consumes: id `batt_soc`, `batt_power`, `data_age`, `font_big`, `font_small`; `ui::draw_ui` (Task 2).
- Produces: display `id: disp`, selezionabile con `-s display_driver ili9xxx`.

- [ ] **Step 1: Verificare che il fallback oggi fallisca**

Run: `.venv/Scripts/esphome.exe -s display_driver ili9xxx config esphome/display-garage.yaml`
Expected: errore di include, `display-ili9xxx.yaml` non trovato.

- [ ] **Step 2: Package**

`esphome/packages/display-ili9xxx.yaml`:

```yaml
# Fallback se mipi_spi dà immagini corrotte. ili9xxx è deprecata: usare solo se serve.
# Attivare con  substitutions: display_driver: ili9xxx  (o  esphome -s display_driver ili9xxx ...)
spi:
  clk_pin: GPIO14
  mosi_pin: GPIO13

display:
  - platform: ili9xxx
    id: disp
    model: ILI9341            # alternativa se l'immagine è disturbata: ILI9342
    cs_pin: GPIO15
    dc_pin: GPIO2
    invert_colors: false
    rotation: 90
    update_interval: 5s
    lambda: |-
      ui::draw_ui(it,
          ui::UiInputs{id(batt_soc).state, id(batt_soc).has_state(), id(batt_power).state,
                       id(data_age).state, wifi::global_wifi_component->is_connected(),
                       api::global_api_server->is_connected()},
          ui::UiConfig{${soc_green}, ${soc_yellow}, ${power_deadband_w}, ${power_invert},
                       ${battery_capacity_kwh}, ${stale_after_min}},
          id(font_big), id(font_small));
```

La lambda MUST essere identica a quella di `display-mipi.yaml`.

- [ ] **Step 3: Verifica completa**

Run: `sh scripts/check.sh`
Expected: `all passed`, due compilazioni riuscite, `check OK: mipi ili9xxx`.
I warning di deprecazione di `ili9xxx` sono attesi.

- [ ] **Step 4: Commit**

```bash
git add esphome/packages/display-ili9xxx.yaml
git commit -m "feat: add ili9xxx fallback display driver"
```

---

### Task 4: File Home Assistant

**Files:**
- Create: `homeassistant/templates/solaredge_data_age.yaml`
- Create: `homeassistant/automations/display-garage-backlight.yaml`

**Interfaces:**
- Consumes: `light.display_garage_retroilluminazione` (Task 2); `sensor.solaredge_storage_level` (integrazione cloud, da verificare nel Task 7).
- Produces: `sensor.solaredge_data_age` (minuti), consumato dal firmware tramite `age_entity`.

- [ ] **Step 1: Template dell'età**

`homeassistant/templates/solaredge_data_age.yaml`:

```yaml
# Età in minuti dell'ultima lettura SolarEdge. Usa last_reported, aggiornato a ogni
# lettura dell'integrazione anche se il valore non cambia. Si ricalcola ogni minuto (now()).
# Installazione: incollare in configuration.yaml (o in un file di package), oppure
# creare un helper Template → Sensore dall'interfaccia con lo stesso template di stato.
template:
  - sensor:
      - name: "SolarEdge data age"
        unique_id: solaredge_data_age
        unit_of_measurement: "min"
        state_class: measurement
        icon: mdi:timer-sand
        availability: "{{ states.sensor.solaredge_storage_level is not none }}"
        state: >
          {{ ((now() - states.sensor.solaredge_storage_level.last_reported)
              .total_seconds() / 60) | round(0) }}
```

- [ ] **Step 2: Automazione della retroilluminazione**

`homeassistant/automations/display-garage-backlight.yaml`:

```yaml
# Accende il display al movimento rilevato dalla telecamera Reolink del garage e lo spegne
# dopo 5 minuti senza movimento. SEGNAPOSTO: binary_sensor.garage_camera_motion
# (sostituire con l'entità di movimento reale della Reolink).
alias: "Display garage - retroilluminazione su movimento"
description: "Accende il display garage al movimento, spegne dopo 5 minuti di quiete."
mode: restart
triggers:
  - trigger: state
    entity_id: binary_sensor.garage_camera_motion
    to: "on"
    id: movimento
  - trigger: state
    entity_id: binary_sensor.garage_camera_motion
    to: "off"
    for:
      minutes: 5
    id: quiete
actions:
  - choose:
      - conditions:
          - condition: trigger
            id: movimento
        sequence:
          - action: light.turn_on
            target:
              entity_id: light.display_garage_retroilluminazione
      - conditions:
          - condition: trigger
            id: quiete
        sequence:
          - action: light.turn_off
            target:
              entity_id: light.display_garage_retroilluminazione
```

- [ ] **Step 3: Validazione della sintassi YAML**

Run:
```bash
.venv/Scripts/python.exe -c "import yaml,sys;[yaml.safe_load(open(f,encoding='utf-8')) for f in sys.argv[1:]];print('yaml ok')" homeassistant/templates/solaredge_data_age.yaml homeassistant/automations/display-garage-backlight.yaml
```
Expected: `yaml ok`. La validazione semantica si fa in HA nel Task 7 (Strumenti per sviluppatori → Template, per lo stato; "Verifica configurazione").

- [ ] **Step 4: Commit**

```bash
git add homeassistant
git commit -m "feat: add HA data-age template and backlight automation"
```

---

### Task 5: CI GitHub Actions

**Files:**
- Create: `.github/workflows/esphome.yaml`

**Interfaces:**
- Consumes: `scripts/check.sh`, `requirements.txt`, `esphome/secrets.yaml.example`.

- [ ] **Step 1: Workflow**

`.github/workflows/esphome.yaml`:

```yaml
name: ESPHome

on:
  push:
  pull_request:

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
          cache: pip
      - name: Install ESPHome
        run: pip install -r requirements.txt
      - name: Dummy secrets
        run: cp esphome/secrets.yaml.example esphome/secrets.yaml
      - name: Cache toolchains
        uses: actions/cache@v4
        with:
          path: |
            ~/.espressif
            ~/.platformio
            ~/.esphome
            esphome/.esphome
          key: esphome-${{ runner.os }}-${{ hashFiles('requirements.txt') }}
      - name: Logic tests + config + compile (mipi, ili9xxx)
        run: sh scripts/check.sh
```

- [ ] **Step 2: Validazione YAML**

Run: `.venv/Scripts/python.exe -c "import yaml;yaml.safe_load(open('.github/workflows/esphome.yaml'));print('yaml ok')"`
Expected: `yaml ok`.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/esphome.yaml
git commit -m "ci: validate and compile firmware for both display drivers"
```

- [ ] **Step 4: Push e verifica della CI (solo con l'ok del proprietario)**

Chiedere conferma prima del push. Poi:
```bash
git push -u origin main
gh run watch --exit-status
```
Expected: workflow verde. Se fallisce, leggere il log con `gh run view --log-failed` e correggere.

---

### Task 6: Documentazione (README, CLAUDE.md, allineamento della spec)

**Files:**
- Modify: `README.md` (riscrittura)
- Modify: `CLAUDE.md` (sezione Comandi e vincoli)
- Modify: `specs/2026-09-25-display-garage-design.md` (deviazioni elencate in Global Constraints)

- [ ] **Step 1: README**

Sostituire l'intero `README.md` con:

````markdown
# CYD FV Battery – Display Garage

Display a parete (Freenove CYD 2,8", ESP32 + ESPHome) che mostra lo stato dell'accumulo
fotovoltaico SolarEdge letto da Home Assistant: percentuale, barra, kWh disponibili,
carica/scarica con potenza, età del dato e stato della connessione. Serve a decidere
al volo se mettere in carica l'auto elettrica.

```
Inverter SolarEdge ──(cloud, ~15 min)──> Home Assistant ──(API ESPHome)──> CYD 2,8"
```

Design: [`specs/2026-09-25-display-garage-design.md`](specs/2026-09-25-display-garage-design.md)

## Schermata

```
┌──────────────────────────────────────────┐
│ ▲ In carica 2.4 kW            ● WiFi ● HA │
│                  87%                     │  verde ≥60%, giallo ≥30%, rosso sotto
│ [██████████████████████████████░░░░░░░]  │
│ 26.1 kWh                   agg. 12 min fa │
└──────────────────────────────────────────┘
```

| Situazione | Cosa si vede |
|---|---|
| Nessun dato dall'avvio | "In attesa dati..." (se il pallino HA è verde: `entity_id` sbagliato) |
| SOC `unavailable` in HA | "Dati non disponibili" |
| Wi-Fi o HA scollegati | valori in grigio, pallino rosso, "HA non connesso" |
| Dato più vecchio di 30 min | valori in grigio, "agg. N min fa" in giallo |

## Struttura

```
esphome/display-garage.yaml   # entry point: TUTTI i parametri (substitutions)
esphome/packages/             # core, data, ui, display-mipi (default), display-ili9xxx (fallback)
esphome/ui_logic.h, ui.h      # logica (testata su PC) e disegno
homeassistant/                # template età dato + automazione retroilluminazione
tests/, scripts/              # test logica UI, verifica completa
```

## Requisiti

- Python 3.12+ e ESPHome `2026.9.0` (`requirements.txt`)
- Driver USB-seriale della scheda (CH340 o CP210x)
- Per i test della logica: g++ oppure Docker

## Setup Home Assistant

1. **Template età del dato**: copiare `homeassistant/templates/solaredge_data_age.yaml` in
   `configuration.yaml`, oppure creare un helper *Template → Sensore* con lo stesso template di stato.
   Deve comparire `sensor.solaredge_data_age` (minuti).
2. **Adattare gli `entity_id`**: in *Strumenti per sviluppatori → Stati* cercare `solaredge` e
   annotare l'entità del livello batteria (%) e quella della potenza batteria (W). Se sono diverse
   dai default, modificarle in `esphome/display-garage.yaml` (`soc_entity`, `power_entity`) e nel template.
3. **Segno della potenza**: mentre l'app SolarEdge mostra la batteria in carica, controllare il segno
   di `power_entity`. Se è negativo in carica, impostare `power_invert: "true"`.
4. **Automazione retroilluminazione**: importare `homeassistant/automations/display-garage-backlight.yaml`
   sostituendo `binary_sensor.garage_camera_motion` con il sensore di movimento della Reolink.

## Primo flash (USB)

```bash
py -3.12 -m venv .venv
.venv/Scripts/pip install -r requirements.txt
cp esphome/secrets.yaml.example esphome/secrets.yaml   # poi inserire Wi-Fi reale e generare chiavi
.venv/Scripts/esphome run esphome/display-garage.yaml  # scegliere la porta COM della scheda
```

Poi in Home Assistant: *Impostazioni → Dispositivi → ESPHome* → aggiungere "Display Garage"
con la `api_encryption_key` di `secrets.yaml`. Gli aggiornamenti successivi vanno via OTA con lo stesso comando.

In alternativa si può fare il flash dall'add-on ESPHome di HA (copiando i file di `esphome/` in `/config/esphome/`).

## Verifica

```bash
sh scripts/check.sh          # test logica + config/compile di entrambi i driver
sh scripts/test-logic.sh     # solo test logica UI
```

La stessa verifica gira in CI (GitHub Actions) a ogni push.

## Troubleshooting

| Sintomo | Causa probabile | Rimedio |
|---|---|---|
| Schermo nero, retroilluminazione spenta | pin backlight errato o luce spenta da HA | verificare `backlight_pin` (GPIO21), accendere "Retroilluminazione" in HA |
| Colori al negativo | inversione colori | aggiungere `invert_colors: true` al display in `packages/display-mipi.yaml` |
| Rosso e blu scambiati | ordine colori | aggiungere `color_order: bgr` (o `rgb`) |
| Immagine disturbata o corrotta | variante del pannello o driver | provare `model: ESP32-2432S028-9342`; poi il fallback `display_driver: ili9xxx` (con `model: ILI9342` se serve) |
| "In attesa dati..." con pallino HA verde | `entity_id` inesistente | correggere `soc_entity` |
| Età sempre assente | template non creato | vedi Setup HA punto 1 |
| Carica/scarica invertite | segno della potenza | `power_invert: "true"` |
````

- [ ] **Step 2: CLAUDE.md**

Sostituire la sezione `## Comandi` con:

```markdown
## Comandi

- Setup: `py -3.12 -m venv .venv && .venv/Scripts/pip install -r requirements.txt`
- Verifica completa (test logica + config/compile di entrambi i driver): `sh scripts/check.sh`
- Solo test logica UI (g++ o Docker): `sh scripts/test-logic.sh`
- Config/compile singolo driver: `.venv/Scripts/esphome.exe -s display_driver ili9xxx compile esphome/display-garage.yaml`
- Flash/log: `.venv/Scripts/esphome.exe run esphome/display-garage.yaml` / `logs ...`
```

Nella sezione `## Vincoli di progetto` sostituire la riga sul driver con:

```markdown
- Il driver display sta solo in `packages/display-*.yaml` (scelto con la substitution `display_driver`); logica pura in `ui_logic.h` (testata su PC), disegno in `ui.h`. Le due lambda dei driver MUST restare identiche.
```

Nella sezione `## Criteri di qualità dei test` sostituire la prima riga con:

```markdown
- Ogni modifica MUST passare `sh scripts/check.sh` (test logica + config/compile di `mipi` e `ili9xxx`, anche in CI). Ogni modifica a soglie o stati MUST avere un caso in `tests/test_ui_logic.cpp`.
```

- [ ] **Step 3: Allineare la spec**

In `specs/2026-09-25-display-garage-design.md`:
- nella "Struttura del repo" rimuovere `display-garage-ili9xxx.yaml`, aggiungere `ui_logic.h`, `tests/test_ui_logic.cpp`, `scripts/` e `requirements.txt`; aggiungere il parametro `display_driver` alla tabella Parametri;
- nella Test Strategy, Happy Path #3: sostituire `display-garage-ili9xxx.yaml` con `-s display_driver ili9xxx`; rimuovere la frase "Non c'è logica applicativa testabile con coverage" e indicare che gli Edge Cases 1–6 sono coperti da `tests/test_ui_logic.cpp`;
- nel Layout, riga in basso: `26.1 kWh` invece di `26.1 kWh disponibili`;
- in `ui.yaml` rimuovere "colori" (stanno in `ui.h`); ▲▼● sono disegnati con primitive grafiche (rimuovere la nota sui glyph MDI e il rischio corrispondente).

- [ ] **Step 4: Verifica**

Run: `sh scripts/check.sh`
Expected: `check OK: mipi ili9xxx` (i documenti non cambiano la build, ma confermano che il repo è verde).

- [ ] **Step 5: Commit**

```bash
git add README.md CLAUDE.md specs/2026-09-25-display-garage-design.md
git commit -m "docs: complete README, align CLAUDE.md and spec with implementation"
```

---

### Task 7: Verifiche su Home Assistant (manuale, con token)

Prerequisito: il proprietario fornisce l'URL di HA (`http://192.168.0.216:8123` oppure `homeassistant.local`) e un long-lived token, da passare come variabile d'ambiente `HA_TOKEN` e mai scritto in file del repo. Ogni scrittura su HA (creazione del template, automazione) MUST essere mostrata e approvata prima.

**Files:**
- Modify (se servono): `esphome/display-garage.yaml` (entità, `power_invert`), `homeassistant/**` (entità)
- Modify: `specs/2026-09-25-display-garage-design.md` (esiti I1–I4 in Open Questions, poi rimuovere quelle risolte)

- [ ] **Step 1 (I1): Elenco delle entità SolarEdge**

```bash
curl -s -H "Authorization: Bearer $HA_TOKEN" http://192.168.0.216:8123/api/states \
  | .venv/Scripts/python.exe -c "import json,sys;[print(s['entity_id'],s['state'],s['attributes'].get('unit_of_measurement')) for s in json.load(sys.stdin) if 'solaredge' in s['entity_id'] or 'motion' in s['entity_id']]"
```
Expected: l'entità del livello di storage (%) e quella della potenza di storage (W), più le entità di movimento Reolink. Annotarle.

- [ ] **Step 2 (I3): Comportamento di `last_reported`**

```bash
curl -s -H "Authorization: Bearer $HA_TOKEN" http://192.168.0.216:8123/api/states/<soc_entity> \
  | .venv/Scripts/python.exe -m json.tool
```
Ripetere dopo circa 20 minuti. Expected: `last_reported` avanzato anche se `state` e `last_changed` sono uguali. Se non avanza, fermarsi e proporre al proprietario le alternative (vedi Risks nella spec).

- [ ] **Step 3 (I2): Segno della potenza**

Leggere lo stato di `<power_entity>` in un momento in cui l'app SolarEdge mostra la batteria in carica (di giorno, con produzione). Expected: positivo → `power_invert: "false"`; negativo → `"true"`.

- [ ] **Step 4: Aggiornare i default e i file HA**

Sostituire le entità reali in `esphome/display-garage.yaml`, `homeassistant/templates/solaredge_data_age.yaml` e `homeassistant/automations/display-garage-backlight.yaml`. Poi `sh scripts/check.sh` → `check OK`.

- [ ] **Step 5: Creazione in HA (con approvazione)**

Mostrare al proprietario il template e l'automazione definitivi. Solo dopo il suo ok: creare l'helper template (dall'interfaccia, o incollare in `configuration.yaml` e riavviare) e l'automazione. Verificare che `sensor.solaredge_data_age` esista e abbia un valore in minuti.

- [ ] **Step 6: Commit**

```bash
git add esphome/display-garage.yaml homeassistant specs/2026-09-25-display-garage-design.md
git commit -m "chore: set real HA entities and power sign from HA verification"
```

---

### Task 8: Verifiche sull'hardware (manuale, all'arrivo della scheda)

Prerequisito: scheda collegata via USB a questo PC e credenziali Wi-Fi reali in `esphome/secrets.yaml`.

**Files:**
- Modify (se servono): `esphome/packages/display-mipi.yaml` (`invert_colors`, `color_order`, `model`), `esphome/display-garage.yaml` (`backlight_pin`, `display_driver`), `esphome/ui.h` (costanti di layout)
- Modify: `specs/2026-09-25-display-garage-design.md` (esiti H1–H6)

- [ ] **Step 1 (H1): Porta seriale e primo flash**

```powershell
Get-PnpDevice -Class Ports -PresentOnly | Select-Object FriendlyName
```
Expected: `USB-SERIAL CH340 (COMx)` o `CP210x (COMx)`. Poi:
```bash
.venv/Scripts/esphome.exe run esphome/display-garage.yaml --device COMx
```
Expected: flash riuscito e log con `WiFi Connected`, poi `API Client ... connected` una volta aggiunto il dispositivo in HA.

- [ ] **Step 2 (H2–H4): Immagine**

Verifiche con il proprietario davanti allo schermo:
- schermo acceso;
- orientamento orizzontale e dritto;
- colori corretti (verde, giallo, rosso riconoscibili);
- nessun disturbo.

Correzioni in ordine, ricompilando dopo ciascuna:
1. `invert_colors: true`
2. `color_order: bgr`
3. `model: ESP32-2432S028-9342`
4. `rotation: 270`
5. `display_driver: ili9xxx`

- [ ] **Step 3: Stati d'errore (Error Cases 1, 3, 5 della spec)**

- Boot con HA non raggiungibile → "In attesa dati...".
- Disabilitare il device in HA → valori grigi, pallino HA rosso, "HA non connesso".
- Impostare temporaneamente `stale_after_min: "1.0"` → dopo il primo aggiornamento del template l'età diventa gialla e i valori grigi. Poi ripristinare `"30.0"`.

- [ ] **Step 4 (H5): Leggibilità**

A 2–3 m dalla posizione di montaggio: la percentuale è leggibile. Se non lo è, regolare `SOC_Y`/font in `ui.h`/`ui.yaml` e ricontrollare che niente si sovrapponga.

- [ ] **Step 5 (H6): OTA**

Scollegare l'USB e alimentare con l'alimentatore da 5 V. Poi `.venv/Scripts/esphome.exe run esphome/display-garage.yaml --device display-garage.local`. Expected: upload via rete riuscito.

- [ ] **Step 6: Retroilluminazione (I4)**

Passare davanti alla telecamera → display acceso. Aspettare 5 minuti senza movimento → display spento.

- [ ] **Step 7: Chiusura**

`sh scripts/check.sh` → `check OK`. Aggiornare la spec: esiti H1–H6 e I4, rimuovere le Open Questions risolte, Status → `Implemented`.

```bash
git add -A
git status --short   # secrets.yaml NON deve comparire
git commit -m "chore: hardware-verified display settings"
```
