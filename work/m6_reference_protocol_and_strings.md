# APB Reference Spec — Protocol, Schema, Strings (fidelity source)

> Mined 2026-07-20 from `D:\APBReloaded\APB Reloaded\ApbPrivateServer` (retail C# emu)
> and `D:\APBReloaded\2011 apb\...\APBGame` (UE3 config + INT localization).
> **Scope note:** M6 uses UE-native NetDriver auth (per `_active.md` non-goal: real-client
> protocol compatibility stays out). This doc is the **behavioral/data/string fidelity
> reference** for current + future milestones — NOT a spec to emulate raw TCP.

## Retail world protocol (ApbPrivateServer)

Ports: Lobby TCP=2106, World TCP=2107, CharacterServer RPC=2000, FileServer RPC=2100,
MySQL=3306. Client gate: version 1.4.1 build 555239.

World opcodes C->S `0xBB8..0xC07`:
- `ASK_WORLD_ENTER=0xBB8`, `ASK_INSTANCE_LIST=0xBBA`, `ASK_DISTRICT_RESERVE=0xBBB`,
  `ASK_DISTRICT_ENTER=0xBBD`, `ASK_DISTRICT_EXIT=0xBBE`, `LOGOUT=0xBC0`,
  chat/group/clan/friend/marketplace/mail/config/league/population blocks follow.

World opcodes S->C `0xFA0..0x1013`:
- `ERROR=0xFA0`, `KICK=0xFA1`, `ANS_WORLD_ENTER=0xFA3`, `DISTRICT_LIST=0xFA6`,
  `ANS_DISTRICT_ENTER=0xFAA`, `ANS_DISTRICT_EXIT=0xFAB`, `ANS_NAME_QUERY=0xFAE`.
- **Ref bug:** `ANS_GROUP_LEADER == ANS_GROUP_LEAVE == 0xFBD`. Assign distinct in UE5.

`ASK_WORLD_ENTER` packet = single `uint32 AcctId` (big-endian). Character selection is
implicit — committed earlier by the lobby via `CharacterMgr.SetEnter(AcctId, SlotId)`.

`ANS_WORLD_ENTER` (success), all ints big-endian:
`int32 Result=0`, `int32 CharId`, `int32 Points`, `byte 1`, `int64 timestamp`,
`float marketBidPct=5.0`, `byte groupPublic`, `byte groupInvite`,
`5x int32 configFileVersion`, `byte tutorialComplete=1`, `byte lookInForGroup`.

## DB schema (CharDB)

`Characters` (13 cols): Id(PK,auto), AcctId(FK), SlotId(byte 1-8), Name(unique varchar),
Faction(byte), Gender(byte 0=M/1=F), Version(int), ChangeList(int), Seconds(int,unix),
Money(int), Rank(int), Threat(byte), Custom(string; '-'-delimited hex byte blob).

`WorldsInfo` (6 cols): Id(int,unique), Name(unique varchar), Status(byte 1=online),
Population(byte), Enf(byte), Crim(byte). Max 8 characters per account.

Login gating flow: client -> Lobby(2106) auth+charselect (`SetEnter`) -> redirect to
World(2107) -> `ASK_WORLD_ENTER{AcctId}` -> World RPC to CharacterServer(2000) fetches
the pre-flagged `Enter=true` char -> `ANS_WORLD_ENTER`. WorldServer self-registers into
CharDB via `RegisterWorld` on startup (CharacterServer = source of truth for live worlds).

## 2011 client (UE3 config + INT localization)

Network (`APBEngine.ini`): `Port=7777`, `Map=APBLoginLevel.apb`, beacon `8777`/`9777`,
`LanAnnouncePort=14001`, `ConnectionTimeout=60`, `InitialConnectTimeout=600`.
Replication: action `DefaultRepFreq_Action=200` (200 Hz), social `DefaultRepFreq_Social=5`.
`NetServerMaxTickRate=200` action / `15` social. `MaxPlayers=500`. Region `EU`.
LS link (`cHostingGC2LS`): `m_sLS1=apbworlddev:15001`, connect timeout 60000ms.
District exit timeout 300000ms (5 min). Min resolution 800x600.

Character name rule: alphanumeric `[a-z][A-Z][0-9]` only, may **not** start with a number.
Max 8 characters per account. Faction binding is permanent per world.

Key UI string sections in `APBUserInterface.int` (use verbatim for UE5 widget strings):
- `[APBLoginScreen]`: EmailAddress, Password, Login, Instructions, ContactingLS,
  LoggingIn, LoggingOut, ReturningToLobby, NoUserID, NoPassword, CapsLockIsOn.
  6 rotating `Compliment_00..05` login flavor lines.
- `[CharacterSelectScreen]`: SelectCharacter, CreateCharacter, DeleteCharacter, Play,
  Logout, EmptyCharacter, DeleteCheck, Threat, Rating, gametime remaining strings.
- `[CharacterCreateScreen]`: ChooseFaction, ChooseGender, Male/Female, Enforcer/criminal,
  CharacterName, SelectWorld; `CC_*` creation-failure + `CN_*` name-validation error codes.
- `[WorldSelectScreen]`: WorldName, WorldID, Status, EnfPopulation, CrimPopulation,
  ContactingWorldServer, permanent world-binding confirm.
- `[DistrictSelect_Action]` / `[WorldMapScreen]`: ActionDistricts, SocialDistrict, joins.

Kick/disconnect (`[CommonGameFlowScenes]`): KickDuplicateLogin, KickGM, KickTimeExpired,
KickPunkbuster, KickPackageMismatch. Gametime-expired gates Action Districts.

Content taxonomy: Districts = Financial, Waterfront, Social (+Criminal/Enforcer Social).
Rulesets = Default, Hardcore, Tutorial, Social. Factions = Enforcer, Criminal.
Orgs: Criminal (G-Kings, Blood Roses, Red Rain); Enforcer (Praetorians, Prentiss Tigers, SPPD).
Support URLs (`[APBSupportURL]`): AccountManagement, CreateAccount, APBVault.
