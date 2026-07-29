// APBProgression.cpp — M15 (brief #14): implementation of the economy & progression Domain
// service declared in APBProgression.h.
//
// JSON parsing reuses the public helpers declared in APBCatalog.h
// (JsonGetString/JsonSplitObjects, defined in APBCatalog.cpp — link it), plus a local file
// reader (APBCatalog's ReadFile has internal linkage and is not visible here).
#include "APBProgression.h"
#include "APBCatalog.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>

namespace apb {
namespace {

std::string ReadWholeFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf();
	return ss.str();
}

} // namespace

std::string DistrictFromContactId(const std::string& id) {
	size_t u = id.find('_');
	return u == std::string::npos ? std::string() : id.substr(0, u);
}

std::string NormalizeContactId(const std::string& id) {
	// Strip leading zeros from a trailing digit run: "Financial_C01" -> "Financial_C1",
	// "Waterfront_E12" -> "Waterfront_E12", "Binky" -> "Binky" (no trailing digits).
	size_t end = id.size(), i = end;
	while (i > 0 && std::isdigit((unsigned char)id[i - 1])) --i;
	if (i == end) return id; // no trailing digits
	const std::string prefix = id.substr(0, i);
	const std::string digits = id.substr(i);
	const size_t nz = digits.find_first_not_of('0');
	const std::string trimmed = (nz == std::string::npos) ? std::string("0") : digits.substr(nz);
	return prefix + trimmed;
}

bool ProgressionCatalog::LoadContactsFromText(const std::string& json) {
	int added = 0;
	for (const auto& obj : JsonSplitObjects(json)) {
		ContactDef c;
		c.id = JsonGetString(obj, "id");
		if (c.id.empty()) continue;
		c.title = JsonGetString(obj, "title", c.id);
		c.district = DistrictFromContactId(c.id);
		contacts[c.id] = c;
		++added;
	}
	return added > 0;
}

bool ProgressionCatalog::LoadRolesFromText(const std::string& json) {
	int added = 0;
	for (const auto& obj : JsonSplitObjects(json)) {
		RoleDef r;
		r.id = JsonGetString(obj, "id");
		if (r.id.empty()) continue;
		r.name = JsonGetString(obj, "name", r.id);
		r.description = JsonGetString(obj, "description");
		roles[r.id] = r;
		++added;
	}
	return added > 0;
}

bool ProgressionCatalog::LoadContactsFromFile(const std::string& path) {
	const std::string text = ReadWholeFile(path);
	if (text.empty()) return false;
	return LoadContactsFromText(text);
}

bool ProgressionCatalog::LoadRolesFromFile(const std::string& path) {
	const std::string text = ReadWholeFile(path);
	if (text.empty()) return false;
	return LoadRolesFromText(text);
}

bool ProgressionCatalog::LoadContactLevelsFromText(const std::string& json) {
	int added = 0;
	for (const auto& obj : JsonSplitObjects(json)) {
		const std::string id = JsonGetString(obj, "contact_id");
		if (id.empty()) continue;
		const int32_t ml = (int32_t)std::llround(JsonGetNumber(obj, "max_level", 0.0));
		if (ml <= 0) continue;
		// Keep the highest level seen per normalized id (merge-friendly / idempotent).
		int32_t& slot = contact_max_level[NormalizeContactId(id)];
		if (ml > slot) slot = ml;
		++added;
	}
	return added > 0;
}

bool ProgressionCatalog::LoadContactLevelsFromFile(const std::string& path) {
	const std::string text = ReadWholeFile(path);
	if (text.empty()) return false;
	return LoadContactLevelsFromText(text);
}

int32_t ProgressionCatalog::ContactMaxLevel(const std::string& contact_id) const {
	auto it = contact_max_level.find(NormalizeContactId(contact_id));
	return it == contact_max_level.end() ? 0 : it->second;
}

LevelLadder ProgressionCatalog::LadderForContact(const std::string& contact_id) const {
	const int32_t ml = ContactMaxLevel(contact_id);
	return ml > 0 ? LevelLadder::ContactLadderWithMaxLevel(ml) : LevelLadder::DefaultContactLadder();
}

