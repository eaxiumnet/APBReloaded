# M14 S10 — Mail Claim Journal + Fail-Closed Items (work plan)

Scope approved by user: **full plumbing now, items fail-closed, S10 item-grant stays RED.**
Architecture per Oracle ruling (option (a) — world owns the whole claim).

## Corrections to earlier assumptions (verified on disk this session)

1. `InitSocialPersistence` is **not** called from `UAPBGameInstanceSubsystem::Initialize`.
   It is called on `SocialAuthority` from `Systems/Server/APBWorldGameMode.cpp:26`.
   The journal must be initialised inside `WorldService::InitSocialPersistence`
   (`Domain/APBWorldService.cpp:548-552`), which already does
   `social_store.Init(dir + "/social")` — the exact `Saved/DomainDB/social/` dir the
   journal needs. No new UE-side init call, no new path construction in Domain.
2. The "keep the journal out of shared `$srcs`" goal is **unachievable** once the journal
   is a `WorldService` member: `APBWorldService.cpp` is already in `$srcs`, so
   `APBMailClaimJournal.cpp` must join `$srcs` or every suite linking WorldService fails
   to link. Accept the re-link across the 4+ suites that use `$srcs`; do not pretend the
   constraint holds. The *serializer* constraint still holds — mail JSON stays owned by
   `JsonDomainStore` alone; the journal has its own separate schema/file.
3. Existing mail tests only ever construct cash-only attachments (`SendMail` → one
   attachment with empty `item_id`). The new item guard collides with **no** existing
   expectation. `TestClaimSemantics` and `TestDeleteRules` stay valid unchanged.
4. `EAPBMailResult` (`Systems/APBGameInstanceSubsystem.h:98-107`) has 7 values;
   `UnsupportedAttachment` is added as an 8th. `SocialMailSend` is declared at `:399`
   with no `Character` parameter — one is added.
5. Harness slots run to `$exe18` (`APBHandoffTests.exe`). The journal suite takes
   `$exe19` → `APBMailClaimJournalTests.exe`.

## Domain API (agreed shape)

`MailService` (`Domain/APBSocial.h`) gains:

| Member | Contract |
|---|---|
| `bool HasItemAttachments(int64_t id) const` | true iff any attachment has non-empty `item_id` |
| `bool CommitClaimed(int64_t id)` | flag-only step: sets `claimed=true; read=true`; false if missing/already claimed |
| `ClaimAttachments(int64_t id)` *(modified)* | refuses item-bearing mail: returns `{}` and leaves `claimed=false` |

Fail-closed invariant: on refusal, `read`, `claimed`, `attachments`, cash, inventory and
persistence are **byte-identical** to pre-call. Because `Delete` refuses while
`!claimed && !attachments.empty()`, leaving `claimed=false` keeps the message
undeletable and reclaimable — exactly the retail behaviour we want until inventory lands.

## Journal

New translation unit `Domain/APBMailClaimJournal.{h,cpp}`, pure C++17, `namespace apb`,
no UE headers, caller-supplied dir and clock.

- File: `<dir>/mail_claims.json` where `<dir>` is the same `dir + "/social"` that
  `SocialStore` receives.
- Key: `{character, mail_id}`. **Not** `operation_id` — that neither survives a world
  restart nor dedups a retry re-issued after travel with a fresh id.
- Record: `{character, mail_id, state, cash_delta, claimed_utc}`.
- States: `Prepared` → `CharacterCommitted` → `MailCommitted`.
- Ordering: `Prepared` persists **before** either aggregate mutates; then cash credit +
  durable `{character,mail_id}` receipt; only then `mail.claimed=true` persists.
- Recovery keys off the **character receipt**, not the operation id.

| Crash point | Recovery action |
|---|---|
| after `Prepared` | nothing was credited → re-execute the claim in full |
| after `CharacterCommitted` | cash already credited → do **not** re-credit; just commit the mail flag, advance to `MailCommitted` |
| after `MailCommitted` | terminal → no-op, replay returns the committed result |

Accepted failure mode (Oracle): delayed delivery after commit shows the client a
Timeout/stale UI; the retry replays the committed result with no second credit.
Never loss, never duplication.

## Defect 6 — stale handoff overwrite

