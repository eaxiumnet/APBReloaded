# Catalog Data Scope

## Overview

Runtime JSON catalogs derived from apbdb.com and verified APB reference data. These files
drive Domain behavior, frontend choices, districts, missions, items, vehicles, and imports.

## Ownership

| Data | Typical Owner/Consumer |
|---|---|
| Districts and placements | District directory, streamer, freeroam GameModes |
| Weapons, vehicles, clothing, inventory types | Catalog and customization/economy services |
| Missions, scripts, contacts, roles, threat | Mission/progression/threat Domain services |
| Palettes and UI strings | Frontend/customization presentation |
| Server/world records | World directory and launch tooling |

## Conventions

- Treat apbdb.com as the default authority. Use retail tables only where the active plan
  documents them as the fidelity source (for example `Contacts.INT`). The 2011 install is
  a valid source for main menu / frontend data only; never use it for gameplay, district,
  or non-frontend catalog values.
- Preserve stable string IDs and documented numeric IDs; code should resolve them through
  catalog APIs rather than duplicating them.
- Keep valid UTF-8 JSON with deterministic key ordering/formatting produced by the owning
  converter when one exists.
- Update the generator/parser and its provenance record with generated data; do not patch
  generated output alone when regeneration would overwrite it.
- Placement manifests have bound/unbound stages. Preserve the pairing and update import
  ledger/status evidence when asset bindings change.
- Validate schema-sensitive changes with the matching Domain/fidelity test plus a real
  JSON parser before launching UE.

## Anti-Patterns

- Inventing catalog entries, prices, stats, IDs, or relationships from memory.
- Renaming IDs without finding Domain, UE, test, and tooling consumers.
- Converting missing values to plausible defaults that hide incomplete extraction.
- Copying cooked binaries into this directory or treating extracted reference output as
  already imported runtime content.
- Hand-editing very large generated catalogs without updating their source pipeline.

## Verification

Run `tests/build_and_run.ps1` for catalogs consumed by Domain tests. For placements,
frontend data, or import bindings, also run the relevant tool gate and verify the actual
UE surface because standalone parsing does not prove runtime asset resolution.
