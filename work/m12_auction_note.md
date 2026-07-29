# M12 — Marketplace / Auction House (Domain) handoff

**Status:** Domain logic DONE + tested + restart-durable. All 13 domain suites FAILS=0
(`$exe13 = APBAuctionTests`, 6 test groups).
**Author:** Qoder. **Scope:** the D7 marketplace mechanics on `apb::AuctionHouse` —
bid / buyout / expiry / **5% fee** / cancel, with item+cash settlement delivered through
`MailService` (retail delivers marketplace results to the mailbox so an offline
counterparty still receives them). Pure C++17.

## Files
- `Source/APBReloaded/Domain/APBAuction.{h,cpp}` — extended (was buyout-only).
- `Source/APBReloaded/Domain/APBPersistence.cpp` — `SaveAuction`/`LoadAuction` extended to
  the full §4 schema (auction-only lines; the mail block was untouched).
- `tests/run_auction_tests.cpp` — 6 groups; wired as `$exe13` in `tests/build_and_run.ps1`
  (links `APBAuction.cpp` + `APBInventory.cpp` + `APBCatalog.cpp`; `MailService` is header-only).
- `tests/run_domain_tests.cpp` — one existing assertion updated: buyout seller proceeds now
  reflect the 5% fee (1000 → 950).

## Model
`AuctionListing` now carries `start_price` (opening bid), `buyout_price`, `current_bid` +
`high_bidder` (escrow), `created_utc`, `expires_utc` (0 = never), `fee_paid`, and an
`AuctionState { Active, Sold, Expired, Cancelled }` (`active` bool kept in sync for
persistence back-compat). `AuctionHouse` gains `MailService* mail` and `fee_bps = 500` (5%).

### Behaviors
- **ListItem** (legacy) = buyout-only, never-expiring; delegates to **ListItemEx** which adds
  opening-bid + expiry. A listing must have a buyout price, an opening bid, or both.
- **Buyout**: item → buyer, `price - 5%` → seller, `fee_paid` recorded, state → Sold. A
  standing bidder is refunded their escrow via mail. (Signature unchanged — existing
  WorldService `AuctionBuy` facade keeps working.)
- **PlaceBid**: escrows the bid from the bidder immediately; the previous high bidder is
  refunded via mail. Rejects: own listing, expired, below `start_price`, not beating the
  current bid, `>= buyout` (use buyout instead), insufficient funds.
- **Cancel** (seller only): refunds a standing bidder + returns the item to the seller, both
  via mail; state → Cancelled. Requires a mail service.
- **SettleExpired(now)**: any active listing past `expires_utc` — with a high bidder → Sold
  (item → winner, `bid - 5%` → seller, via mail); with no bids → Expired (item returned to
  seller via mail). Requires a mail service. Returns count settled. Idempotent (settled
  listings are skipped).

### Determinism / integration
- All time is a **caller-supplied `now_utc`** — no wall clock, reproducible under test.
- Escrow invariant: the house holds exactly `current_bid` for the standing `high_bidder`;
  every path that clears/replaces it refunds that escrow via mail.
- Persistence round-trips the full schema; old `auction.json` files (state `active|inactive`)
  still load (`inactive` → Sold).

## NOT done here (open for other agents)
- **UE Economy widgets** (`Systems/Economy/APBAuctionWidget`, `APBMailWidget`) + the
  `AAPBInteractable` marketplace kiosk on retail `[Marketplace]` strings (ARCHITECTURE §2).
- **World control routing**: `auction.sync` over the TCP/JSON control channel + district-side
  browse snapshot caching (ARCHITECTURE §3). The world server must own the single
  `AuctionHouse` instance and drive `SettleExpired(now)` on a timer.
- Wiring `MailService*` into the live `WorldService.auction` at server bootstrap (the Domain
  API is ready; only the pointer assignment + expiry tick remain).

## Verify
`powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1`
→ all 13 suites `FAILS=0` (auction header: `=== APB Auction Tests (M12 marketplace bid/buyout/expiry/fee/cancel) ===`).

Ready for: `feat(M12): Domain marketplace bid/buyout/expiry/5%-fee/cancel + mail settlement + persistence`.
