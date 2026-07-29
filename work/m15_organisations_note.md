# M15 increment — Organisation catalog (contact gangs / Joker vendors / Armas store fronts)

**Status:** COMPLETE. All 17 domain test binaries green (FAILS=0, 0 FAIL:, 0 error C).

## What this adds
The authoritative **organisation** list that the game groups content by:
- the 8 contact **gangs** (G-Kings, Blood Roses, Red Rain, Anarchists = Criminal;
  Praetorians, Prentiss Tigers, SPPD, Red Hill Institute of Technology = Enforcer),
- the Criminal/Enforcer **default** + **seasonal** ("Holiday Contact") buckets,
- the Joker weapon **vendors** (Joker Distribution, Joker Associates),
- and the Armas **store** fronts (Armas, Joker Box, Joker Box No-Trade) + the Tutorial org.

This is the list the Armas marketplace filters and the contact UI use to group items and
contacts by their owning organisation.

## Source & extraction
- Retail `...\APBGame\Localization\INT\Organisations.INT` (UTF-16LE mirror of the cooked
  SDD table `Organisation`) — a flat `Organisations_<id>_Name=<display>` list of 20 ids.
- `tools/scripts/extract_organisations.ps1` -> `Content/Data/organisations.json` (19 rows;
  the empty `None` row is skipped). Names are taken **verbatim** from the INT.

## Provenance of `faction` / `kind` (important)
The INT only carries the display **name**. The org->faction affiliation (the SDD
`Organisation.Faction` column) is **cooked away** and absent from every plain-text file, so
the extractor classifies each id from canonical, unambiguous APB fact:
- `faction`: the `Criminal*`/`Enforcer*` prefixed ids resolve by prefix; the named gangs use
  their well-known affiliation (Blood Roses/G-Kings/Red Rain/Anarchists = Criminal;
  Praetorians/Prentiss Tigers/SPPD/RIOT = Enforcer); store/vendor/tutorial = `None` (neutral).
- `kind`: `gang` / `default` / `seasonal` / `vendor` / `store` / `tutorial` / `none`.
If a future authoritative source (e.g. an apbdb Organisation dump) contradicts a row, prefer
it and re-run the extractor — the Domain load is merge-by-id so it updates in place.

## Domain
- `Source/APBReloaded/Domain/APBOrganisations.h` — header-only `OrganisationCatalog`
  (matches `FactionInfoCatalog`: implicitly-inline, string-/escape-aware parser + proper JSON
  unescaper). API: `Find`, `Name`, `ForFaction(Faction|string)` (rank-ordered), `OfKind`,
  `Count`, `CountForFaction`, `CountOfKind`. Additive/merge-by-id, sorted by rank.
- Wired into `APBWorldService` as `organisations` (loaded from `organisations.json` in
  `InitFromDataDir`, next to `faction_info`), with an `organisations=<n>` INIT log token.

## Tests
`tests/run_domain_tests.cpp` — `TestOrganisationsFromRetail`: 19-row count; verbatim display
names; faction affiliation; kind classification; faction grouping counts (6 criminal /
6 enforcer / 7 neutral) with rank order preserved; kind counts (8 gangs / 3 stores); rank
sort; missing-id default; WorldService end-to-end.

## Notes for other agents
- Group Armas listings / contacts by `OrganisationCatalog::ForFaction(faction)` or
  `OfKind("gang")` — do NOT hardcode the gang list. On the UE side, map `Organisation` onto
  a lightweight struct for the store filter + contact header.
- `faction` is a string ("Criminal"/"Enforcer"/"None"), not the `Faction` enum, because
  neutral store/vendor orgs have no faction. Use `ForFaction(Faction)` for the enum path.
- The contact catalog (`contacts_lore.json`) has NO org id per contact — Contacts.INT only
  carries Title/Description. Linking a contact to its org needs the SDD Contact table
  (cooked away), so that mapping is a separate future increment, not derivable from the INT.