std::vector<const ContactDef*> ProgressionCatalog::ContactsInDistrict(const std::string& district) const {
	std::vector<const ContactDef*> out;
	for (const auto& kv : contacts) {
		if (kv.second.district == district) out.push_back(&kv.second);
	}
	return out;
}

LevelLadder LevelLadder::DefaultContactLadder() {
	// Levels 0..15 — a tunable recreation default until a retail table is recovered.
	return ContactLadderWithMaxLevel(15);
}

LevelLadder LevelLadder::ContactLadderWithMaxLevel(int32_t max_level) {
	// Cumulative standing grows super-linearly (early levels cheap, later levels expensive).
	// Sized to max_level so a contact's real retail level count drives ladder length.
	LevelLadder l;
	if (max_level < 0) max_level = 0;
	l.thresholds.push_back(0);
	int64_t step = 1000;
	int64_t cum = 0;
	for (int i = 1; i <= max_level; ++i) {
		cum += step;
		l.thresholds.push_back(cum);
		step += 1000; // 1000, 2000, 3000, ... increments
	}
	return l;
}


int32_t LevelLadder::LevelFor(int64_t standing) const {
	if (thresholds.empty()) return 0;
	int32_t best = 0;
	for (int32_t i = 0; i < (int32_t)thresholds.size(); ++i) {
		if (standing >= thresholds[i]) best = i;
		else break;
	}
	return best;
}

int64_t LevelLadder::StandingForLevel(int32_t level) const {
	if (thresholds.empty()) return 0;
	if (level < 0) level = 0;
	if (level > MaxLevel()) level = MaxLevel();
	return thresholds[level];
}

int64_t CharacterProgress::ContactStanding(const std::string& contact_id) const {
	auto it = contact_standing.find(contact_id);
	return it == contact_standing.end() ? 0 : it->second;
}

int64_t CharacterProgress::RoleXp(const std::string& role_id) const {
	auto it = role_xp.find(role_id);
	return it == role_xp.end() ? 0 : it->second;
}

int64_t CharacterProgress::AddContactStanding(const std::string& contact_id, int64_t amount) {
	if (amount > 0) contact_standing[contact_id] += amount;
	return ContactStanding(contact_id);
}

int64_t CharacterProgress::AddRoleXp(const std::string& role_id, int64_t amount) {
	if (amount > 0) role_xp[role_id] += amount;
	return RoleXp(role_id);
}

int32_t CharacterProgress::ContactLevel(const std::string& contact_id, const LevelLadder& ladder) const {
	return ladder.LevelFor(ContactStanding(contact_id));
}

MissionReward ComputeMissionReward(int64_t base_cash, int64_t base_standing,
	int64_t base_role_xp, double threat_reward_multiplier) {
	if (threat_reward_multiplier < 0.0) threat_reward_multiplier = 0.0;
	MissionReward r;
	auto scale = [&](int64_t base) -> int64_t {
		if (base <= 0) return 0;
		double v = (double)base * threat_reward_multiplier;
		int64_t out = (int64_t)std::llround(v);
		return out < 0 ? 0 : out;
	};
	r.cash = scale(base_cash);
	r.standing = scale(base_standing);
	r.role_xp = base_role_xp > 0 ? base_role_xp : 0; // flat, not threat-scaled
	return r;
}

bool IsUnlocked(const CharacterProgress& progress, const LevelLadder& ladder,
	const ContactUnlock& unlock) {
	return progress.ContactLevel(unlock.contact_id, ladder) >= unlock.required_level;
}

std::vector<std::string> UnlockedItems(const CharacterProgress& progress,
	const LevelLadder& ladder, const std::vector<ContactUnlock>& unlocks) {
	std::vector<std::string> out;
	for (const auto& u : unlocks) {
		if (IsUnlocked(progress, ladder, u)) out.push_back(u.item_id);
	}
	return out;
}

bool TrySpend(int64_t& balance, int64_t cost) {
	if (cost < 0) return false;
	if (balance < cost) return false;
	balance -= cost;
	return true;
}

} // namespace apb
