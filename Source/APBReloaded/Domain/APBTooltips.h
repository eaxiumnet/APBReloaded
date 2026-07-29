#pragma once
// APB UI TOOLTIP catalog — the retail hover-tooltip text for the frontend/menu UI, grouped by scene. Unlike
// the other localization catalogs (flat `<Table>_<id>_<Suffix>` grammar) this one is SECTION-SCOPED: the
// retail Tooltips.INT groups tooltips under `[<Scene>]` headers and keys them `<Scene>@<Widget>=<text>`
// (e.g. scene "Login_Scene", widget "UILabelButton_TOS" -> "Create a new APB Account."). A UI widget looks up
// its hover hint by (scene, widget).
//
// Extracted from the retail Tooltips.INT by tools/scripts/extract_tooltips.ps1 ->
// Content/Data/tooltips.json, FLATTENED to one row per (scene, widget): 412 keys across 54 scenes -> 409 rows
// (3 empty placeholder widgets dropped; 53 scenes retain at least one tooltip). Text is short plain-text hover
// help and is preserved VERBATIM.
//
// Header-only (matches the other Domain catalogs): every method is defined in-class so it is implicitly
// inline and safe to include in multiple TUs.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct TooltipEntry {
	std::string scene;   // UI scene / section ("Login_Scene", "VehicleUI_Main", "SymbolEditor_0001", ...)
	std::string widget;  // widget id within the scene ("UILabelButton_TOS", "RotationSliders", ...)
	std::string text;    // the hover tooltip shown for that widget
	int32_t order = 0;   // stable display order (file order)
};

class TooltipCatalog {
public:
	std::vector<TooltipEntry> items; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge keyed by (scene, widget): an existing pair is updated in place; a new one is appended.
	// Never clears on empty input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			TooltipEntry r;
			r.scene  = Unescape(RawStr(obj, "scene"));
			r.widget = Unescape(RawStr(obj, "widget"));
			r.text   = Unescape(RawStr(obj, "text"));
			r.order  = (int32_t)RNum(obj, "order", 0);
			if (r.scene.empty() || r.widget.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const TooltipEntry& e){ return e.scene == r.scene && e.widget == r.widget; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const TooltipEntry& a, const TooltipEntry& b){ return a.order < b.order; });
		return touched > 0;
	}

	// A specific (scene, widget). Returns nullptr if absent.
	const TooltipEntry* Find(const std::string& scene, const std::string& widget) const {
		for (const auto& r : items) if (r.scene == scene && r.widget == widget) return &r;
		return nullptr;
	}

	// The hover tooltip for a (scene, widget), or def when the pair has no entry.
	std::string TooltipFor(const std::string& scene, const std::string& widget, const std::string& def = std::string()) const {
		const TooltipEntry* r = Find(scene, widget);
		return (r && !r->text.empty()) ? r->text : def;
	}

	// All tooltips for a scene, in file order. Empty if the scene is unknown.
	std::vector<const TooltipEntry*> ForScene(const std::string& scene) const {
		std::vector<const TooltipEntry*> out;
		for (const auto& r : items) if (r.scene == scene) out.push_back(&r);
		std::sort(out.begin(), out.end(),
			[](const TooltipEntry* a, const TooltipEntry* b){ return a->order < b->order; });
		return out;
	}

	// Number of tooltips a scene ships (0 if unknown).
	int32_t SceneTooltipCount(const std::string& scene) const {
		int32_t n = 0;
		for (const auto& r : items) if (r.scene == scene) ++n;
		return n;
	}

	// Distinct scenes with at least one tooltip.
	int32_t SceneCount() const {
		std::vector<std::string> seen;
		for (const auto& r : items)
			if (std::find(seen.begin(), seen.end(), r.scene) == seen.end()) seen.push_back(r.scene);
		return (int32_t)seen.size();
	}

	// Total tooltip rows across all scenes.
	int32_t Count() const { return (int32_t)items.size(); }

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

	// Returns the RAW (still-escaped) string value for "key" within a single object, honouring
	// backslash escapes so an embedded \" does not terminate early.
	static std::string RawStr(const std::string& obj, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return std::string();
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return std::string();
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r')) ++i;
		if (i >= obj.size() || obj[i] != '"') return std::string();
		++i;
		std::string raw; bool esc = false;
		for (; i < obj.size(); ++i) {
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
