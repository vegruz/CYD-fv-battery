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
