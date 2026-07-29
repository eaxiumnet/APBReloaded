#pragma once
// APB role-MILESTONE catalog (the individual ranks/steps of a player role: as you progress a role
// you clear milestones, each with a display Title and — for the ones that grant loot — a reward-mail
// Subject + Body), extracted from the retail RoleMilestones.INT (mirror of the cooked SDD table
// "RoleMilestones") by tools/scripts/extract_role_milestones.ps1 -> Content/Data/role_milestones.json.
//
// Header-only (matches MedalCatalog / ModifierEffectCatalog / ModifierItemTypeCatalog): every method
// is defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// A milestone id carries a trailing "_<NN>" rank (e.g. "15th_Anniversary_Celebrations_01"); the base
// part maps back to a player_roles id via RoleId(), and Rank() parses the trailing number. Not every
// milestone binds to a player_roles row (event/legacy milestones without a matching role), so callers
// should Find() the derived role id in the roles catalog and tolerate a miss.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct RoleMilestone {
	std::string id;                   // "15th_Anniversary_Celebrations_01", "Ach_BackUp_01", ...
	std::string title;                // "15th Year Anniversary Celebrations - Rank 1"
	std::string reward_mail_subject;  // "" unless the milestone grants a mailed reward
	std::string reward_mail_body;     // "" unless the milestone grants a mailed reward
	int32_t order = 0;                // stable display order (file order)
};

class RoleMilestoneCatalog {
public:
	std::vector<RoleMilestone> items; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge: existing ids are updated in place; new ids appended. Never clears on empty
	// input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			RoleMilestone it;
			it.id                  = Unescape(RawStr(obj, "id"));
			it.title               = Unescape(RawStr(obj, "title"));
			it.reward_mail_subject = Unescape(RawStr(obj, "reward_mail_subject"));
			it.reward_mail_body    = Unescape(RawStr(obj, "reward_mail_body"));
			it.order               = (int32_t)RNum(obj, "order", 0);
			if (it.id.empty()) continue;
			auto existing = std::find_if(items.begin(), items.end(),
				[&](const RoleMilestone& x){ return x.id == it.id; });
			if (existing == items.end()) items.push_back(it);
			else *existing = it;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const RoleMilestone& a, const RoleMilestone& b){ return a.order < b.order; });
		return touched > 0;
	}

	const RoleMilestone* Find(const std::string& id) const {
		for (const auto& it : items) if (it.id == id) return &it;
		return nullptr;
	}

	std::string Title(const std::string& id, const std::string& def = std::string()) const {
		const RoleMilestone* it = Find(id);
		return it ? it->title : def;
	}
	std::string RewardSubject(const std::string& id, const std::string& def = std::string()) const {
		const RoleMilestone* it = Find(id);
		return it ? it->reward_mail_subject : def;
	}
	std::string RewardBody(const std::string& id, const std::string& def = std::string()) const {
		const RoleMilestone* it = Find(id);
		return it ? it->reward_mail_body : def;
	}

	// True if the milestone grants a mailed reward (non-empty subject or body).
	bool HasReward(const std::string& id) const {
		const RoleMilestone* it = Find(id);
		return it && (!it->reward_mail_subject.empty() || !it->reward_mail_body.empty());
	}

	// All milestones belonging to a role (base id), sorted by rank then display order.
	std::vector<const RoleMilestone*> ForRole(const std::string& roleId) const {
		std::vector<const RoleMilestone*> out;
		for (const auto& it : items) if (RoleId(it.id) == roleId) out.push_back(&it);
		std::sort(out.begin(), out.end(), [](const RoleMilestone* a, const RoleMilestone* b){
			int ra = Rank(a->id), rb = Rank(b->id);
			if (ra != rb) return ra < rb;
			return a->order < b->order;
		});
		return out;
	}

	int32_t Count() const { return (int32_t)items.size(); }

	// The player_roles id a milestone maps to: strip a trailing "_<digits>" rank suffix.
	// e.g. "15th_Anniversary_Celebrations_01" -> "15th_Anniversary_Celebrations". A milestone id
	// with no numeric suffix is returned unchanged. Static: usable without a catalog instance.
	static std::string RoleId(const std::string& id) {
		size_t end = id.size();
		size_t i = end;
		while (i > 0 && id[i - 1] >= '0' && id[i - 1] <= '9') --i;
		// Require at least one digit AND a preceding underscore to treat it as a rank suffix.
		if (i < end && i > 0 && id[i - 1] == '_') return id.substr(0, i - 1);
		return id;
	}

	// The rank number parsed from a milestone's trailing "_<digits>"; -1 if none.
	static int32_t Rank(const std::string& id) {
		size_t end = id.size();
		size_t i = end;
		while (i > 0 && id[i - 1] >= '0' && id[i - 1] <= '9') --i;
		if (i < end && i > 0 && id[i - 1] == '_')
			return (int32_t)std::strtol(id.c_str() + i, nullptr, 10);
		return -1;
	}

	// Convenience: the role id / rank for a stored milestone (empty / -1 if unknown id).
	std::string RoleIdFor(const std::string& id) const {
		const RoleMilestone* it = Find(id);
		return it ? RoleId(it->id) : std::string();
	}
	int32_t RankFor(const std::string& id) const {
		const RoleMilestone* it = Find(id);
		return it ? Rank(it->id) : -1;
	}

