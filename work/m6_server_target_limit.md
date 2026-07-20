# M6 — Server Target Limit: Root Cause, Workaround, and M16 Path

**Status**: Documented 2026-07-20
**Milestone**: M6 (Login / Auth / World Server)

---

## Root Cause

### InstalledBuild.txt

`D:\UE58\UE_5.8\Engine\Build\InstalledBuild.txt` exists and contains:

```
UE_5.8
```

Its presence tells UBT this is an **installed** (binary) engine distribution, not a source build.

### InstalledPlatforms restriction

Installed engines ship with a `BaseEngine.ini` inside the engine tree that declares `InstalledPlatforms` entries only for **Editor** and **Game** targets. There is no `Server` entry. The project-level `Config\BaseEngine.ini` does not exist, so there is no project-side override.

### UBT throw site

`Engine\Source\Programs\UnrealBuildTool\System\UEBuildTarget.cs`
When UBT resolves a target, it checks whether the requested `TargetType` is listed in `InstalledPlatforms`. If the type (`Server`) is absent, UBT throws:

> "Target type Server is not supported in an installed build of the engine."

This fires on any attempt to build `APBReloadedServer` with the installed engine binary.

---

## Current State of APBReloadedServer.Target.cs

File: `Source\APBReloadedServer.Target.cs`
**Status: EXISTS — DO NOT MODIFY**

```csharp
Type = TargetType.Server;
DefaultBuildSettings = BuildSettingsVersion.V7;
IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
```

This file is kept untouched so it is ready the moment a source engine is available (M16). Editing it would create unnecessary merge work later.

---

## M6 Workaround — Game Target Headless + `-WorldServer` Role Flag

Since the installed engine rejects `TargetType.Server`, the world-server role runs on the **Game target** in a headless configuration. The `-WorldServer` CLI flag signals to `APBGameInstanceSubsystem` / `APBWorldGameMode` that this process should behave as a world server (no player client, no rendering).

**Launch flags:**

```
APBReloaded -WorldServer -nullrhi -nosound -unattended
```

| Flag | Purpose |
|------|---------|
| `-WorldServer` | Activates world-server role in GameInstanceSubsystem |
| `-nullrhi` | Disables GPU / render hardware interface |
| `-nosound` | Disables audio |
| `-unattended` | Suppresses interactive dialogs |

This is a **runtime role flag**, not a build-time target change. `APBReloadedServer.Target.cs` is NOT used in M6.

---

## M16 Path — Source Engine

When upgrading to a **UE 5.8 source build**:

1. Replace `D:\UE58\UE_5.8` with a compiled source engine (no `InstalledBuild.txt` present).
2. `APBReloadedServer.Target.cs` (already present, untouched) becomes buildable immediately.
3. Remove or deprecate the `-WorldServer` headless workaround once the Server target compiles.
4. Confirm `InstalledPlatforms` entries in the source engine's `BaseEngine.ini` include `Server`, or confirm their absence means no restriction applies.

---

## File Inventory

| File | Status | Notes |
|------|--------|-------|
| `Engine\Build\InstalledBuild.txt` | EXISTS | Contains `UE_5.8`; triggers installed-engine mode |
| `Config\BaseEngine.ini` (project) | NOT FOUND | No project-level InstalledPlatforms override |
| `Config\DefaultEngine.ini` | EXISTS | No InstalledPlatforms entries present |
| `Source\APBReloadedServer.Target.cs` | EXISTS | `TargetType.Server`; kept untouched for M16 |
| `Source\APBReloaded.Target.cs` | EXISTS | Game target; used for M6 headless run |
| `Source\APBReloadedEditor.Target.cs` | EXISTS | Editor target |
