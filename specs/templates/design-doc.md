# [Nome componente/feature] — Design Document

<!-- Formato: Design Doc (stile Google) + estensioni operative -->
<!-- Linguaggio: usare RFC 2119 (MUST, SHOULD, MAY) dove serve precisione -->

## Meta

| Campo | Valore |
|---|---|
| Author | |
| Status | Draft | In Review | Approved | Implemented |
| Date | YYYY-MM-DD |
| ADR correlato | <!-- link ad ADR esterno, OPPURE omettere se la decisione è nella sezione sotto --> |

## Decision

<!-- OPZIONALE: compilare se il documento è autosufficiente (decisione + implementazione).
     Omettere e linkare un ADR nel campo Meta se la decisione è in un documento separato. -->

### Context and Problem Statement

<!-- Perché serve questa modifica? Qual è il problema? -->

### Decision Drivers

- <!-- [driver 1] -->

### Considered Options

1. [Opzione A]
2. [Opzione B]

### Decision Outcome

<!-- Quale opzione è stata scelta e perché. -->

Chosen option: "[Opzione X]", because [giustificazione].

### Consequences

- Good, because [impatto positivo]
- Bad, because [impatto negativo]

---

## Context

<!-- Qual è il problema che questo componente/feature risolve?
     Perché adesso? Qual è il contesto di business o tecnico?
     Se la sezione Decision è compilata, qui si descrive il contesto tecnico/implementativo. -->

## Goals and Non-goals

### Goals

<!-- Cosa DEVE fare questo componente. Essere specifici e misurabili dove possibile. -->

- <!-- [goal 1] -->

### Non-goals

<!-- Cosa NON è nello scope. Importante per evitare scope creep. -->

- <!-- [non-goal 1] -->

## Design

<!-- La sezione principale. Strutturare in sotto-sezioni in base al tipo di componente.
     Le sotto-sezioni qui sotto sono suggerimenti — adattare al contesto. -->

### API

<!-- Endpoint, contratti, esempi di request/response. -->

### Data Model

<!-- Entity, relazioni, tabelle. Diagrammi se utili. -->

### Key Flows

<!-- Flussi principali (happy path + errori significativi).
     Usare sequenze numerate o diagrammi Mermaid. -->

### Security

<!-- Autenticazione, autorizzazione, validazione input, dati sensibili. -->

## Impact Analysis

<!-- Analisi dei file e artefatti impattati dalla modifica.
     Omettere per componenti nuovi (greenfield). -->

### Specifications

<!-- Spec e documenti da aggiornare. -->

| File | Modifica |
|---|---|

### Code

<!-- File di codice impattati, raggruppati per artefatto. -->

| Artefatto | File | Modifica | Tipo |
|---|---|---|---|

### Tests

<!-- File di test impattati (logica che cambia vs solo dati di test). -->

| Artefatto | File | Modifica | Note |
|---|---|---|---|

## Test Strategy

<!-- Test case specifici di questa feature. Le regole generali (naming, tooling, ecc.)
     sono in CLAUDE.md § "Convenzioni di test". Qui si definisce COSA testare. -->

### Happy Path

<!-- Scenari principali che DEVONO funzionare. -->

| # | Scenario | Input | Expected Output |
|---|---|---|---|
| 1 | <!-- [es. "Creazione risorsa con dati validi"] --> | | |

### Edge Cases

<!-- Casi limite, input anomali, condizioni al contorno. -->

| # | Scenario | Input | Expected Output |
|---|---|---|---|
| 1 | <!-- [es. "Stringa vuota nel campo obbligatorio"] --> | | |

### Error Cases

<!-- Errori attesi: validazione, autorizzazione, risorse mancanti, conflitti. -->

| # | Scenario | Input | Expected Error |
|---|---|---|---|
| 1 | <!-- [es. "Tenant inesistente → 404"] --> | | |

### Concurrency / Performance

<!-- Opzionale: scenari di concorrenza, race condition, carico.
     Rimuovere se non applicabile. -->

| # | Scenario | Setup | Expected Behavior |
|---|---|---|---|
| 1 | <!-- [es. "Due richieste parallele sullo stesso record"] --> | | |

### Integration Tests

<!-- Test che richiedono infrastruttura esterna (DB, Redis, Keycloak, API).
     Indicare quali container/mock servono. -->

| # | Scenario | Infrastruttura | Verifica |
|---|---|---|---|
| 1 | <!-- [es. "Persistenza su Firebird reale"] --> | <!-- TestContainers Firebird --> | |

## Implementation Order

<!-- Passi sequenziali per implementare la modifica.
     Ogni passo dovrebbe essere verificabile indipendentemente (test green). -->

1. <!-- [es. "Aggiornare le specifiche"] -->
2. <!-- [es. "Modificare entity e repository"] -->
3. <!-- [es. "Aggiornare service layer + test unitari"] -->
4. <!-- [es. "Integration test"] -->

## Risks and Mitigations

<!-- Rischi tecnici, operativi o di business. Per ogni rischio, una mitigazione. -->

| Rischio | Impatto | Mitigazione |
|---|---|---|

## Open Questions

<!-- Punti ancora da decidere. Rimuovere questa sezione quando tutti sono risolti. -->

- <!-- [domanda 1] -->
