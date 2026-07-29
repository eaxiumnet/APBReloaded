"""Curated disk-folder -> apbdb catalog-key aliases.

The disk folder names (retail extract) are a DIFFERENT namespace from apbdb
``sAPBDB`` ids: none of them exist as apbdb ids and their meshes/textures carry
no apbdb family token, so the folder->weapon link cannot be derived from data --
it comes from visually identifying the rendered mesh. Each VALUE below is an
apbdb catalog key whose display IS authoritative (verified kd[key] == display);
each KEY->VALUE pairing is confirmed by looking at the model in the viewer. Cut /
prototype / placeholder weapons (e.g. ``Weapon_TacticalAssaultRifle``, the
Apocalypse series) are deliberately absent so they stay honestly "derived"
rather than risk a confidently-wrong label.
"""

from __future__ import annotations

# folder.name -> catalog key (base_weapon_id / mesh_key / id) whose display is authoritative
ALIASES: dict[str, str] = {
    "Weapon_Armas_AgroSniper": "Weapon_SniperRifle_NCR",
    "Weapon_Armas_LightMachineGun_762": "Weapon_LMG_ALIG762",
    "Weapon_Armas_Magnum": "Weapon_Pistol_ACT44-GM_Joker",
    "Weapon_Armas_Shotgun": "Weapon_Shotgun_CSG_Joker",
    "Weapon_AssaultRifle": "Weapon_AssaultRifle_NTEC",
    "Weapon_AutomaticShotgun": "Weapon_Shotgun_NFAS",
    "Weapon_BurstFirePistol": "Weapon_Pistol_RFP9",
    "Weapon_BurstFireRifle": "Weapon_Rifle_OBIR",
    "Weapon_Carbine": "Weapon_Rifle_JokerCarbine",
    "Weapon_ColbyClassic": "Weapon_Pistol_ColbyCommander",
    "Weapon_CrowdControlGun": "Weapon_LTL_CCG",
    "Weapon_DartGun": "Weapon_LTL_DartGun",
    "Weapon_EightBall": "Weapon_Grenade_Physical_EightBall",
    "Weapon_Explosive_AAPED": "Weapon_Explosive_AAEPD",
    "Weapon_FragmentationGrenade": "Weapon_Grenade_Frag",
    "Weapon_HouseBrick": "Weapon_Grenade_Physical_Brick",
    "Weapon_LightMachineGun_556": "Weapon_LMG_SHAW556",
    "Weapon_LightMachineGun_762": "Weapon_LMG_ALIG762",
    "Weapon_LightMachineGun_SSW": "Weapon_LMG_NSSW",
    "Weapon_MachinePistol": "Weapon_Pistol_OCANano",
    "Weapon_MachinePistol_PDW": "Weapon_Pistol_SASPDW",
    "Weapon_MobilePhone": "Weapon_Grenade_Physical_Phone-Crim_Armas",
    "Weapon_ObeyaBattleRifle": "Weapon_Rifle_Obeya",
    "Weapon_RocketLauncher": "Weapon_Explosive_OSMAW",
    "Weapon_Secondary_Fr0g": "Weapon_Pistol_Frog_Armas",
    "Weapon_Secondary_OUL": "Weapon_Pistol_UL3_Base",
    "Weapon_SemiAutoPistol": "Weapon_Pistol_Colby45AP",
    "Weapon_Shotgun": "Weapon_Shotgun_JG",
    "Weapon_SMG_Vector": "Weapon_SMG_CBMP_Base",
    "Weapon_SniperRifle_50Cal": "Weapon_SniperRifle_DMR",
    "Weapon_SniperRifle_PSR": "Weapon_SniperRifle_PSR_Joker",
    "Weapon_SnowBall": "Weapon_Grenade_Physical_Snowball_Missions",
    "Weapon_SnubNoseRevolver": "Weapon_Pistol_Snubnose",
    "Weapon_StarterSMG": "Weapon_SMG_OCA",
    "Weapon_StunGun": "Weapon_LTL_Tazer",
}

# Curated apbdb-authoritative BASE FAMILY display names, keyed by retail disk
# folder. Used ONLY where the generated catalog has no clean family row (junk
# base like 'raptor_base'/'SBSR_', brand-incomplete 'CAP40', or a collision-
# dropped model_key), so the name cannot be derived from data. Every value is
# verified against https://apbdb.com/ item listings; resolved ABOVE the catalog
# tiers to override preset/mod-variant pollution (e.g. "AMG - Bipod", "SBSR 'IRS'").
FAMILY_DISPLAY: dict[str, str] = {
    "Weapon_LMG_AMG": "AMG-556",
    "Weapon_LMG_CAS": "CASE",
    "Weapon_SMG_C9S": "C9",
    "Weapon_SMG_Norsemen": "Norseman",
    "Weapon_AssaultRifle_FAR": "FAR",
    "Weapon_SMG_CBMP": "CBMP-45",
    "Weapon_CAP40": "Obeya CAP40",
    "Weapon_AssaultRifle_Raptor": "Raptor 45",
    "Weapon_SniperRifle_SBSR": "SBSR",
    "Weapon_Rifle_FFA": "FFA 5.56",
    "Weapon_Secondary_OCSP": "OCSP",
    "Weapon_Secondary_Mountie": "Mountie",
    "Weapon_Shotgun_Shredder": "Shredder",
    "Weapon_AssaultRifle_Fanatic": "S1-FA",
    "Weapon_GrenadeLauncher": "O-PGL 79",
    "Weapon_Vas": "VAS SW2",
    "Weapon_AssaultRifle_VAS": "VAS R-2",
    "Weapon_AssaultRifle_NTEC-7": "N-TEC 7",
    "Weapon_Shotgun_DOW": "DOW",
    "Weapon_AssaultRifle_Apocalypse": "AR-97 'Misery'",
    "Weapon_Colby_Commander": "Colby Commander",
    "Weapon_Rifle_SACR": "STAC 10",
    "Weapon_SniperRifle_Vesper": "Vesper",
}

# The Apocalypse "Four Horsemen" folder bundles four distinct weapons (AR/SMG/
# Shotgun/Sniper) side by side; largest-.psk-by-bytes picks Death (sniper), but
# the folder's name "AR-97 'Misery'" is the Famine AR. Maps folder -> the mesh-stem
# token (case-insensitive) build_weapon_catalog must prefer as the primary part.
PRIMARY_MESH: dict[str, str] = {
    "Weapon_AssaultRifle_Apocalypse": "Famine",
}