private:
	// Depth-aware {...} splitter that respects JSON strings and escapes.
	static std::vector<std::string> SplitTopObjects(const std::string& text) {
		std::vector<std::string> out;
		int depth = 0; bool inStr = false, esc = false; size_t start = 0;
		for (size_t i = 0; i < text.size(); ++i) {
			char c = text[i];
			if (inStr) {
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') { inStr = true; continue; }
			if (c == '{') { if (depth == 0) start = i; ++depth; }
			else if (c == '}') { --depth; if (depth == 0) out.push_back(text.substr(start, i - start + 1)); }
		}
		return out;
	}

	// Returns the RAW (still-escaped) string value for "key" within a single object.
	static std::string RawStr(const std::string& obj, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return std::string();
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return std::string();
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r')) ++i;
		if (i >= obj.size() || obj[i] != '"') return std::string();
		return ReadStringAt(obj, i);
	}

	// Reads a JSON string starting at the opening quote index; returns the raw (escaped) contents.
	static std::string ReadStringAt(const std::string& obj, size_t quotePos) {
		std::string raw; bool esc = false;
		for (size_t i = quotePos + 1; i < obj.size(); ++i) {
			char c = obj[i];
			if (esc) { raw.push_back('\\'); raw.push_back(c); esc = false; }
			else if (c == '\\') esc = true;
			else if (c == '"') break;
			else raw.push_back(c);
		}
		return raw;
	}

	// Numeric value for "key" (strtod). Returns def if absent.
	static double RNum(const std::string& obj, const std::string& key, double def) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return def;
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return def;
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) ++i;
		if (i >= obj.size()) return def;
		return std::strtod(obj.c_str() + i, nullptr);
	}

	// Proper JSON string unescape: \" \\ \/ \b \f \n \r \t and \uXXXX (-> UTF-8).
	static std::string Unescape(const std::string& raw) {
		std::string out; out.reserve(raw.size());
		for (size_t i = 0; i < raw.size(); ++i) {
			char c = raw[i];
			if (c != '\\') { out.push_back(c); continue; }
			if (i + 1 >= raw.size()) { out.push_back('\\'); break; }
			char n = raw[++i];
			switch (n) {
				case 'n': out.push_back('\n'); break;
				case 'r': out.push_back('\r'); break;
				case 't': out.push_back('\t'); break;
				case 'b': out.push_back('\b'); break;
				case 'f': out.push_back('\f'); break;
				case '/': out.push_back('/'); break;
				case '"': out.push_back('"'); break;
				case '\\': out.push_back('\\'); break;
				case 'u': {
					if (i + 4 < raw.size()) {
						unsigned code = 0; bool ok = true;
						for (int d = 1; d <= 4; ++d) {
							char h = raw[i + d]; unsigned v;
							if (h >= '0' && h <= '9') v = (unsigned)(h - '0');
							else if (h >= 'a' && h <= 'f') v = (unsigned)(h - 'a' + 10);
							else if (h >= 'A' && h <= 'F') v = (unsigned)(h - 'A' + 10);
							else { ok = false; break; }
							code = (code << 4) | v;
						}
						if (ok) { i += 4; AppendUtf8(out, code); break; }
					}
					out.push_back('u');
					break;
				}
				default: out.push_back(n); break;
			}
		}
		return out;
	}

	static void AppendUtf8(std::string& out, unsigned cp) {
		if (cp <= 0x7F) out.push_back((char)cp);
		else if (cp <= 0x7FF) {
			out.push_back((char)(0xC0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		} else {
			out.push_back((char)(0xE0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
};

} // namespace apb
