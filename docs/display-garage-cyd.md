# Display Garage – Stato accumulo FV su CYD (ESP32 + ESPHome)

Documento di handoff per Claude Code. Contiene contesto, hardware, stack, configurazioni di partenza e attività da svolgere.

---

## 1. Obiettivo

Display a parete in garage che mostra in tempo reale lo stato dell'accumulo del fotovoltaico SolarEdge, per decidere al volo se mettere in carica l'auto elettrica o aspettare, senza aprire l'app SolarEdge.

Requisiti:

- percentuale di carica ben leggibile a 2–3 metri, con colore in base alla soglia (verde ≥ 60%, giallo ≥ 30%, rosso sotto)
- barra di avanzamento
- kWh disponibili (accumulo da **30 kWh**)
- stato dell'accumulo: in carica / in scarica / fermo, con potenza in kW
- retroilluminazione controllabile da Home Assistant (spegnimento notturno o accensione su evento)
- soluzione low cost, nessuna saldatura

---

## 2. Hardware

Due schede in prova, da confrontare sul posto (angolo di visione e leggibilità). Il firmware deve supportarle entrambe.

| | Freenove CYD 3,2" | Freenove CYD 2,8" |
|---|---|---|
| Pannello | IPS | TN |
| Driver | ST7789 | ILI9341 |
| Risoluzione | 240×320 | 240×320 |
| Retroilluminazione | GPIO27 (da verificare) | GPIO21 |
| `invert_colors` | probabilmente `true` | `false` |
| Touch | resistivo (non usato per ora) | resistivo XPT2046 (non usato per ora) |

Pin comuni (piedinatura standard ESP32-2432S028R, da verificare sul tutorial Freenove):

- SPI display: CLK GPIO14, MOSI GPIO13, MISO GPIO12
- CS GPIO15, DC GPIO2

Alimentazione: alimentatore USB 5V/1A. Case: da stampa 3D (MakerWorld/Printables hanno molti case per CYD).

Troubleshooting noto:

- schermo nero con retroilluminazione spenta → pin backlight sbagliato
- colori al negativo → invertire `invert_colors`
- schermo bianco sulla 3,2" → controllare `model` (ST7789V)
- immagine disturbata / colori sballati sulla 2,8" → provare `model: ILI9342`
- ESPHome sta spingendo la piattaforma `mipi_spi` al posto di `ili9xxx`; secondo alcune segnalazioni della community su CYD 2,8" dà immagini corrotte, quindi partire con `ili9xxx` e valutare `mipi_spi` solo se `ili9xxx` viene deprecato

---

## 3. Stack

```
Inverter SolarEdge ──> Home Assistant ──(API nativa ESPHome)──> CYD con ESPHome
```

- **Home Assistant**: già presente, con i dati SolarEdge già integrati.
- **ESPHome**: add-on di HA. Primo flash via USB, poi OTA.
- **Sorgente dati SolarEdge**, punto aperto da verificare:
  - integrazione cloud ufficiale → aggiornamento circa ogni 15 minuti (limite richieste API), non adatto al "tempo reale"
  - **SolarEdge Modbus Multi** (HACS) via Modbus TCP locale → aggiornamento ogni pochi secondi, indipendente dal cloud. Richiede Modbus TCP abilitato sull'inverter tramite SetApp. È la soluzione consigliata.

Gli `entity_id` nel codice sono segnaposto: vanno sostituiti con quelli reali dell'integrazione in uso. Il segno della potenza batteria (positivo = carica o scarica) dipende dall'integrazione e va verificato.

---

## 4. Struttura proposta del repo

```
esphome/
├── display-garage-32.yaml     # scheda 3,2" IPS ST7789
├── display-garage-28.yaml     # scheda 2,8" TN ILI9341
├── packages/
│   └── display-garage-common.yaml
└── secrets.yaml.example
homeassistant/
└── automations/
    └── display-garage-backlight.yaml
```

La logica e il layout stanno nel package comune; i file per scheda definiscono solo le `substitutions` specifiche dell'hardware.

---

## 5. Configurazioni di partenza

### 5.1 `esphome/display-garage-32.yaml`

```yaml
substitutions:
  device_name: display-garage-32
  friendly_name: Display Garage 3.2
  display_model: ST7789V
  backlight_pin: GPIO27
  invert_colors: "true"

packages:
  common: !include packages/display-garage-common.yaml
```

### 5.2 `esphome/display-garage-28.yaml`

```yaml
substitutions:
  device_name: display-garage-28
  friendly_name: Display Garage 2.8
  display_model: ILI9341      # fallback: ILI9342
  backlight_pin: GPIO21
  invert_colors: "false"

packages:
  common: !include packages/display-garage-common.yaml
```

### 5.3 `esphome/packages/display-garage-common.yaml`