`RestoreHandoff` (`APBWorldService.cpp:691`) takes cash wholesale from the district
snapshot, so a stale return silently reverts a world-committed claim. Both
`ApplyHandoff` (`:764`) and `ApplyHandoffForAccount` (`:770`) reconcile **after**
`RestoreHandoff` succeeds and **before** persistence: for each committed receipt for this
character, reapply `cash_delta` exactly once, and for a `CharacterCommitted` receipt whose
mail is still unclaimed, also commit the mail flag. `RestoreHandoff` itself is untouched.

## Wave graph

| Wave | Tasks | Files | Why no collision |
|---|---|---|---|
| 0 | T1, T2, T3 (RED tests) | `run_mail_tests.cpp` / new `run_mail_claim_journal_tests.cpp` / `run_handoff_tests.cpp` | three distinct test files; `build_and_run.ps1` deliberately untouched so the harness stays runnable while the journal type does not exist |
| 1 | T4, T5 | `APBSocial.h` / new journal TU | different files; T5 creates only new files |
| 2 | T6 | `APBWorldService.{h,cpp}`, `build_and_run.ps1` | needs T5's type to declare the member and register `$exe19` + `$srcs` |
| 3 | T7 | `APBWorldService.cpp` (handoff reconcile) | needs T6's member |
| 4 | T8 | `APBGameInstanceSubsystem.{h,cpp}` | needs T4 + T5 symbols |
| 5 | T9 | `APBGameInstanceSubsystem.{h,cpp}` | same file as T8 → strictly after it |
| 6 | T10 | none (verification only) | — |

Contention points, explicit: `APBSocial.h` is touched only by T4. `APBWorldService.cpp` is
touched by T6 then T7, never in parallel. `APBGameInstanceSubsystem.cpp` is touched by T8
then T9, never in parallel. `build_and_run.ps1` is touched only by T6.

## Scenario contract

| # | Scenario | Binary pass condition | Artifact |
|---|---|---|---|
| S1 | cash-only claim, no crash | returns `Ok`, cash += total, `claimed==true`, journal `MailCommitted`, `mail_claims.json` on disk | `APBMailClaimJournalTests.exe` FAILS=0 + the json file contents |
| S2 | crash after `Prepared` | cash credited exactly once after recovery; journal reaches `MailCommitted` | journal suite FAILS=0 |
| S3 | crash after `CharacterCommitted` | cash **not** re-credited; mail flag committed | journal suite FAILS=0 |
| S4 | crash after `MailCommitted` | replay is a no-op; returns already-claimed; no second credit | journal suite FAILS=0 |
| S5 | item-bearing claim (fail-closed) | returns `UnsupportedAttachment`; `read`/`claimed`/attachments/cash unchanged; still undeletable and reclaimable | `APBMailTests.exe` FAILS=0 |
| S6 | stale handoff overwrite | after handoff, cash == `snapshot.cash + committed delta`, not `snapshot.cash` | `APBHandoffTests.exe` FAILS=0 |
| S7 | adjacent-surface regression | mail delete/markread/inbox/unread, relay social verbs, social store all unchanged | full harness EXIT=0 |
| **S10** | **item actually granted to inventory** | **STAYS RED — out of scope by user decision; no inventory integration exists yet. S5 proves we refuse safely instead of destroying, which is the whole of the in-scope work.** | none; reported RED |

## Crash-gate determinism

No process is killed. `MailClaimJournal` exposes the three transitions as separate
methods, so a test writes a fixture `mail_claims.json` pinned at each state, constructs a
fresh service against it, and runs recovery. The fixture *is* the post-crash,
pre-recovery state — fully deterministic, no timing dependence.

## Commit boundaries

| Commit | Tasks | Subject |
|---|---|---|
| 1 | T1-T3 | `test(mail): RED suites for fail-closed claim, claim journal, handoff reconcile` |
| 2 | T4 | `feat(mail): fail-closed item claims via HasItemAttachments + CommitClaimed` |
| 3 | T5, T6 | `feat(mail): MailClaimJournal with Prepared/CharacterCommitted/MailCommitted` |
| 4 | T7 | `fix(handoff): reconcile committed mail claims before snapshot save` |
| 5 | T8 | `feat(mail): journaled claim bridge + EAPBMailResult::UnsupportedAttachment` |
| 6 | T9 | `fix(mail): resolve SocialMailSend sender from world AdmittedRoster` |

Implementation agents never run git. All commits are made by the orchestrator.
