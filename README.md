# CYD FV Battery – Display Garage

Display a parete (ESP32 "Cheap Yellow Display" + ESPHome) che mostra in tempo reale lo stato
dell'accumulo fotovoltaico SolarEdge letto da Home Assistant: percentuale, kWh disponibili,
carica/scarica con potenza. Serve a decidere al volo se mettere in carica l'auto elettrica.

```
Inverter SolarEdge ──> Home Assistant ──(API nativa ESPHome)──> CYD con ESPHome
```

> **Stato:** in progettazione. Contesto e requisiti in [`docs/display-garage-cyd.md`](docs/display-garage-cyd.md).

## Hardware supportato

| Scheda | Pannello | Driver | Config |
|---|---|---|---|
| Freenove CYD 3,2" | IPS | ST7789 | `esphome/display-garage-32.yaml` |
| Freenove CYD 2,8" | TN | ILI9341 | `esphome/display-garage-28.yaml` |

## Struttura

```
esphome/                 # configurazioni ESPHome (una per scheda + package comune)
homeassistant/           # automazioni / template HA di esempio
docs/                    # handoff, spec di design
```

## Quick start

1. Copia `esphome/secrets.yaml.example` in `esphome/secrets.yaml` e compila le credenziali Wi-Fi.
2. Adatta gli `entity_id` SolarEdge nel package comune.
3. Primo flash via USB dall'add-on ESPHome, poi aggiornamenti OTA.

Procedura dettagliata di flash, adattamento `entity_id` e troubleshooting: _in arrivo_.
