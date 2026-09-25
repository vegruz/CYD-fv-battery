# CLAUDE.md

## Descrizione del progetto

Display a parete in garage (Freenove CYD 2,8", ESP32 + ILI9341, 320×240) che mostra lo stato
dell'accumulo fotovoltaico SolarEdge (30 kWh) letto da Home Assistant: SOC %, barra, kWh
disponibili, carica/scarica con potenza, età del dato e stato connessione. Retroilluminazione
comandata da HA su movimento della telecamera Reolink.

Spec di riferimento: `specs/2026-09-25-display-garage-design.md`. Handoff originale: `docs/display-garage-cyd.md`.

## Stack tecnologico

- ESPHome (framework ESP-IDF, default dal 2026.1), piattaforma display `mipi_spi` (preset `ESP32-2432S028`); fallback `ili9xxx` (deprecata)
- C++ per il rendering (`esphome/ui.h`), YAML per config e package
- Home Assistant: integrazione SolarEdge **cloud** (~15 min di latenza), template sensor, automazioni
- GitHub Actions per config/compile

## Skills attive

- superpowers (brainstorming, writing-plans, executing-plans, verification-before-completion)
- new-spec, final-check

## Comandi

- Setup: `python -m venv .venv && .venv/Scripts/pip install esphome`
- Validazione: `.venv/Scripts/esphome config esphome/display-garage.yaml`
- Build: `.venv/Scripts/esphome compile esphome/display-garage.yaml` (e `display-garage-ili9xxx.yaml`)
- Flash/log: `.venv/Scripts/esphome run esphome/display-garage.yaml` / `esphome logs ...`

## Vincoli di progetto

- Solo scheda CYD 2,8". Niente Modbus, niente LVGL/touch, niente consiglio "CARICA ORA" (non-goals della spec).
- Il driver display sta solo in `packages/display-*.yaml`; il rendering solo in `ui.h`. I package driver chiamano `draw_ui(it, UiConfig{...})`.
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

- Ogni modifica MUST passare `esphome config` e `esphome compile` su `display-garage.yaml` e `display-garage-ili9xxx.yaml` (anche in CI).
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
