#pragma once
// APB modification-effect description catalog (the multi-line, colour-marked-up tooltips shown for
// character / vehicle / weapon / consumable modifications in the modification screen and item
// inspector), extracted from the retail ModifierEffects.INT (mirror of the cooked SDD table
// "ModifierEffect") by tools/scripts/extract_modifier_effects.ps1 ->
// Content/Data/modifier_effects.json.
//
// Header-only (matches MedalCatalog / AmmoCategoryCatalog / HUDCombatMessageCatalog): every method
// is defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// Each mod owns one OR MORE description lines, all VERBATIM from the INT, including the inline
// "<Color:R=.. G=.. B=..>" markup that recolours the text that follows it. The markup-aware parser
// lives here: ParseSegments() turns a raw line into coloured text runs the UE tooltip can render,
// and PlainText() strips the markup to the readable string. Unknown/closing tags (e.g. "</Col>")
// and colour tags with swapped channels ("<Color:R=1 B=1 G=1>") or a leading space
// ("<Color: R=..>") are all handled; a malformed tag with no closing '>' is treated as literal.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

// One coloured run of text within a description line. Colour channels are 0..1 floats; the default
// (before any "<Color:...>" tag) is white (1,1,1).
struct ColorSegment {
	std::string text;
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
};

struct ModifierEffect {
	std::string id;                    // "Weapon_Bandolier1", "Vehicle_Nitro3", "Character_Kevlar2", ...
	std::string category;              // "Character" / "Vehicle" / "Weapon" / "Usable"
	std::vector<std::string> lines;    // raw markup lines, verbatim, in display order
	int32_t order = 0;                 // stable display order (file order)
};

class ModifierEffectCatalog {
public:
	std::vector<ModifierEffect> effects; // sorted by order

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
			ModifierEffect e;
			e.id       = Unescape(RawStr(obj, "id"));
			e.category = Unescape(RawStr(obj, "category"));
			e.order    = (int32_t)RNum(obj, "order", 0);
			for (const std::string& raw : RawStrArray(obj, "lines")) e.lines.push_back(Unescape(raw));
			if (e.id.empty()) continue;
			auto it = std::find_if(effects.begin(), effects.end(),
				[&](const ModifierEffect& x){ return x.id == e.id; });
			if (it == effects.end()) effects.push_back(e);
			else *it = e;
			++touched;
		}
		std::sort(effects.begin(), effects.end(),
			[](const ModifierEffect& a, const ModifierEffect& b){ return a.order < b.order; });
		return touched > 0;
	}

	const ModifierEffect* Find(const std::string& id) const {
		for (const auto& e : effects) if (e.id == id) return &e;
		return nullptr;
	}

	// Raw markup lines for a mod (empty vector if unknown).
	std::vector<std::string> Lines(const std::string& id) const {
		const ModifierEffect* e = Find(id);
		return e ? e->lines : std::vector<std::string>();
	}
	int32_t LineCount(const std::string& id) const {
		const ModifierEffect* e = Find(id);
		return e ? (int32_t)e->lines.size() : 0;
	}

	// Markup-stripped lines for a mod (readable text, no "<Color:...>").
	std::vector<std::string> PlainLines(const std::string& id) const {
		std::vector<std::string> out;
		const ModifierEffect* e = Find(id);
		if (e) for (const auto& l : e->lines) out.push_back(PlainText(l));
		return out;
	}

	// All mods of a category ("Character"/"Vehicle"/"Weapon"/"Usable"), in display order.
	std::vector<const ModifierEffect*> ForCategory(const std::string& category) const {
		std::vector<const ModifierEffect*> out;
		for (const auto& e : effects) if (e.category == category) out.push_back(&e);
		return out;
	}

	// Distinct categories present, sorted.
	std::vector<std::string> Categories() const {
		std::vector<std::string> out;
		for (const auto& e : effects)
			if (std::find(out.begin(), out.end(), e.category) == out.end()) out.push_back(e.category);
		std::sort(out.begin(), out.end());
		return out;
	}

	int32_t Count() const { return (int32_t)effects.size(); }

	// --- markup-aware helpers (static: usable without a catalog instance) ------------------------

	// Parse a raw description line into coloured text runs. Text before the first "<Color:...>" is
	// white (1,1,1); each colour tag recolours everything after it until the next tag. Non-colour
	// tags (e.g. "</Col>") are dropped from the output; a "<" with no closing ">" is literal.
	static std::vector<ColorSegment> ParseSegments(const std::string& line) {
		std::vector<ColorSegment> out;
		float r = 1.0f, g = 1.0f, b = 1.0f;
		std::string buf;
		auto flush = [&]() {
			if (!buf.empty()) { out.push_back(ColorSegment{ buf, r, g, b }); buf.clear(); }
		};
		size_t i = 0;
		while (i < line.size()) {
			char c = line[i];
			if (c == '<') {
				size_t gt = line.find('>', i + 1);
				if (gt == std::string::npos) { buf.append(line, i, line.size() - i); break; }
				std::string inner = line.substr(i + 1, gt - i - 1);
				size_t s = 0; while (s < inner.size() && inner[s] == ' ') ++s;
				if (inner.compare(s, 6, "Color:") == 0) {
					float nr = r, ng = g, nb = b;
					ParseChannel(inner, "R=", nr);
					ParseChannel(inner, "G=", ng);
					ParseChannel(inner, "B=", nb);
					flush();                 // accumulated text keeps the colour that was active
					r = nr; g = ng; b = nb;  // then switch to the new colour
				}
				// else: unknown/closing tag -> consumed and dropped from rendered text
				i = gt + 1;
				continue;
			}
			buf.push_back(c);
			++i;
		}
		flush();
		return out;
	}

	// The readable text of a line with all markup removed.
	static std::string PlainText(const std::string& line) {
		std::string out;
		for (const auto& seg : ParseSegments(line)) out += seg.text;
		return out;
	}