```yaml
esphome:
  name: ${device_name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32dev
  framework:
    type: arduino

logger:
api:
ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

spi:
  clk_pin: GPIO14
  mosi_pin: GPIO13
  miso_pin: GPIO12

output:
  - platform: ledc
    pin: ${backlight_pin}
    id: backlight_pwm

light:
  - platform: monochromatic
    output: backlight_pwm
    name: "Retroilluminazione"
    id: backlight
    restore_mode: ALWAYS_ON

sensor:
  - platform: homeassistant
    id: batt_soc
    entity_id: sensor.solaredge_battery_state_of_charge   # DA ADATTARE
  - platform: homeassistant
    id: batt_power
    entity_id: sensor.solaredge_battery_power             # DA ADATTARE (W, verificare il segno)

font:
  - file: "gfonts://Roboto@700"
    id: font_big
    size: 90
  - file: "gfonts://Roboto"
    id: font_small
    size: 20

color:
  - id: verde
    hex: "22C55E"
  - id: giallo
    hex: "EAB308"
  - id: rosso
    hex: "EF4444"
  - id: grigio
    hex: "404040"
  - id: bianco
    hex: "E5E5E5"

display:
  - platform: ili9xxx
    model: ${display_model}
    cs_pin: GPIO15
    dc_pin: GPIO2
    invert_colors: ${invert_colors}
    rotation: 90            # orizzontale, 320x240
    update_interval: 5s
    lambda: |-
      if (!id(batt_soc).has_state() || isnan(id(batt_soc).state)) {
        it.print(160, 120, id(font_small), id(bianco), TextAlign::CENTER, "In attesa dati...");
        return;
      }
      float soc = id(batt_soc).state;
      Color c = soc >= 60 ? id(verde) : (soc >= 30 ? id(giallo) : id(rosso));

      // Stato carica/scarica (soglia 50 W per ignorare il rumore)
      if (id(batt_power).has_state() && !isnan(id(batt_power).state)) {
        float p = id(batt_power).state / 1000.0;
        if (p > 0.05)       it.printf(160, 18, id(font_small), id(verde), TextAlign::CENTER, "In carica  %.1f kW", p);
        else if (p < -0.05) it.printf(160, 18, id(font_small), id(giallo), TextAlign::CENTER, "In scarica  %.1f kW", -p);
        else                it.print(160, 18, id(font_small), id(bianco), TextAlign::CENTER, "Ferma");
      }

      // Percentuale grande
      it.printf(160, 100, id(font_big), c, TextAlign::CENTER, "%.0f%%", soc);

      // Barra
      it.rectangle(20, 165, 280, 30, id(grigio));
      it.filled_rectangle(22, 167, (int)(276 * soc / 100.0), 26, c);

      // Energia disponibile (accumulo da 30 kWh)
      it.printf(160, 220, id(font_small), id(bianco), TextAlign::CENTER, "%.1f kWh disponibili", 30.0 * soc / 100.0);
```

### 5.4 `esphome/secrets.yaml.example`

```yaml
wifi_ssid: "NOME_RETE"
wifi_password: "PASSWORD"
```

---

## 6. Attività per Claude Code

1. Creare la struttura del repo come al punto 4, con i file del punto 5.
2. Verificare che la configurazione sia valida per la versione corrente di ESPHome (sintassi `ota`, `packages`, `substitutions` dentro i package, piattaforma `ili9xxx` vs `mipi_spi`, `framework`).
3. Rendere parametrico nel package comune ciò che oggi è hardcoded: capacità accumulo (30 kWh), soglie colore (60/30%), soglia potenza (50 W), segno della potenza batteria (flag per invertirlo).
4. Aggiungere un indicatore di dato vecchio: se il sensore SOC non si aggiorna da più di N minuti, mostrare un avviso a schermo (es. puntino o testo grigio) invece di un valore potenzialmente fuorviante.
5. Aggiungere un indicatore di connessione (Wi-Fi / API HA disconnessa).
6. Creare in `homeassistant/automations/` un'automazione di esempio per la retroilluminazione: spenta di notte e/o accesa per qualche minuto su un evento (apertura basculante o sensore di movimento; entità segnaposto).
7. Opzionale: una riga di consiglio "CARICA ORA" / "ASPETTA" basata su una regola configurabile (SOC, fascia oraria, produzione FV in corso). La logica è preferibile calcolarla in HA con un template sensor ed esporla al display come testo, così il firmware resta semplice.
8. Opzionale, fase successiva: migrazione del layout a LVGL in ESPHome, per sfruttare il touch (es. pagina di dettaglio o comandi come avvio/stop ricarica).
9. Scrivere un README con procedura di primo flash, troubleshooting (sezione 2) e come adattare gli `entity_id`.

## 7. Punti aperti

- `entity_id` reali e integrazione SolarEdge in uso (cloud o Modbus locale)
- segno della potenza batteria nell'integrazione in uso
- pin effettivi delle schede Freenove (in particolare backlight della 3,2")
- quale delle due schede resta in garage dopo il confronto
