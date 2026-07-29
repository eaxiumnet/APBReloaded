# M14 — Mail attachment claim + delete (retail fidelity) — landed

Author: GPT-5.8 agent · 2026-07-20
Status: DONE + tested. Extends the existing real `MailService` (`APBSocial.h`) with the two
retail behaviors it was missing — attachment claim ("Take All") and message delete — and
persists the claim state through the canonical `mail.json` store.

## Why

`MailService` already had send / inbox / unread / mark-read + `mail.json` persistence via
`JsonDomainStore::SaveMail/LoadMail`. The M14 brief includes the **mail UI**, and the two
gameplay verbs the UI needs were absent: you could not *claim* a mail's cash/item
attachments, nor *delete* a message. Both are core retail interactions.

## What landed (all additive, backward-compatible)

| File | Change |
|---|---|
| `Source/APBReloaded/Domain/APBSocial.h` | `MailMessage.claimed` + `created_utc`; `MailService::ClaimAttachments` / `HasUnclaimedAttachments` / `Delete` / `PurgeExpired` (inline, header-only) |
| `Source/APBReloaded/Domain/APBPersistence.cpp` | `SaveMail`/`LoadMail` now round-trip `"claimed"` + `"created_utc"` (default false/0 when absent) |
| `tests/run_mail_tests.cpp` | new header-only suite: 4 groups (~30 assertions incl. expiry) |
| `tests/run_persistence_tests.cpp` | claim-in-B → `claimed` persisted-in-C assertions |
| `tests/build_and_run.ps1` | wired 10th suite `APBMailTests` (`$exe10`) |

Verified: `powershell -File tests\build_and_run.ps1` → all **10** suites `FAILS=0`, exit 0
(Domain, Persistence, Fidelity, Auth, Chat, Group, Clan, Friend, SocialStore, **Mail**).
Persistence suite shows `PASS: B claim mail cash attachment` + `PASS: C mail claimed flag persisted`.

## Semantics (retail-grounded)

- **`ClaimAttachments(id)`** — the retail "Take All". Returns the cash+item attachments
  **exactly once**; marks the message `read` + `claimed`. A second claim (or a message with
  no attachments / a missing id) returns empty. The attachment records stay on the message
  for history. The **caller** (currency/inventory service) applies the returned cash/items —
  `MailService` stays pure and does not touch the wallet/bag itself.
- **`HasUnclaimedAttachments(id)`** — true only when the message has attachments and has not
  been claimed.
- **`Delete(id)`** — retail forbids discarding a message that still holds unclaimed
  attachments (the client forces "Take All" first): returns `false` in that case. Messages
  with no attachments, or already-claimed ones, delete immediately.
- **`PurgeExpired(now, ttl_seconds)`** — retail mail expiry. Purges messages whose
  `created_utc > 0` and `now - created_utc >= ttl_seconds` (retail default ~30 days =
  2592000s); returns the count removed. `now` is caller-supplied (server clock) so the Domain
  stays clock-free/deterministic. Legacy/system mail sent without a timestamp
  (`created_utc == 0`) never expires. Callers wanting retail "return unclaimed attachments to
  sender" should check `HasUnclaimedAttachments` before purging.
- **Persistence** — `claimed` and `created_utc` survive a relog via `mail.json`. A cash-only
  `SendMail(...,cash>0)` still materializes as one attachment (unchanged), so it participates
  in claim/delete/expiry too.

## Integration seam (for the UE/mail UI)

The mail panel calls `InboxFor` / `UnreadCount` for the list, `ClaimAttachments` on "Take All"
(then hands the returned attachments to the inventory/currency service and re-saves via
`JsonDomainStore::SaveMail`), and `Delete` on discard. `HasUnclaimedAttachments` gates the
delete button. No new store file — mail stays in the canonical `mail.json`.

## Not touched / deferred

- **Item-attachment application to inventory** is the caller's responsibility (kept out of the
  pure mail Domain) — `ClaimAttachments` returns the records; the currency/inventory service
  applies them.
- **Return-to-sender on expiry** — `PurgeExpired` currently drops expired mail; wiring the
  retail "unclaimed attachments bounce back to sender" behavior is a caller-side follow-up
  (check `HasUnclaimedAttachments` before purge, re-`SendMail` to the original sender).

Do NOT re-create `MailService` — extend it in place. Keep new logic inline/header-only so the
`run_mail_tests.cpp` suite stays zero-link.
