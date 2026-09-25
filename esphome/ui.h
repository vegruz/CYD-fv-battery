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
  it.filled_circle(216, ROW_TOP_Y, 5, in.wifi_ok ? C_GREEN : C_RED);
  it.print(224, ROW_TOP_Y, small, C_TEXT, TextAlign::CENTER_LEFT, "WiFi");
  it.filled_circle(274, ROW_TOP_Y, 5, in.api_ok ? C_GREEN : C_RED);
  it.print(282, ROW_TOP_Y, small, C_TEXT, TextAlign::CENTER_LEFT, "HA");
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
