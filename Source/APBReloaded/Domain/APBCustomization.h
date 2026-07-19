#pragma once
#include "APBTypes.h"
#include "APBCatalog.h"
namespace apb {
struct BodyProfile {
	float height = 1.0f;
	float bulk = 0.5f;
	int32_t skin_tone = 0;
	int32_t face_preset = 0;
	int32_t hair_style = 0;
	int32_t hair_color = 0;
	int32_t eye_color = 0;
	std::string face_decal_key;
};
struct ClothingSlot {
	std::string slot;
	std::string item_id;
	int32_t color_primary = 0;
	int32_t color_secondary = 0;
	std::string decal_key;
};
struct CharacterAppearance {
	BodyProfile body;
	std::vector<ClothingSlot> clothing;
	ClothingSlot* FindSlot(const std::string& slot);
	const ClothingSlot* FindSlot(const std::string& slot) const;
	bool Equip(const std::string& slot, const std::string& item_id, int32_t c0 = 0, int32_t c1 = 0, const std::string& decal = "");
	bool Unequip(const std::string& slot);
	std::string Serialize() const;
	static bool Deserialize(const std::string& blob, CharacterAppearance& out);
	bool DiffersFrom(const CharacterAppearance& other) const;
};
struct EquipResult { bool ok = false; std::string error; };
class CustomizationService {
public:
	const Catalog* catalog = nullptr;
	EquipResult EquipFromCatalog(CharacterAppearance& app, const std::string& slot, const std::string& item_id,
		int32_t c0 = 0, int32_t c1 = 0, const std::string& decal = "") const;
	static CharacterAppearance DefaultForFaction(Faction f);
};
}
