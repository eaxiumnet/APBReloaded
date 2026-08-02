# M3R Task 17 — Verified asset registry startup crash fix (2026-08-02)

## Reported crash (startup, every launch)

```
Assertion failed: false [GenericPlatformMisc.cpp:2021] No SHA256 Platform implementation
UnrealEditor_APBReloaded!`anonymous namespace'::IsAllowlistProvenanceBound()
  [Source/APBReloaded/Systems/APBVerifiedAssetRegistry.cpp:87]
UnrealEditor_APBReloaded!UAPBVerifiedAssetRegistry::Initialize()
```

## Root cause

`IsAllowlistProvenanceBound()` hashed the allowlist file bytes via
`FGenericPlatformMisc::GetSHA256Signature()` + `FSHA256Signature`. The generic platform
fallback for that API is an unimplemented stub that asserts on this engine distribution.
The check runs on every startup (even with the empty allowlist), so the game could not
boot at all.

## Fix

`Source/APBReloaded/Systems/APBVerifiedAssetRegistry.cpp` now uses the project's own
engine-free, pure-C++17 SHA-256 (`Domain/APBCrypto.h`, already used by
`APBSecretProvider` for HMAC-SHA256):

```cpp
const std::array<uint8_t, 32> Digest = apb::sha256(AllowlistBytes.GetData(), static_cast<size_t>(AllowlistBytes.Num()));
const std::string Hex = apb::hex_encode(Digest.data(), Digest.size());
return ExpectedHash.Equals(UTF8_TO_TCHAR(Hex.c_str()), ESearchCase::IgnoreCase);
```

Removed `GenericPlatform/GenericPlatformMisc.h`; added `APBCrypto.h`, `<array>`, `<string>`.
Output format matches the tooling: lowercase 64-hex, same as
`check_strict_asset_provenance.ps1`/`promote_verified_assets.ps1` hashes.

Commit: `ba5e669a` (M3R task-17 delta via `tools/commit_m3r_task_delta.ps1`; files were
untracked add-files). Evidence: `.omo/evidence/m3r-task17-20260802/`.

## Verification (all green)

- `APBReloadedEditor Win64 Development` → `Result: Succeeded` (58s).
- Headless launch of the exact crashing path:
  `UnrealEditor.exe <project> /Game/Maps/Lvl_APB_Frontend -game -APBProbe=asset_allowlist
  -APBStrictAssetAllowlist -nullrhi -unattended` → no assertion; log shows
  `VERIFIED_ASSET_REGISTRY_INIT strict=1 manifest=1 entries=0` — the provenance-bound
  SHA-256 check ran and PASSED (allowlist hash == `catalog_provenance_manifest.json`
  registration `a9c3c9d7...`).
- Full probe verdicts (empty allowlist, expected R6 state):
  `RUNTIME_ALLOWLIST_ALLOW_BLOCKED reason=no_verified_entry`,
  `RUNTIME_ALLOWLIST_REJECT_OK`, `RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK`, exit 0.

## Notes

- Probe run needed `MSYS_NO_PATHCONV=1` in git-bash: MSYS mangles a leading `/Game/...`
  map arg into `C:/Program Files/Git/Game/...`, which aborts startup with
  `StartGameInstance.Cancelled`. The spine runs via PowerShell so it is unaffected.
- Runtime enforcement is unaffected by the empty allowlist: unlisted paths are still
  rejected (`REJECT_OK`) and no substitutes are allowed (`NO_SUBSTITUTE_OK`); the R6
  positive proof (`ALLOW_OK`) intentionally stays blocked until verified ledger rows exist.
