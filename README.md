# CYD FV Battery – Display Garage

Display a parete (Freenove ESP32 Display FNK0103 B, 2,8" ST7789, con ESPHome) che mostra lo stato
dell'accumulo fotovoltaico SolarEdge letto da Home Assistant: percentuale, barra, kWh disponibili,
carica/scarica con potenza, età del dato e stato della connessione. Serve a decidere al volo se
mettere in carica l'auto elettrica.

```
Inverter SolarEdge ──(cloud, ~15 min)──> Home Assistant ──(API ESPHome)──> CYD 2,8"
```

Design: [`specs/2026-09-25-display-garage-design.md`](specs/2026-09-25-display-garage-design.md)

## Schermata

```
┌──────────────────────────────────────────┐
│ ▲ In carica 2.4 kW           ● WiFi ● HA  │
│                  87%                     │  verde ≥60%, giallo ≥30%, rosso sotto
│ [██████████████████████████████░░░░░░░]  │
│ 26.1 kWh                   agg. 12 min fa │
└──────────────────────────────────────────┘
```

| Situazione | Cosa si vede |
|---|---|
| Nessun dato dall'avvio | "In attesa dati..." (se il pallino HA è verde: `entity_id` sbagliato) |
| SOC `unavailable` in HA | "Dati non disponibili" |
| Wi-Fi o HA scollegati | valori in grigio, pallino rosso, "HA non connesso" / "WiFi non connesso" |
| Dato più vecchio di 30 min | valori in grigio, "agg. N min fa" in giallo |

## Hardware

| | |
|---|---|
| Scheda | Freenove ESP32 Display **FNK0103 B** (ESP32-D0WD-V3, 4 MB flash, no PSRAM, USB-C, CH340) |
| Pannello | 2,8" TN 240×320, controller **ST7789** (ordine colori BGR, nessuna inversione) |
| Pin | SPI CLK 14, MOSI 13, CS 15, DC 2; retroilluminazione GPIO21 |

La variante **FNK0103 F** ha lo stesso pinout ma controller ILI9341: in quel caso usare
`model: ESP32-2432S028` in `packages/display-mipi.yaml`.

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
- Driver USB-seriale CH340 (WCH `CH341SER`) per il primo flash
- Per i test della logica: g++ oppure Docker
- Windows: la toolchain ESP-IDF non funziona in percorsi lunghi né sotto MSYS. `scripts/check.sh`
  gestisce entrambe le cose (usa `C:/ESPHome/idf`); lanciando `esphome` da PowerShell impostare
  prima `$env:ESPHOME_ESP_IDF_PREFIX = 'C:\ESPHome\idf'`.

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
py -3.14 -m venv .venv
.venv/Scripts/pip install -r requirements.txt
cp esphome/secrets.yaml.example esphome/secrets.yaml   # poi inserire Wi-Fi reale e generare la chiave API
.venv/Scripts/esphome run esphome/display-garage.yaml --device COM5   # porta del CH340
```

Poi in Home Assistant: *Impostazioni → Dispositivi → ESPHome* → aggiungere "Display Garage"
con la `api_encryption_key` di `secrets.yaml`. Gli aggiornamenti successivi vanno via OTA
(`--device display-garage.local`), cifrati con la stessa chiave.

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
| Immagine ripetuta/schiacciata, parte dello schermo non aggiornata, fondo chiaro | controller sbagliato (ILI9341 su pannello ST7789 o viceversa) | verificare la variante sull'etichetta (B = ST7789, F = ILI9341) e il `model` in `display-mipi.yaml` |
| Schermo nero, retroilluminazione spenta | pin backlight errato o luce spenta da HA | verificare `backlight_pin` (GPIO21), accendere "Retroilluminazione" in HA |
| Colori al negativo | inversione colori | invertire `invert_colors` in `packages/display-mipi.yaml` |
| Rosso e blu scambiati | ordine colori | `color_order: rgb` al posto di `bgr` |
| Immagine capovolta | rotazione | `rotation: 270` |
| Immagine corrotta con `mipi_spi` | driver | fallback `display_driver: ili9xxx` |
| "In attesa dati..." con pallino HA verde | `entity_id` inesistente | correggere `soc_entity` |
| Età sempre assente | template non creato | vedi Setup HA punto 1 |
| Carica/scarica invertite | segno della potenza | `power_invert: "true"` |
| Build Windows: `MSys/Mingw is not supported` o `bits/c++config.h` mancante | MSYS / percorso lungo | usare `scripts/check.sh` o impostare `ESPHOME_ESP_IDF_PREFIX` |
