#pragma once
// Pure placement resolve / metrics (no UE). Shared by freeroam loader concepts and domain tests.
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace apb {

struct PlacementVec3 {
	double x = 0, y = 0, z = 0;
};

struct PlacementEntryPure {
	std::string mesh_id;
	std::string ue_path;
	std::string package;
	PlacementVec3 location;
	PlacementVec3 scale{1, 1, 1};
};

struct DistrictManifestPure {
	std::string district_id;
	std::string source_package;
	PlacementVec3 player_start{-200, -1200, 120};
	PlacementVec3 vehicle_start{400, -1200, 50};
	std::vector<PlacementEntryPure> placements;
	int bound_count = 0;
	int manifest_total = 0;
	double hit_rate = 0;
	bool loaded_bound = false;
	int stream_chunk_count = 0;
};

/** Map name → district id (mirrors UAPBDistrictPlacementLoader::ResolveDistrictIdFromMapName). */
inline std::string ResolveDistrictIdFromMapName(const std::string& mapName) {
	auto has = [&](const char* s) {
		return mapName.find(s) != std::string::npos;
	};
	if (has("Waterfront")) return "Waterfront";
	if (has("Asylum") || has("Abington")) return "PGAsylum";
	if (has("Beacon")) return "PGBeacon";
	if (has("Crate")) return "PGCrate";
	if (has("Social") || has("Breakwater")) return "Social";
	if (has("FinancialChaos")) return "FinancialChaos";
	if (has("FinancialRiot")) return "FinancialRiot";
	if (has("Financial")) return "Financial";
	return "Financial";
}

/** District id → placement file base name (without .json / _bound). */
inline std::string PlacementBaseNameForDistrict(const std::string& districtId) {
	if (districtId.find("Waterfront") != std::string::npos) return "Waterfront_Block05";
	if (districtId.find("Asylum") != std::string::npos || districtId.find("PGAsylum") != std::string::npos)
		return "Asylum_Block";
	if (districtId.find("Beacon") != std::string::npos) return "Beacon_Block";
	if (districtId.find("Crate") != std::string::npos) return "Crate_Block";
	if (districtId.find("Social") != std::string::npos) return "Social_Block";
	return "Financial_Block09";
}

inline bool PathIsEngineCube(const std::string& uePath, const std::string& meshId) {
	return uePath.find("BasicShapes/Cube") != std::string::npos
		|| meshId.find("Cube") != std::string::npos;
}

inline bool ManifestUsesEngineCubes(const DistrictManifestPure& m) {
	for (const auto& e : m.placements) {
		if (PathIsEngineCube(e.ue_path, e.mesh_id)) return true;
	}
	return false;
}

inline double DistSquared(const PlacementVec3& a, const PlacementVec3& b) {
	const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

/** Count placements within radius (cm) of center — freeroam stream bubble metric. */
inline int CountPlacementsNear(const DistrictManifestPure& m, PlacementVec3 center, double radiusCm) {
	const double r2 = radiusCm * radiusCm;
	int n = 0;
	for (const auto& e : m.placements) {
		if (DistSquared(e.location, center) <= r2) ++n;
	}
	return n;
}

inline bool PathIsImportedDistrict(const std::string& uePath) {
	return uePath.find("/Game/Imported/Districts/") != std::string::npos;
}

// --- minimal JSON helpers for placement manifests (numbers, strings, nested arrays) ---

namespace detail {
inline void SkipWs(const std::string& s, size_t& i) {
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
}
inline bool ParseString(const std::string& s, size_t& i, std::string& out) {
	SkipWs(s, i);
	if (i >= s.size() || s[i] != '"') return false;
	++i;
	out.clear();
	while (i < s.size() && s[i] != '"') {
		if (s[i] == '\\' && i + 1 < s.size()) { out.push_back(s[i + 1]); i += 2; continue; }
		out.push_back(s[i++]);
	}
	if (i >= s.size()) return false;
	++i;
	return true;
}
inline bool ParseNumber(const std::string& s, size_t& i, double& out) {
	SkipWs(s, i);
	size_t start = i;
	if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
	while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
		// allow scientific; stop on second sign handled loosely
		if ((s[i] == '+' || s[i] == '-') && i > start && s[i - 1] != 'e' && s[i - 1] != 'E') break;
		++i;
	}
	if (i == start) return false;
	try { out = std::stod(s.substr(start, i - start)); } catch (...) { return false; }
	return true;
}
inline bool FindKey(const std::string& s, const char* key, size_t from, size_t& valuePos) {
	const std::string pat = std::string("\"") + key + "\"";
	size_t p = s.find(pat, from);
	if (p == std::string::npos) return false;
	p = s.find(':', p + pat.size());
	if (p == std::string::npos) return false;
	valuePos = p + 1;
	return true;
}
inline bool ParseVec3Array(const std::string& s, size_t i, PlacementVec3& v) {
	SkipWs(s, i);
	if (i >= s.size() || s[i] != '[') return false;
	++i;
	if (!ParseNumber(s, i, v.x)) return false;
	SkipWs(s, i); if (i < s.size() && s[i] == ',') ++i;
	if (!ParseNumber(s, i, v.y)) return false;
	SkipWs(s, i); if (i < s.size() && s[i] == ',') ++i;
	if (!ParseNumber(s, i, v.z)) return false;
	return true;
}
} // namespace detail

/** Parse district placement JSON (bound or full). Returns false if no placements. */
inline bool ParsePlacementManifestJson(const std::string& text, DistrictManifestPure& out) {
	using namespace detail;
	out = DistrictManifestPure();
	size_t vp = 0;
	if (FindKey(text, "district_id", 0, vp)) ParseString(text, vp, out.district_id);
	if (FindKey(text, "source_package", 0, vp)) ParseString(text, vp, out.source_package);
	if (FindKey(text, "player_start", 0, vp)) ParseVec3Array(text, vp, out.player_start);
	if (FindKey(text, "vehicle_start", 0, vp)) ParseVec3Array(text, vp, out.vehicle_start);
	if (FindKey(text, "bound_count", 0, vp)) {
		double n = 0; if (ParseNumber(text, vp, n)) out.bound_count = static_cast<int>(n);
	}
	if (FindKey(text, "manifest_total", 0, vp)) {
		double n = 0; if (ParseNumber(text, vp, n)) out.manifest_total = static_cast<int>(n);
	}
	if (FindKey(text, "hit_rate", 0, vp)) {
		double n = 0; if (ParseNumber(text, vp, n)) out.hit_rate = n;
	}
	// stream_chunks length
	{
		const std::string key = "\"stream_chunks\"";
		size_t p = text.find(key);
		if (p != std::string::npos) {
			size_t lb = text.find('[', p);
			size_t rb = text.find(']', lb);
			// count objects roughly by "id"
			if (lb != std::string::npos && rb != std::string::npos) {
				std::string slice = text.substr(lb, rb - lb);
				size_t pos = 0; int c = 0;
				while ((pos = slice.find("\"id\"", pos)) != std::string::npos) { ++c; pos += 4; }
				out.stream_chunk_count = c;
			}
		}
	}
	// placements: walk each {"mesh_id"
	size_t pos = 0;
	while ((pos = text.find("\"mesh_id\"", pos)) != std::string::npos) {
		PlacementEntryPure e;
		size_t i = pos + 9;
		// find :
		i = text.find(':', i);
		if (i == std::string::npos) break;
		++i;
		if (!ParseString(text, i, e.mesh_id)) { pos += 8; continue; }
		// search forward in this object for ue_path, location, package (until next mesh_id or reasonable window)
		size_t windowEnd = text.find("\"mesh_id\"", i);
		if (windowEnd == std::string::npos) windowEnd = std::min(text.size(), i + 2000);
		std::string win = text.substr(pos, windowEnd - pos);
		size_t w = 0;
		if (FindKey(win, "ue_path", 0, w)) ParseString(win, w, e.ue_path);
		if (FindKey(win, "package", 0, w)) ParseString(win, w, e.package);
		if (FindKey(win, "location", 0, w)) ParseVec3Array(win, w, e.location);
		if (FindKey(win, "scale", 0, w)) ParseVec3Array(win, w, e.scale);
		out.placements.push_back(std::move(e));
		pos = windowEnd == text.size() ? i : windowEnd;
	}
	if (out.bound_count <= 0) out.bound_count = static_cast<int>(out.placements.size());
	if (out.manifest_total <= 0) out.manifest_total = static_cast<int>(out.placements.size());
	return !out.placements.empty();
}

/** Prefer *_bound.json path then full — returns which path string to open (caller reads file). */
inline std::string PreferredManifestFileName(const std::string& districtId, bool bound) {
	const std::string base = PlacementBaseNameForDistrict(districtId);
	return bound ? (base + "_bound.json") : (base + ".json");
}

} // namespace apb
