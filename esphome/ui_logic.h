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

// Testo della schermata WAITING: dopo un reboot per timeout di API/WiFi non ci sono
// ultimi valori da mostrare in grigio, quindi si spiega il motivo dell'attesa.
inline const char *waiting_text(const UiInputs &in) {
  if (!in.wifi_ok) return "WiFi non connesso";
  if (!in.api_ok) return "HA non connesso";
  return "In attesa dati...";
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