private:
	// Overwrites v with the float following "key" (a 2-char token like "R=") inside a colour tag,
	// if present; leaves v unchanged otherwise. Tolerant of channel order and a leading space.
	static void ParseChannel(const std::string& inner, const char* key, float& v) {
		size_t p = inner.find(key);
		if (p == std::string::npos) return;
		v = (float)std::strtod(inner.c_str() + p + 2, nullptr);
	}

	// Depth-aware {...} splitter that respects JSON strings and escapes. Array brackets inside an
	// object ("lines": [ ... ]) contain no braces, so top-level object splitting is unaffected.
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

	// Returns the RAW (still-escaped) string elements of the array value for "key". Tolerates a
	// scalar string value too (single element), so it is immune to PowerShell ConvertTo-Json
	// unwrapping a single-element array into a bare string.
	static std::vector<std::string> RawStrArray(const std::string& obj, const std::string& key) {
		std::vector<std::string> out;
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return out;
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return out;
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r')) ++i;
		if (i >= obj.size()) return out;
		if (obj[i] == '[') {
			++i;
			while (i < obj.size()) {
				while (i < obj.size() && obj[i] != '"' && obj[i] != ']') ++i;
				if (i >= obj.size() || obj[i] == ']') break;
				std::string raw = ReadStringAt(obj, i); // starts at the opening quote
				out.push_back(raw);
				// advance past the string we just consumed
				i = SkipStringAt(obj, i);
			}
		} else if (obj[i] == '"') {
			out.push_back(ReadStringAt(obj, i));
		}
		return out;
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

	// Returns the index just past the closing quote of the JSON string starting at quotePos.
	static size_t SkipStringAt(const std::string& obj, size_t quotePos) {
		bool esc = false;
		size_t i = quotePos + 1;
		for (; i < obj.size(); ++i) {
			char c = obj[i];
			if (esc) esc = false;
			else if (c == '\\') esc = true;
			else if (c == '"') { ++i; break; }
		}
		return i;
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
