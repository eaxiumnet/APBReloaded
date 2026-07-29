#pragma once
// APB chat-channel catalog (slash commands, tags, descriptions, syntax examples), extracted from
// the retail ChatMessageCategories.INT (mirror of the cooked SDD table "ChatMessageCategory") by
// tools/scripts/extract_chat_message_categories.ps1 -> Content/Data/chat_message_categories.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each channel carries five localized strings, all verbatim from the INT:
//   slash_command            primary slash command, e.g. "/c" (may be "DNT - DO NOT TRANSLATE"
//                            for system-only channels that have no player-facing command)
//   secondary_slash_command  long-form alias, e.g. "/clan"
//   tag                      display tag shown in the chat UI, e.g. "Clan"
//   description              player-facing help text describing the channel
//   syntax_example           example usage, e.g. "/c message"
// HasSlashCommand() returns true only when the channel has a real (non-DNT) slash command.
// FindBySlashCommand() resolves either the primary or secondary command (case-insensitive).
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace apb {

struct ChatMessageCategoryDef {
	std::string id;
	std::string slash_command;
	std::string secondary_slash_command;
	std::string tag;
	std::string description;
	std::string syntax_example;
	int32_t order = 0;
};

class ChatMessageCategoryCatalog {
public:
	std::vector<ChatMessageCategoryDef> channels;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			ChatMessageCategoryDef c;
			c.id                      = Unescape(RawStr(obj, "id"));
			c.slash_command           = Unescape(RawStr(obj, "slash_command"));
			c.secondary_slash_command = Unescape(RawStr(obj, "secondary_slash_command"));
			c.tag                     = Unescape(RawStr(obj, "tag"));
			c.description             = Unescape(RawStr(obj, "description"));
			c.syntax_example          = Unescape(RawStr(obj, "syntax_example"));
			c.order                   = (int32_t)RNum(obj, "order", 0);
			if (c.id.empty()) continue;
			auto it = std::find_if(channels.begin(), channels.end(),
				[&](const ChatMessageCategoryDef& e){ return e.id == c.id; });
			if (it == channels.end()) channels.push_back(c);
			else *it = c;
			++touched;
		}
		std::sort(channels.begin(), channels.end(),
			[](const ChatMessageCategoryDef& a, const ChatMessageCategoryDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const ChatMessageCategoryDef* Find(const std::string& id) const {
		for (const auto& c : channels) if (c.id == id) return &c;
		return nullptr;
	}

	// Resolve a slash command (primary or secondary, case-insensitive) to its channel.
	const ChatMessageCategoryDef* FindBySlashCommand(const std::string& cmd) const {
		const std::string lower = ToLower(cmd);
		for (const auto& c : channels) {
			if (!HasSlashCommand(c)) continue;
			if (ToLower(c.slash_command) == lower) return &c;
			if (HasSecondarySlashCommand(c) && ToLower(c.secondary_slash_command) == lower) return &c;
		}
		return nullptr;
	}

	std::string Tag(const std::string& id, const std::string& def = std::string()) const {
		const ChatMessageCategoryDef* c = Find(id);
		return c ? c->tag : def;
	}
	std::string SlashCommand(const std::string& id, const std::string& def = std::string()) const {
		const ChatMessageCategoryDef* c = Find(id);
		return c ? c->slash_command : def;
	}
	std::string SecondarySlashCommand(const std::string& id, const std::string& def = std::string()) const {
		const ChatMessageCategoryDef* c = Find(id);
		return c ? c->secondary_slash_command : def;
	}
	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const ChatMessageCategoryDef* c = Find(id);
		return c ? c->description : def;
	}
	std::string SyntaxExample(const std::string& id, const std::string& def = std::string()) const {
		const ChatMessageCategoryDef* c = Find(id);
		return c ? c->syntax_example : def;
	}

	static bool HasSlashCommand(const ChatMessageCategoryDef& c) {
		return !c.slash_command.empty() && c.slash_command.compare(0, 3, "DNT") != 0;
	}
	static bool HasSecondarySlashCommand(const ChatMessageCategoryDef& c) {
		return !c.secondary_slash_command.empty() && c.secondary_slash_command.compare(0, 3, "DNT") != 0;
	}

	// Count of channels with a real slash command (player-usable, not system-only).
	int32_t PlayerChannelCount() const {
		int32_t n = 0;
		for (const auto& c : channels) if (HasSlashCommand(c)) ++n;
		return n;
	}

	size_t Count() const { return channels.size(); }

private:
	static std::vector<std::string> SplitTopObjects(const std::string& text) {
		std::vector<std::string> out;
		int depth = 0; size_t start = 0; bool inStr = false; bool esc = false;
		for (size_t i = 0; i < text.size(); ++i) {
			char c = text[i];
			if (esc) { esc = false; continue; }
			if (c == '\\' && inStr) { esc = true; continue; }
			if (c == '"') { inStr = !inStr; continue; }
			if (inStr) continue;
			if (c == '{') { if (depth == 0) start = i; ++depth; }
			else if (c == '}') { --depth; if (depth == 0) out.push_back(text.substr(start, i - start + 1)); }
		}
		return out;
	}

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

	static std::string ToLower(const std::string& s) {
		std::string r = s;
		for (auto& c : r) c = (char)std::tolower((unsigned char)c);
		return r;
	}
};

} // namespace apb

