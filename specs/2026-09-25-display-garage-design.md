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

Serve un display a parete in garage che mostri lo stato dell'accumulo SolarEdge (30 kWh) letto da Home Assistant, per decidere se mettere in carica l'auto senza aprire l'app. Hardware: un solo Freenove CYD 2,8" (ESP32, ILI9341, 320×240). L'handoff proponeva la piattaforma ESPHome `ili9xxx` con framework Arduino. Dalla verifica sulla documentazione ESPHome corrente (settembre 2026) risulta che:

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
├── display-garage-ili9xxx.yaml    # variante di fallback (usata anche in CI)
├── ui.h                           # rendering C++ condiviso
├── packages/
│   ├── core.yaml                  # esp32 (ESP-IDF), wifi, api, ota, logger, backlight
│   ├── data.yaml                  # sensori homeassistant (SOC, potenza, età dato)
│   ├── display-mipi.yaml          # mipi_spi, preset ESP32-2432S028 (default)
│   ├── display-ili9xxx.yaml       # fallback ili9xxx, stesso id e stessa lambda
│   └── ui.yaml                    # font, colori, include di ui.h
└── secrets.yaml.example
homeassistant/
├── templates/solaredge_data_age.yaml
└── automations/display-garage-backlight.yaml
.github/workflows/esphome.yaml     # config + compile di entrambe le varianti
```

Ogni package ha un solo compito. I package driver MUST esporre un display con `id: disp` e una lambda che chiama `draw_ui(it, cfg)`, così il cambio di driver non tocca il resto.

### Parametri (`substitutions` in `display-garage.yaml`)

| Nome | Default | Significato |
|---|---|---|
| `device_name` | `display-garage` | hostname / nome del nodo |
| `friendly_name` | `Display Garage` | nome mostrato in HA |
| `soc_entity` | `sensor.solaredge_storage_level` | SOC in % (entità segnaposto, da verificare) |
| `power_entity` | `sensor.solaredge_storage_power` | potenza della batteria in W (da verificare) |
| `age_entity` | `sensor.solaredge_data_age` | età del dato in minuti (template HA) |
| `power_invert` | `"false"` | `true` se l'integrazione usa un valore positivo per la scarica |
| `battery_capacity_kwh` | `30` | capacità utile |
| `soc_green` / `soc_yellow` | `60` / `30` | soglie di colore in % |
| `power_deadband_w` | `50` | sotto questo valore assoluto la batteria risulta "Ferma" |
| `stale_after_min` | `30` | oltre questa età il dato è vecchio |
| `backlight_pin` | `GPIO21` | da verificare sulla scheda |

Convenzione interna: dopo l'applicazione di `power_invert`, **potenza > 0 = carica**.

### Componenti

- **core.yaml**: `esp32` (`board: esp32dev`, framework ESP-IDF predefinito); `wifi` con credenziali da `secrets` e `ap` di fallback; `api` con `encryption.key` da `secrets`; `ota` (`platform: esphome`, password da `secrets`); `logger`; backlight come `output: ledc` sul `backlight_pin` più `light: monochromatic` "Retroilluminazione" (`restore_mode: RESTORE_DEFAULT_ON`).
- **data.yaml**: tre `sensor: platform: homeassistant` (`batt_soc`, `batt_power`, `data_age`) collegati alle substitutions.
- **display-mipi.yaml**: `platform: mipi_spi`, `model: ESP32-2432S028`, `rotation: 90`, `update_interval: 5s`, lambda che chiama `draw_ui(...)`. Il preset copre pin e init; `invert_colors` e `color_order` MAY richiedere una correzione dopo la verifica sulla scheda.
- **display-ili9xxx.yaml**: `platform: ili9xxx`, `model: ILI9341` (alternativa `ILI9342`), bus `spi` esplicito (CLK 14, MOSI 13, MISO 12), CS 15, DC 2, stessa lambda.
- **ui.yaml**: font (Roboto 700 a 90 px limitato ai glyph `0123456789%`; Roboto a 20 px), colori, `esphome: includes: [ui.h]`.
- **Parametri verso il C++**: `ui.h` non vede le substitutions. Le lambda dei package driver le passano esplicitamente in una struct, per esempio `draw_ui(it, UiConfig{${soc_green}, ${soc_yellow}, ${power_deadband_w}, ${power_invert}, ${battery_capacity_kwh}, ${stale_after_min}});`. Font, colori e sensori si leggono tramite `id(...)`.
- **ui.h**: `draw_ui(display::Display &it, const UiConfig &cfg)` suddivisa in due parti:
  - `ui_state()`, logica pura che sceglie lo stato (vedi sotto);
  - le funzioni di disegno per ciascuno stato.

### Layout (320×240, orizzontale)

```
┌──────────────────────────────────────────┐
│ ▲ In carica 2.4 kW            ● WiFi ● HA │  riga stato (20 px)
│                  87%                     │  90 px bold, colore soglia
│ ┌──────────────────────────────────────┐ │
│ │██████████████████████████████░░░░░░░│ │  barra, stesso colore
│ └──────────────────────────────────────┘ │
│ 26.1 kWh disponibili       agg. 12 min fa │  riga info (20 px)
└──────────────────────────────────────────┘
```

Riga di stato:

| Condizione | Resa |
|---|---|
| potenza > deadband | ▲ "In carica X.X kW", verde |
| potenza < −deadband | ▼ "In scarica X.X kW", giallo |
| entro la deadband | "Ferma", bianco |
| potenza NaN / assente | lato sinistro vuoto |

Se il font non contiene ▲▼● si SHOULD usare le icone MDI tramite glyph.

### Key Flows: selezione dello stato (priorità decrescente)

| # | Condizione | Resa |
|---|---|---|
| 1 | SOC mai ricevuto, oppure NaN (`unavailable`) | solo testo centrato "In attesa dati…" o "Dati non disponibili" |
| 2 | `!wifi.connected` oppure `!api.connected` | ultimi valori **in grigio**, pallino WiFi/HA rosso, in basso a destra "HA non connesso" |
| 3 | `data_age > stale_after_min` | percentuale e barra **in grigio**, "agg. N min fa" in giallo |
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

- `esphome/secrets.yaml` MUST NOT essere committato (è in `.gitignore`); nel repo c'è solo `secrets.yaml.example` con `wifi_ssid`, `wifi_password`, `api_encryption_key`, `ota_password`, `ap_password`.
- L'API ESPHome MUST usare la chiave di cifratura.
- L'accesso di Claude a HA avviene con un long-lived token fornito dal proprietario: in sola lettura per le verifiche; ogni scrittura (template, automazioni) MUST essere approvata prima. Il token MUST NOT essere scritto nel repo.

## Impact Analysis

Greenfield: sezione omessa.

## Test Strategy

Non c'è logica applicativa testabile con coverage: la verifica si basa su validazione e compilazione della config, controlli su HA e una checklist sull'hardware.

### Happy Path

| # | Scenario | Input | Expected Output |
|---|---|---|---|
| 1 | Config valida (mipi) | `esphome config esphome/display-garage.yaml` | exit 0 |
| 2 | Compilazione (mipi) | `esphome compile esphome/display-garage.yaml` | firmware generato |
| 3 | Config + compilazione (fallback) | stessi comandi su `display-garage-ili9xxx.yaml` | exit 0, firmware generato |
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
| H3 | Modello e colori | scheda | immagine pulita, colori corretti (altrimenti si passa a `invert_colors`/`color_order`, poi a `-9342`, poi al fallback `ili9xxx`) |
| H4 | Rotazione | scheda | orizzontale e dritto |
| H5 | Leggibilità | scheda a parete, 2–3 m | percentuale leggibile |
| H6 | OTA | scheda su Wi-Fi | secondo flash via rete riuscito |

## Implementation Order

1. Tooling: venv con CLI ESPHome, `secrets.yaml` locale a partire dall'esempio
2. `core.yaml` + `display-mipi.yaml` + un `ui.h` minimale ("hello") → config e compile verdi
3. `data.yaml` + `ui.yaml` + `ui.h` completo (stati 1–4, layout) → compile verde
4. Fallback `display-ili9xxx.yaml` + `display-garage-ili9xxx.yaml` → compile verde
5. Workflow GitHub Actions
6. File HA (template dell'età, automazione della retroilluminazione)
7. README completo (primo flash, adattamento degli `entity_id`, troubleshooting)
8. Verifiche su HA I1–I3 (dopo l'ok del proprietario), aggiornamento dei default
9. Verifiche sull'hardware H1–H6 all'arrivo della scheda, poi test manuali degli Edge/Error Cases

## Risks and Mitigations

| Rischio | Impatto | Mitigazione |
|---|---|---|
| `mipi_spi` dà immagini corrotte sulla CYD 2,8" | display inutilizzabile | package di fallback `ili9xxx` già compilato in CI |
| Pinout Freenove diverso dallo standard ESP32-2432S028 | schermo nero | verifica H1–H3 sulla scheda, pin ridefinibili nel package |
| `last_reported` non si aggiorna senza cambio di valore | falso "dato vecchio" | verifica I3; alternativa: età basata sull'attributo o sul sensore di ultimo aggiornamento dell'integrazione |
| Cloud SolarEdge fuori servizio | entità `unavailable` | stato 1 o 3, mai un valore fuorviante in colore pieno |
| Breaking change di ESPHome | la build fallisce | CI su ogni push; versione di ESPHome annotata nel README |
| Glyph ▲▼● assenti in Roboto | simboli non visualizzati | icone MDI tramite glyph |

## Open Questions

- Pinout effettivo della Freenove 2,8" (backlight GPIO21, preset `ESP32-2432S028` compatibile?) → da risolvere con H1–H3
- `entity_id` reali e segno della potenza → I1–I2
- Entità del sensore di movimento Reolink → I4
