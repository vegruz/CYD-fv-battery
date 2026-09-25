# Display Garage (stato accumulo FV su CYD 2,8") — Design Document

## Meta

| Campo | Valore |
|---|---|
| Author | vegruz (con Claude Code) |
| Status | Approved |
| Date | 2026-09-25 |
| ADR correlato | — (decisione inclusa nella sezione Decision) |
| Handoff di origine | [`docs/display-garage-cyd.md`](../docs/display-garage-cyd.md) |

## Decision

### Context and Problem Statement

Serve un display a parete in garage che mostri lo stato dell'accumulo SolarEdge (30 kWh) letto da Home Assistant, per decidere se mettere in carica l'auto senza aprire l'app. Hardware: un solo Freenove ESP32 Display FNK0103 B (ESP32-D0WD-V3, pannello 2,8" TN **ST7789**, 320×240). L'handoff indicava ILI9341: la verifica sulla scheda ha mostrato che è un ST7789 (vedi Implementation Notes). L'handoff proponeva la piattaforma ESPHome `ili9xxx` con framework Arduino. Dalla verifica sulla documentazione ESPHome corrente (settembre 2026) risulta che:

- `ili9xxx` è deprecata ("may be removed in a future release"): il sostituto è `mipi_spi`;
- `mipi_spi` offre il preset `ESP32-2432S028` (CYD 2,8" ILI9341) e le varianti `-7789` / `-9342`;
- dalla versione 2026.1 il framework predefinito per ESP32 è ESP-IDF, e dalla 2026.7 anche la toolchain predefinita è quella nativa ESP-IDF.

### Decision Drivers

- Configurazione mantenibile nel tempo, senza dipendere da componenti deprecati
- Segnalazioni della community di immagini corrotte con `mipi_spi` sulla CYD 2,8"
- Semplicità: una sola schermata statica, nessuna interazione touch

### Considered Options

1. `mipi_spi` con rendering tramite lambda; `ili9xxx` tenuto come package di fallback
2. `ili9xxx` con rendering tramite lambda (proposta dell'handoff)
3. LVGL

### Decision Outcome

Chosen option: "`mipi_spi` con rendering tramite lambda e fallback `ili9xxx`", because è la piattaforma supportata e ha un preset per la scheda. Il rischio di immagini corrotte si gestisce isolando il driver in un package intercambiabile.

### Consequences

- Good, because si evita di migrare in futuro una piattaforma deprecata
- Good, because il preset riduce gli errori su pin e sequenza di inizializzazione
- Good, because tornare a `ili9xxx` richiede di cambiare una sola riga `!include`
- Bad, because il codice di rendering deve essere condiviso tra i due package driver (lo si risolve con un header C++)
- Neutral, because LVGL resta un'evoluzione possibile ma è fuori scope

---

## Context

Flusso dei dati:

```
Inverter SolarEdge ──(cloud SE, ~15 min)──> HA (integrazione SolarEdge cloud) ──(API nativa ESPHome)──> CYD 2,8"
```

- L'integrazione in uso è **quella cloud** e il proprietario ha scelto di non passare a Modbus. Da una scansione della LAN del 2026-09-25 risulta un inverter SolarEdge probabile a `192.168.0.162` (OUI `84:D6:C5`) con nessuna porta TCP aperta tra 1 e 10000: Modbus TCP non è abilitato.
- Con il cloud il dato ha una latenza di circa 15 minuti. Il display quindi MUST mostrare quanto è recente il dato.
- ESPHome riceve i sensori `homeassistant` solo quando lo stato **cambia**. Con un SOC stabile (per esempio al 100%) l'ESP non vede alcun aggiornamento, quindi l'età del dato MUST essere calcolata in HA.

## Goals and Non-goals

### Goals

- G1: mostrare il SOC in %, leggibile a 2–3 m, colorato in base alle soglie (verde ≥ `soc_green`, giallo ≥ `soc_yellow`, rosso sotto)
- G2: mostrare una barra di avanzamento dello stesso colore
- G3: mostrare i kWh disponibili (`battery_capacity_kwh × SOC / 100`)
- G4: mostrare lo stato della batteria (in carica / in scarica / ferma) e la potenza in kW, con una soglia `power_deadband_w`
- G5: mostrare l'età del dato ("agg. N min fa") e segnalarlo come vecchio oltre `stale_after_min`
- G6: segnalare la disconnessione di Wi-Fi e dell'API HA
- G7: retroilluminazione comandata da HA: accesa al movimento rilevato dalla telecamera Reolink, spenta dopo 5 minuti senza movimento
- G8: configurazione parametrica (entità, soglie, capacità, segno della potenza) tramite `substitutions`, senza toccare il C++

### Non-goals

- Scheda Freenove 3,2" (ordine annullato)
- Integrazione SolarEdge Modbus locale
- Consiglio "CARICA ORA / ASPETTA"
- Touch, LVGL, comandi di ricarica
- Anteprima grafica su PC (piattaforma `host` con SDL)

## Design

### Struttura del repo

```
esphome/
├── display-garage.yaml            # entry point: substitutions + include dei package
├── ui_logic.h                     # logica pura (stati, soglie, calcoli), testata su PC
├── ui.h                           # disegno C++ condiviso dai due driver
├── packages/
│   ├── core.yaml                  # esp32 (ESP-IDF), wifi, api, ota, logger, backlight
│   ├── data.yaml                  # sensori homeassistant (SOC, potenza, età dato)
│   ├── display-mipi.yaml          # mipi_spi, preset ESP32-2432S028-7789 (default)
│   ├── display-ili9xxx.yaml       # fallback ili9xxx, stesso id e stessa lambda
│   └── ui.yaml                    # font, include di ui_logic.h e ui.h
└── secrets.yaml.example
homeassistant/
├── templates/solaredge_data_age.yaml
└── automations/display-garage-backlight.yaml
tests/test_ui_logic.cpp            # test su PC di ui_logic.h
scripts/test-logic.sh, check.sh    # test logica; verifica completa (test + config/compile dei due driver)
requirements.txt                   # esphome==2026.9.0
.github/workflows/esphome.yaml     # esegue scripts/check.sh
```

Ogni package ha un solo compito. Il driver si sceglie con la substitution `display_driver` (`display: !include packages/display-${display_driver}.yaml`), anche da riga di comando: `esphome -s display_driver ili9xxx ...`. I package driver MUST esporre un display con `id: disp` e una lambda **identica** che chiama `ui::draw_ui(...)`, così il cambio di driver non tocca il resto.

### Parametri (`substitutions` in `display-garage.yaml`)

| Nome | Default | Significato |
|---|---|---|
| `device_name` | `display-garage` | hostname / nome del nodo |
| `friendly_name` | `Display Garage` | nome mostrato in HA |
| `soc_entity` | `sensor.solaredge_storage_level` | SOC in % (entità segnaposto, da verificare) |
| `power_entity` | `sensor.solaredge_storage_power` | potenza della batteria in W (da verificare) |
| `age_entity` | `sensor.solaredge_data_age` | età del dato in minuti (template HA) |
| `power_scale` | `"1000.0"` | fattore per portare `power_entity` in W (l'integrazione cloud usa kW; `1.0` se è già in W) |
| `power_invert` | `"true"` | `true` se l'integrazione usa un valore positivo per la scarica (così fa la cloud: carica negativa) |
| `battery_capacity_kwh` | `30` | capacità utile |
| `soc_green` / `soc_yellow` | `60` / `30` | soglie di colore in % |
| `power_deadband_w` | `50` | sotto questo valore assoluto la batteria risulta "Ferma" |
| `stale_after_min` | `30` | oltre questa età il dato è vecchio |
| `backlight_pin` | `GPIO21` | verificato sulla scheda |
| `display_driver` | `mipi` | `mipi` oppure `ili9xxx` (fallback) |

Convenzione interna: dopo l'applicazione di `power_invert`, **potenza > 0 = carica**.

### Componenti

- **core.yaml**: `esp32` (`board: esp32dev`, framework ESP-IDF predefinito); `wifi` con credenziali da `secrets` e `ap` di fallback; `api` con `encryption.key` da `secrets`; `ota` (`platform: esphome`, `encryption: {}` che eredita la chiave dell'API); `logger`; backlight come `output: ledc` sul `backlight_pin` più `light: monochromatic` "Retroilluminazione" (`restore_mode: RESTORE_DEFAULT_ON`).
- **data.yaml**: tre `sensor: platform: homeassistant` (`batt_soc`, `batt_power`, `data_age`) collegati alle substitutions; `batt_power` ha il filtro `multiply: ${power_scale}`, così il C++ lavora sempre in W.
- **display-mipi.yaml**: bus `spi` (CLK 14, MOSI 13); `platform: mipi_spi`, `model: ESP32-2432S028-7789` (ST7789, CS 15 e DC 2 nel preset), `color_order: bgr`, `invert_colors: false`, `rotation: 90`, `update_interval: 5s`, lambda che chiama `ui::draw_ui(...)`. Per la variante ILI9341 (FNK0103 F) si usa `model: ESP32-2432S028`.
- **display-ili9xxx.yaml**: bus `spi` (CLK 14, MOSI 13); `platform: ili9xxx`, `model: ST7789V`, `color_order: bgr`, `invert_colors: false`, CS 15, DC 2, stessa lambda.
- **ui.yaml**: font (Roboto 700 a 90 px limitato ai glyph `0123456789%`; Roboto a 20 px), `esphome: includes: [ui_logic.h, ui.h]`.
- **Parametri verso il C++**: `ui.h` non vede le substitutions né gli `id(...)`. La lambda passa tutto esplicitamente: `ui::draw_ui(it, ui::UiInputs{soc, soc_received, power_w, age_min, wifi_ok, api_ok}, ui::UiConfig{${soc_green}, ${soc_yellow}, ${power_deadband_w}, ${power_invert}, ${battery_capacity_kwh}, ${stale_after_min}}, id(font_big), id(font_small));`. La connessione si legge da `wifi::global_wifi_component->is_connected()` e da `api::global_api_server->is_connected()`.
- **ui_logic.h**: logica pura, senza dipendenze da ESPHome: `screen_state()`, `soc_level()`, `power_flow()`, `clamp_soc()`, `available_kwh()`, `bar_fill_px()`, `has_age()`.
- **ui.h**: `draw_ui(Display &it, const UiInputs &in, const UiConfig &cfg, Font *big, Font *small)`, con i colori (costanti `Color`) e le coordinate del layout.

### Layout (320×240, orizzontale)

```
┌──────────────────────────────────────────┐
│ ▲ In carica 2.4 kW            ● WiFi ● HA │  riga stato (20 px)
│                  87%                     │  90 px bold, colore soglia
│ ┌──────────────────────────────────────┐ │
│ │██████████████████████████████░░░░░░░│ │  barra, stesso colore
│ └──────────────────────────────────────┘ │
│ 26.1 kWh                   agg. 12 min fa │  riga info (20 px)
└──────────────────────────────────────────┘
```

Riga di stato:

| Condizione | Resa |
|---|---|
| potenza > deadband | ▲ "In carica X.X kW", verde |
| potenza < −deadband | ▼ "In scarica X.X kW", giallo |
| entro la deadband | "Ferma", bianco |
| potenza NaN / assente | lato sinistro vuoto |

▲▼● sono disegnati con primitive grafiche (`filled_triangle`, `filled_circle`), quindi non dipendono dai glyph del font. I testi sono in ASCII (niente "…" né lettere accentate).

### Key Flows: selezione dello stato (priorità decrescente)

| # | Condizione | Resa |
|---|---|---|
| 1 | SOC mai ricevuto, oppure NaN (`unavailable`) | solo testo centrato: "In attesa dati...", oppure "HA non connesso" / "WiFi non connesso" se manca la connessione (dopo il reboot automatico di ESPHome per timeout di 15 min non ci sono ultimi valori), oppure "Dati non disponibili" |
| 2 | `!wifi.connected` oppure `!api.connected` | ultimi valori **in grigio**, pallino WiFi/HA rosso, in basso a destra "HA non connesso" |
| 3 | `data_age > stale_after_min` | percentuale, barra, riga di stato e kWh **in grigio**, "agg. N min fa" in giallo |
| 4 | altrimenti | normale, colori per soglia, pallini verdi |

Lo stato 2 MUST essere rilevato dall'ESP: con l'API scollegata `data_age` smette di aggiornarsi. Se `data_age` è NaN l'età non viene mostrata e lo stato 3 non si applica.

### Home Assistant

**Template sensor dell'età del dato** (`homeassistant/templates/solaredge_data_age.yaml`), si ricalcola ogni minuto perché usa `now()`:

```yaml
template:
  - sensor:
      - name: "SolarEdge data age"
        unique_id: solaredge_data_age
        unit_of_measurement: "min"
        state: >
          {{ ((now() - states.sensor.solaredge_storage_level.last_reported)
              .total_seconds() / 60) | round(0) }}
```

Presupposto da verificare (vedi Test Strategy, I3): `last_reported` si aggiorna a ogni lettura dell'integrazione cloud anche quando il valore non cambia.

**Automazione della retroilluminazione** (`homeassistant/automations/display-garage-backlight.yaml`):
- Trigger "on": `binary_sensor.<reolink>_motion` passa a `on` → `light.turn_on` sulla retroilluminazione.
- Trigger "off": lo stesso sensore resta `off` per 5 minuti → `light.turn_off`.
- `mode: restart`.
- L'entità della telecamera è un segnaposto.

### Security

- `esphome/secrets.yaml` MUST NOT essere committato (è in `.gitignore`); nel repo c'è solo `secrets.yaml.example` con `wifi_ssid`, `wifi_password`, `api_encryption_key`, `ap_password`.
- L'API ESPHome MUST usare la chiave di cifratura; l'OTA usa la stessa chiave (`encryption: {}`).
- L'accesso di Claude a HA avviene con un long-lived token fornito dal proprietario: in sola lettura per le verifiche; ogni scrittura (template, automazioni) MUST essere approvata prima. Il token MUST NOT essere scritto nel repo.

## Impact Analysis

Greenfield: sezione omessa.

## Test Strategy

La logica pura (`ui_logic.h`) è coperta da `tests/test_ui_logic.cpp`, eseguito sul PC: gli Edge Cases 1–6 e la priorità degli stati negli Error Cases sono test automatici. Il resto si verifica con config e compilazione di entrambi i driver (`scripts/check.sh`, anche in CI), controlli su HA e la checklist sull'hardware.

### Happy Path

| # | Scenario | Input | Expected Output |
|---|---|---|---|
| 1 | Config valida (mipi) | `esphome config esphome/display-garage.yaml` | exit 0 |
| 2 | Compilazione (mipi) | `esphome compile esphome/display-garage.yaml` | firmware generato |
| 3 | Config + compilazione (fallback) | stessi comandi con `-s display_driver ili9xxx` | exit 0, firmware generato |
| 4 | CI | push su GitHub | workflow verde su entrambe le varianti |
| 5 | Stato normale sulla scheda | SOC 87%, carica 2.4 kW, età 5 min | layout come da mockup, verde, ▲ |

### Edge Cases

| # | Scenario | Input | Expected Output |
|---|---|---|---|
| 1 | Soglie di colore | SOC 60 / 59.9 / 30 / 29.9 | verde / giallo / giallo / rosso |
| 2 | Deadband | potenza +50 / +51 / −51 W | Ferma / In carica 0.1 / In scarica 0.1 |
| 3 | `power_invert: true` | potenza +2000 W | "In scarica 2.0 kW" |
| 4 | SOC 0 / 100 | — | barra vuota / piena, nessun overflow grafico |
| 5 | SOC fuori range (glitch) | 101 / −1 | valore limitato a 0–100 per barra e kWh |
| 6 | Età al limite | `data_age` = 30 / 31, soglia 30 | normale / dato vecchio |

### Error Cases

| # | Scenario | Input | Expected Error |
|---|---|---|---|
| 1 | Boot senza dati | nessun SOC ricevuto | stato 1 "In attesa dati…" |
| 2 | SOC `unavailable` in HA | NaN | stato 1 "Dati non disponibili" |
| 3 | API HA scollegata | device disabilitato in HA | stato 2, pallino HA rosso |
| 4 | Wi-Fi assente | AP irraggiungibile | stato 2, pallino WiFi rosso |
| 5 | Dato vecchio | `stale_after_min: 1` temporaneo | stato 3, età in giallo |
| 6 | Potenza assente | `power_entity` inesistente | riga di stato sinistra vuota, resto normale |

### Integration Tests

| # | Scenario | Infrastruttura | Verifica |
|---|---|---|---|
| I1 | `entity_id` reali | HA (token in lettura) | le entità SOC e potenza della batteria esistono e hanno unità % e W |
| I2 | Segno della potenza | HA più app SolarEdge durante carica e scarica | si imposta `power_invert` |
| I3 | `last_reported` | HA, cronologia dell'entità SOC con valore stabile | si aggiorna circa ogni 15 minuti anche senza cambi di valore |
| I4 | Automazione retroilluminazione | HA più telecamera Reolink | si accende al movimento e si spegne dopo 5 minuti |
| H1 | Primo flash USB | scheda collegata al PC | `esphome run` va a buon fine, log seriale OK |
| H2 | Retroilluminazione su GPIO21 | scheda | schermo acceso, dimmer da HA funzionante |
| H3 | Modello e colori | scheda | immagine pulita, colori corretti (controller dall'etichetta: B = ST7789, F = ILI9341; poi `invert_colors`/`color_order`; poi fallback `ili9xxx`) |
| H4 | Rotazione | scheda | orizzontale e dritto |
| H5 | Leggibilità | scheda a parete, 2–3 m | percentuale leggibile |
| H6 | OTA | scheda su Wi-Fi | secondo flash via rete riuscito |

## Implementation Order

1. Tooling: venv con CLI ESPHome, `secrets.yaml` locale a partire dall'esempio
2. `core.yaml` + `display-mipi.yaml` + un `ui.h` minimale ("hello") → config e compile verdi
3. `data.yaml` + `ui.yaml` + `ui.h` completo (stati 1–4, layout) → compile verde
4. Fallback `display-ili9xxx.yaml` selezionabile con `display_driver` → compile verde
5. Workflow GitHub Actions
6. File HA (template dell'età, automazione della retroilluminazione)
7. README completo (primo flash, adattamento degli `entity_id`, troubleshooting)
8. Verifiche su HA I1–I3 (dopo l'ok del proprietario), aggiornamento dei default
9. Verifiche sull'hardware H1–H6 all'arrivo della scheda, poi test manuali degli Edge/Error Cases

## Risks and Mitigations

| Rischio | Impatto | Mitigazione |
|---|---|---|
| `mipi_spi` dà immagini corrotte sulla CYD 2,8" | display inutilizzabile | package di fallback `ili9xxx` già compilato in CI |
| Variante del controller diversa da quella attesa | immagine corrotta | verificata: FNK0103 B = ST7789; la variante F (ILI9341) è documentata nel package e nel README |
| `last_reported` non si aggiorna senza cambio di valore | falso "dato vecchio" | verifica I3; alternativa: età basata sull'attributo o sul sensore di ultimo aggiornamento dell'integrazione |
| Cloud SolarEdge fuori servizio | entità `unavailable` | stato 1 o 3, mai un valore fuorviante in colore pieno |
| Breaking change di ESPHome | la build fallisce | CI su ogni push; versione di ESPHome annotata nel README |

## Open Questions

- `entity_id` reali e segno della potenza → I1–I2
- Entità del sensore di movimento Reolink → I4

## Implementation Notes (2026-09-25)

- **Scheda reale**: l'etichetta riporta "Freenove ESP32 Display FNK0103 B A1B0 — 2.8 Inch ST7789 TN 240x320 Touch". esptool: ESP32-D0WD-V3 rev 3.1, flash da 4 MB, niente PSRAM, CH340 sulla COM5.
- **Primo flash con il preset ILI9341**: immagine ripetuta e schiacciata, schermo aggiornato solo in parte, fondo chiaro. Con `ESP32-2432S028-7789` + `color_order: bgr` + `invert_colors: false` (coerente con il setup TFT_eSPI di Freenove FNK0114B) l'immagine è corretta.
- **Esiti hardware**: H1 ✔ (flash USB), H2 ✔ (retroilluminazione su GPIO21), H3 ✔, H4 ✔ (orizzontale, pallini verde e rosso corretti), H6 ✔ (OTA verso `display-garage.local`). Restano H5 (leggibilità a parete) e il controllo del dimmer da HA.
- **Toolchain Windows**: l'installer ESP-IDF rifiuta MSYS (Git Bash) e fallisce su percorsi lunghi. `scripts/check.sh` imposta `ESPHOME_ESP_IDF_PREFIX=C:/ESPHome/idf` e toglie `MSYSTEM`.
- **Layout**: pallini WiFi/HA spostati di 10 px a sinistra perché "HA" toccava il bordo destro.
