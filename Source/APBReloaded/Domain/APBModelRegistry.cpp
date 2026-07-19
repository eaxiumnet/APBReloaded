#include "APBModelRegistry.h"
#include <cstdlib>
namespace apb {
namespace {
std::string ReadAll(const std::string& path) {
	std::ifstream in(path, std::ios::binary); if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
std::string JStr(const std::string& obj, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return {};
	p = obj.find(':', p); if (p == std::string::npos) return {};
	p = obj.find(char(34), p + 1); if (p == std::string::npos) return {};
	size_t e = p + 1; std::string out;
	while (e < obj.size() && obj[e] != char(34)) {
		if (obj[e] == char(92) && e + 1 < obj.size()) { out.push_back(obj[e + 1]); e += 2; continue; }
		out.push_back(obj[e++]);
	}
	return out;
}
int64_t JInt(const std::string& obj, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return 0;
	p = obj.find(':', p); if (p == std::string::npos) return 0; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	return (int64_t)strtoll(obj.c_str() + p, nullptr, 10);
}
std::vector<std::string> SplitObjs(const std::string& text) {
	std::vector<std::string> out; int d = 0; size_t s = std::string::npos;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '{') { if (d == 0) s = i; ++d; }
		else if (text[i] == '}') { --d; if (d == 0 && s != std::string::npos) { out.push_back(text.substr(s, i - s + 1)); s = std::string::npos; } }
	}
	return out;
}
std::string Arr(const std::string& text, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = text.find(pat); if (p == std::string::npos) return {};
	p = text.find('[', p); if (p == std::string::npos) return {};
	int d = 0; size_t s = p;
	for (size_t i = p; i < text.size(); ++i) {
		if (text[i] == '[') ++d;
		else if (text[i] == ']') { --d; if (d == 0) return text.substr(s, i - s + 1); }
	}
	return {};
}
}
bool ModelRegistry::LoadFromFile(const std::string& path) {
	std::string text = ReadAll(path); if (text.empty()) return false;
	vehicles.clear(); characters.clear();
	source_install = JStr(text.substr(0, std::min<size_t>(text.size(), 800)), "source_install");
	// fallback full-file search for top keys using first object-like slice is enough for tests
	if (source_install.empty()) source_install = JStr(text, "source_install");
	umodel_path = JStr(text, "umodel");
	game_tag = JStr(text, "umodel_game_tag");
	if (game_tag.empty()) game_tag = "apb";
	for (const auto& obj : SplitObjs(Arr(text, "vehicles"))) {
		ModelRef r; r.package = JStr(obj, "package"); r.rel_path = JStr(obj, "rel_path");
		r.family = JStr(obj, "family"); r.ue5_import_hint = JStr(obj, "ue5_import_hint"); r.size = JInt(obj, "size");
		if (!r.package.empty()) vehicles.push_back(r);
	}
	for (const auto& obj : SplitObjs(Arr(text, "characters"))) {
		ModelRef r; r.package = JStr(obj, "package"); r.rel_path = JStr(obj, "rel_path");
		r.ue5_import_hint = JStr(obj, "ue5_import_hint"); r.size = JInt(obj, "size");
		if (!r.package.empty()) characters.push_back(r);
	}
	return !vehicles.empty() || !characters.empty();
}
const ModelRef* ModelRegistry::FindVehiclePackage(const std::string& packageName) const {
	for (const auto& v : vehicles) if (v.package == packageName) return &v; return nullptr;
}
const ModelRef* ModelRegistry::FindByFamily(const std::string& family) const {
	for (const auto& v : vehicles) if (v.family == family) return &v; return nullptr;
}
}
