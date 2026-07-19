# ApbPrivateServer reuse report

## What it is
C# multi-process emu (Lobby / Character / World / File) under Steam `ApbPrivateServer`.
Partial protocol surface (~2011 APS). **Not** a UE5 game runtime.

## Reusable
- Lobby + World **opcode numbers** (wired into `APBPrivateServerOpcodes.h`)
- `DBCharacter` fields: Name, Faction, Gender, Money, Rank, Threat, Custom → Domain CharacterProfile alignment
- Flow: login → character create/list → world list → world enter → district enter

## Not reusable as-is
- C# TCP servers replacing UE Domain
- Incomplete world handlers / modern client features
- DB stack (FrameWork) for UE shipping

## Concrete reuse in this project
- Generated `Source/APBReloaded/Domain/APBPrivateServerOpcodes.h`
- JSON map `Content/Extracted/Convert/privateserver_opcodes.json`
- Domain phase ↔ private-server stage helper
- Unit test reads **real** Opodes.cs / Opcodes.cs / DBCharacter.cs

## Lobby opcodes (29)
- `ASK_LOGIN` = 0x3E8 (1000)
- `LOGIN_PROOF` = 0x3E9 (1001)
- `ASK_CHARACTER_INFO` = 0x3EA (1002)
- `ASK_WORLD_LIST` = 0x3EB (1003)
- `ASK_CHARACTER_NAME_CHECK` = 0x3EC (1004)
- `ASK_CHARACTER_NAME_CHANGE` = 0x3ED (1005)
- `ASK_CHARACTER_CREATE` = 0x3EE (1006)
- `ASK_CHARACTER_DELETE` = 0x3EF (1007)
- `ASK_WORLD_ENTER` = 0x3F0 (1008)
- `ASK_CONFIGFILE_LOAD` = 0x3F1 (1009)
- `ASK_CONFIGFILE_SAVE` = 0x3F2 (1010)
- `ERROR` = 0x7D0 (2000)
- `KICK` = 0x7D1 (2001)
- `LOGIN_PUZZLE` = 0x7D2 (2002)
- `LOGIN_SALT` = 0x7D3 (2003)
- `ANS_LOGIN_SUCCESS` = 0x7D4 (2004)
- `ANS_LOGIN_FAILED` = 0x7D5 (2005)
- `CHARACTER_LIST` = 0x7D6 (2006)
- `ANS_CHARACTER_INFO` = 0x7D7 (2007)
- `WORLD_LIST` = 0x7D8 (2008)

## World opcodes (189) — sample
- `ASK_WORLD_ENTER` = 0xBB8
- `ASK_WORLD_QUEUE_CANCEL` = 0xBB9
- `ASK_INSTANCE_LIST` = 0xBBA
- `ASK_DISTRICT_RESERVE` = 0xBBB
- `ASK_DISTRICT_RESERVE_CANCEL` = 0xBBC
- `ASK_DISTRICT_ENTER` = 0xBBD
- `ASK_DISTRICT_EXIT` = 0xBBE
- `ASK_DISTRICT_QUEUE_CANCEL` = 0xBBF
- `LOGOUT` = 0xBC0
- `ASK_NAME_QUERY` = 0xBC1
- `ASK_CHAT_WHISPER` = 0xBC2
- `CHAT_GROUP` = 0xBC3
- `CHAT_CLAN` = 0xBC4
- `CHAT_OFFICER` = 0xBC5
- `CHAT_DISTRICT` = 0xBC6

## Character fields
- Id
- AcctId
- SlotId
- Name
- Faction
- Gender
- Version
- ChangeList
- Seconds
- Money
- Rank
- Threat
- Custom

## UE Domain alignment
- **LoginAccount**: Lobby ASK_LOGIN / LOGIN_PROOF / ANS_LOGIN_SUCCESS
- **CreateCharacter**: Lobby ASK_CHARACTER_CREATE / ANS_CHARACTER_CREATE
- **EnterWorld**: Lobby ASK_WORLD_ENTER / World ASK_WORLD_ENTER
- **JoinDistrict**: World ASK_DISTRICT_ENTER / ANS_DISTRICT_ENTER / DISTRICT_LIST
- **SessionPhase::Boot**: pre-login
- **SessionPhase::LoggedIn**: after ANS_LOGIN_SUCCESS
- **SessionPhase::WorldLobby**: after WORLD_LIST / ANS_WORLD_ENTER
- **SessionPhase::District**: after ANS_DISTRICT_ENTER
- **CharacterProfile.name**: DBCharacter.Name
- **CharacterProfile.faction**: DBCharacter.Faction (Enforcer/Criminal)
- **cash**: DBCharacter.Money
- **threat**: DBCharacter.Threat
