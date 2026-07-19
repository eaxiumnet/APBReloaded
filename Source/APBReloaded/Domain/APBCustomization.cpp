#include "APBCustomization.h"
#include <sstream>
#include <cstdlib>
namespace apb {
ClothingSlot* CharacterAppearance::FindSlot(const std::string& slot) {
	for (auto& c : clothing) if (c.slot == slot) return &c; return nullptr;
}
const ClothingSlot* CharacterAppearance::FindSlot(const std::string& slot) const {
	for (const auto& c : clothing) if (c.slot == slot) return &c; return nullptr;
}
bool CharacterAppearance::Equip(const std::string& slot, const std::string& item_id, int32_t c0, int32_t c1, const std::string& decal) {
	if (slot.empty() || item_id.empty()) return false;
	if (auto* s = FindSlot(slot)) { s->item_id = item_id; s->color_primary = c0; s->color_secondary = c1; s->decal_key = decal; return true; }
	ClothingSlot ns; ns.slot = slot; ns.item_id = item_id; ns.color_primary = c0; ns.color_secondary = c1; ns.decal_key = decal;
	clothing.push_back(ns); return true;
}
bool CharacterAppearance::Unequip(const std::string& slot) {
	for (size_t i = 0; i < clothing.size(); ++i) if (clothing[i].slot == slot) { clothing.erase(clothing.begin() + (std::ptrdiff_t)i); return true; }
	return false;
}
std::string CharacterAppearance::Serialize() const {
	std::ostringstream ss;
	ss << "H=" << body.height << ";B=" << body.bulk << ";SK=" << body.skin_tone << ";FP=" << body.face_preset
	   << ";HS=" << body.hair_style << ";HC=" << body.hair_color << ";EC=" << body.eye_color << ";FD=" << body.face_decal_key << "|";
	for (size_t i = 0; i < clothing.size(); ++i) {
		const auto& c = clothing[i]; if (i) ss << ",";
		ss << c.slot << ":" << c.item_id << ":" << c.color_primary << ":" << c.color_secondary << ":" << c.decal_key;
	}
	return ss.str();
}
static void ParseKVFloat(const std::string& part, const char* key, float& out) {
	std::string k = std::string(key) + "="; size_t p = part.find(k); if (p == std::string::npos) return;
	out = (float)strtod(part.c_str() + p + k.size(), nullptr);
}
static void ParseKVInt(const std::string& part, const char* key, int32_t& out) {
	std::string k = std::string(key) + "="; size_t p = part.find(k); if (p == std::string::npos) return;
	out = (int32_t)strtol(part.c_str() + p + k.size(), nullptr, 10);
}
static void ParseKVStr(const std::string& part, const char* key, std::string& out) {
	std::string k = std::string(key) + "="; size_t p = part.find(k); if (p == std::string::npos) return;
	out = part.substr(p + k.size()); size_t sc = out.find(';'); if (sc != std::string::npos) out = out.substr(0, sc);
}
bool CharacterAppearance::Deserialize(const std::string& blob, CharacterAppearance& out) {
	out = CharacterAppearance{};
	size_t bar = blob.find('|');
	std::string head = bar == std::string::npos ? blob : blob.substr(0, bar);
	std::string tail = bar == std::string::npos ? std::string() : blob.substr(bar + 1);
	ParseKVFloat(head, "H", out.body.height); ParseKVFloat(head, "B", out.body.bulk);
	ParseKVInt(head, "SK", out.body.skin_tone); ParseKVInt(head, "FP", out.body.face_preset);
	ParseKVInt(head, "HS", out.body.hair_style); ParseKVInt(head, "HC", out.body.hair_color);
	ParseKVInt(head, "EC", out.body.eye_color); ParseKVStr(head, "FD", out.body.face_decal_key);
	if (!tail.empty()) {
		size_t i = 0;
		while (i < tail.size()) {
			size_t j = tail.find(',', i);
			std::string piece = tail.substr(i, j == std::string::npos ? std::string::npos : j - i);
			std::vector<std::string> f; size_t a = 0;
			while (a <= piece.size()) {
				size_t b = piece.find(':', a);
				f.push_back(piece.substr(a, b == std::string::npos ? std::string::npos : b - a));
				if (b == std::string::npos) break; a = b + 1;
			}
			if (f.size() >= 2) {
				ClothingSlot s; s.slot = f[0]; s.item_id = f[1];
				if (f.size() > 2) s.color_primary = (int32_t)strtol(f[2].c_str(), nullptr, 10);
				if (f.size() > 3) s.color_secondary = (int32_t)strtol(f[3].c_str(), nullptr, 10);
				if (f.size() > 4) s.decal_key = f[4];
				out.clothing.push_back(s);
			}
			if (j == std::string::npos) break; i = j + 1;
		}
	}
	return true;
}
bool CharacterAppearance::DiffersFrom(const CharacterAppearance& o) const {
	if (body.height != o.body.height || body.bulk != o.body.bulk) return true;
	if (body.skin_tone != o.body.skin_tone || body.face_preset != o.body.face_preset) return true;
	if (body.hair_style != o.body.hair_style || body.hair_color != o.body.hair_color) return true;
	if (body.eye_color != o.body.eye_color || body.face_decal_key != o.body.face_decal_key) return true;
	if (clothing.size() != o.clothing.size()) return true;
	for (size_t i = 0; i < clothing.size(); ++i) {
		if (clothing[i].slot != o.clothing[i].slot || clothing[i].item_id != o.clothing[i].item_id) return true;
		if (clothing[i].color_primary != o.clothing[i].color_primary || clothing[i].color_secondary != o.clothing[i].color_secondary) return true;
		if (clothing[i].decal_key != o.clothing[i].decal_key) return true;
	}
	return false;
}
EquipResult CustomizationService::EquipFromCatalog(CharacterAppearance& app, const std::string& slot, const std::string& item_id,
	int32_t c0, int32_t c1, const std::string& decal) const {
	EquipResult r; if (!catalog) { r.error = "no_catalog"; return r; }
	if (!catalog->FindItem(item_id)) { r.error = "unknown_item"; return r; }
	if (!app.Equip(slot, item_id, c0, c1, decal)) { r.error = "equip_failed"; return r; }
	r.ok = true; return r;
}
CharacterAppearance CustomizationService::DefaultForFaction(Faction f) {
	CharacterAppearance a; a.body.height = 1.0f; a.body.bulk = f == Faction::Enforcer ? 0.55f : 0.5f;
	a.body.skin_tone = 1; a.body.face_preset = f == Faction::Enforcer ? 2 : 1;
	a.body.hair_style = 1; a.body.hair_color = f == Faction::Enforcer ? 2 : 5; a.body.eye_color = 1; return a;
}
}
