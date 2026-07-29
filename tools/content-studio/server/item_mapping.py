"""Mapping between extracted asset names and APBDB.com item IDs.

This module provides the mapping between the internal asset names used in
the extracted game files and the item IDs used on apbdb.com.

The mapping is bidirectional:
- asset_to_apbdb: Maps extracted asset folder names to APBDB item IDs
- apbdb_to_asset: Maps APBDB item IDs to extracted asset folder names
"""

from __future__ import annotations

# Mapping from extracted asset names to APBDB item IDs
# Format: "ExtractedAssetName": "APBDB_Item_ID"
ASSET_TO_APBDB = {
    # Dog Tags
    "F_Neckwear_Necklace_Enforcement_Dogtag": "Clothing_F_Neckwear_Necklace_Enforcement_Dogtag",
    "M_Neckwear_Necklace_Enforcement_Dogtag": "Clothing_M_Neckwear_Necklace_Enforcement_Dogtag",
    
    # 7-Sins Collection (Praetorian T3a)
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_DogTags": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_DogTags",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_DollarRingIndexLeft": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_DollarRingIndexLeft",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SkullRingMiddleRight": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SkullRingMiddleRight",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SmallRingRingLeft": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SmallRingRingLeft",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SwirlClawEarringRight": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_SwirlClawEarringRight",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_WristwatchRight": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_WristwatchRight",
    "Clothing_Preset_Male_Jewellery_Praetorian_T3a_LeatherArmBandsLeft": "Clothing_Preset_Male_Jewellery_Praetorian_T3a_LeatherArmBandsLeft",
    "Clothing_Preset_Male_Accessory_Praetorian_T3a_AthleticArmbandLeft": "Clothing_Preset_Male_Accessory_Praetorian_T3a_AthleticArmbandLeft",
    "Clothing_Preset_Male_Accessory_Praetorian_T3a_Earpiece": "Clothing_Preset_Male_Accessory_Praetorian_T3a_Earpiece",
    "Clothing_Preset_Male_Accessory_Praetorian_T3a_KnifeShoulderHolster": "Clothing_Preset_Male_Accessory_Praetorian_T3a_KnifeShoulderHolster",
    "Clothing_Preset_Male_Belts_Praetorian_T3a_SkullBuckleBelt": "Clothing_Preset_Male_Belts_Praetorian_T3a_SkullBuckleBelt",
    "Clothing_Preset_Male_Facewear_Praetorian_T3a_AviatorSunglasses": "Clothing_Preset_Male_Facewear_Praetorian_T3a_AviatorSunglasses",
    "Clothing_Preset_Male_Footwear_Praetorian_T3a_AnkleBoots": "Clothing_Preset_Male_Footwear_Praetorian_T3a_AnkleBoots",
    "Clothing_Preset_Male_Headwear_Praetorian_T3a_Bandana": "Clothing_Preset_Male_Headwear_Praetorian_T3a_Bandana",
    "Clothing_Preset_Male_Top_Praetorian_T3a_RingerTShirt": "Clothing_Preset_Male_Top_Praetorian_T3a_RingerTShirt",
    "Clothing_Preset_Male_Trousers_Praetorian_T3a_Combats": "Clothing_Preset_Male_Trousers_Praetorian_T3a_Combats",
    "Clothing_Preset_Male_Underwear_Praetorian_T3a_TwoToneBriefs": "Clothing_Preset_Male_Underwear_Praetorian_T3a_TwoToneBriefs",
}

# Reverse mapping
APBDB_TO_ASSET = {v: k for k, v in ASSET_TO_APBDB.items()}


def get_apbdb_id(asset_name: str) -> str | None:
    """Get the APBDB item ID for an extracted asset name."""
    return ASSET_TO_APBDB.get(asset_name)


def get_asset_name(apbdb_id: str) -> str | None:
    """Get the extracted asset name for an APBDB item ID."""
    return APBDB_TO_ASSET.get(apbdb_id)


def get_apbdb_url(asset_name: str) -> str | None:
    """Get the APBDB.com URL for an extracted asset name."""
    apbdb_id = get_apbdb_id(asset_name)
    if apbdb_id:
        return f"https://apbdb.com/items/{apbdb_id}"
    return None