# CLAUDE.md

## Descrizione del progetto

Display a parete in garage (Freenove ESP32 Display FNK0103 B: ESP32 + pannello 2,8" TN ST7789, 320×240) che mostra lo stato
dell'accumulo fotovoltaico SolarEdge (30 kWh) letto da Home Assistant: SOC %, barra, kWh
disponibili, carica/scarica con potenza, età del dato e stato connessione. Retroilluminazione
comandata da HA su movimento della telecamera Reolink.

Spec di riferimento: `specs/2026-09-25-display-garage-design.md`. Handoff originale: `docs/display-garage-cyd.md`.

## Stack tecnologico

- ESPHome 2026.9.0 (framework ESP-IDF), piattaforma display `mipi_spi` (preset `ESP32-2432S028-7789`, BGR, no inversione); fallback `ili9xxx` (deprecata)
- C++: logica pura `esphome/ui_logic.h` (testata su PC con g++/Docker), disegno `esphome/ui.h`; YAML per config e package
- Home Assistant: integrazione SolarEdge **cloud** (~15 min di latenza), template sensor, automazioni
- GitHub Actions per config/compile

## Skills attive

- superpowers (brainstorming, writing-plans, executing-plans, verification-before-completion)
- new-spec, final-check

## Comandi

- Setup: `py -3.14 -m venv .venv && .venv/Scripts/pip install -r requirements.txt`
- Verifica completa (test logica + config/compile di entrambi i driver): `sh scripts/check.sh`
- Solo test logica UI (g++ o Docker `gcc:14`; Docker Desktop deve essere avviato): `sh scripts/test-logic.sh`
- Singolo driver: `.venv/Scripts/esphome.exe -s display_driver ili9xxx compile esphome/display-garage.yaml`
- Flash USB: `.venv/Scripts/esphome.exe run esphome/display-garage.yaml --device COM5`; OTA: `--device display-garage.local`
- Windows: da PowerShell impostare prima `$env:ESPHOME_ESP_IDF_PREFIX = 'C:\ESPHome\idf'` (percorsi lunghi); da Git Bash ci pensa `scripts/check.sh` (toglie anche `MSYSTEM`, rifiutato dall'installer ESP-IDF)

## Vincoli di progetto

- Solo scheda CYD 2,8". Niente Modbus, niente LVGL/touch, niente consiglio "CARICA ORA" (non-goals della spec).
- Il driver display sta solo in `packages/display-*.yaml` (scelto con la substitution `display_driver`); logica pura in `ui_logic.h`, disegno in `ui.h`. Le due lambda dei driver MUST restare identiche.
- Parametri (entità, soglie, capacità, segno potenza) solo come `substitutions` in `display-garage.yaml`, mai hardcoded nel C++.
- L'età del dato si calcola in HA (`last_reported`), non sull'ESP: i sensori `homeassistant` arrivano solo al cambio di stato.
- `esphome/secrets.yaml` non va mai committato.
- Home Assistant: accesso con long-lived token (mai nel repo). Lettura libera; ogni scrittura (template, automazioni, entità) va mostrata e approvata prima.
- La documentazione ESPHome cambia spesso: verificare la sintassi sulla doc corrente prima di usarla.

## Regole di stesura delle specifiche

- NON prendere decisioni di design autonomamente. Presenta le alternative e aspetta indicazioni.
- Segnala incongruenze o ambiguità prima di procedere.
- Usa vocabolario RFC 2119 nei requisiti:
  - **MUST** = obbligatorio
  - **SHOULD** = raccomandato
  - **MAY** = opzionale

## Regole operative

- Lavora solo sul file o feature richiesta. Non modificare altro senza conferma esplicita.
- NON prendere decisioni implementative autonomamente. Se ci sono alternative, presentale e aspetta indicazioni.
- Verifica dopo ogni implementazione (config + compile di entrambe le varianti), prima di considerare la feature completata.

## Criteri di qualità dei test

- Ogni modifica MUST passare `sh scripts/check.sh` (test logica + config/compile di `mipi` e `ili9xxx`, anche in CI). Ogni modifica a soglie o stati MUST avere un caso in `tests/test_ui_logic.cpp`.
- Edge case ed error case della spec (tabellari) verificati a mano sulla scheda; soglie testate sui valori limite.
- Verifiche su HA (entity_id, segno potenza, `last_reported`) documentate con esito nella spec.
- Nessuna feature si considera completata senza evidenza (output dei comandi o verifica sulla scheda).

## Template e standard per la documentazione

| Tipo documento | Standard | Template |
|---|---|---|
| Decisione architetturale pura (senza implementazione) | MADR v3 | `specs/templates/adr.md` |
| Feature, modifica, o componente nuovo (con o senza decisione) | Design Doc stile Google | `specs/templates/design-doc.md` |

**Linguaggio**: RFC 2119 (MUST, SHOULD, MAY) quando serve precisione nei requisiti.

**Flusso per nuove feature/modifiche**:

1. Valuta se serve un ADR separato o un Design Doc autosufficiente.
2. Scrivi il documento col template appropriato.
3. Fai approvare prima di implementare.
4. Implementa seguendo Impact Analysis, Test Strategy, Implementation Order.
