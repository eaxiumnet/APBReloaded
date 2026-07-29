#pragma once
// Pure placement resolve / metrics (no UE). Shared by freeroam loader concepts and domain tests.
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <utility>

namespace apb {

struct PlacementVec3 {
	double x = 0, y = 0, z = 0;
};

struct PlacementEntryPure {
	std::string source_id;
	std::string mesh_id;
	std::string ue_path;
	std::string package;
	std::string actor;
	std::string edge;
	std::string reason;
	std::string actor_class;
	std::string mesh_source;
	PlacementVec3 location;
	PlacementVec3 rotation;
	PlacementVec3 scale{1, 1, 1};
	bool source_id_present = false;
	bool ue_path_present = false;
	bool package_present = false;
	bool actor_present = false;
	bool edge_present = false;
	bool reason_present = false;
	bool actor_class_present = false;
	bool mesh_source_present = false;
	bool rotation_present = false;
	bool scale_present = false;
	bool bound = false;
	bool bound_present = false;
};

struct PlacementRejectedRow {
	size_t placement_index = 0;
	std::string reason;
};

struct DistrictManifestPure {
	std::string district_id;
	std::string source_package;
	std::string provenance;
	std::vector<std::string> source_packages;
	std::string layout;
	std::string layout_note;
	int actor_count = 0;
	int unresolved_actor_count = 0;
	PlacementVec3 player_start{-200, -1200, 120};
	PlacementVec3 vehicle_start{400, -1200, 50};
	std::vector<PlacementEntryPure> placements;
	int bound_count = 0;
	int manifest_total = 0;
	double hit_rate = 0;
	bool loaded_bound = false;
	int stream_chunk_count = 0;
	int renderable_count = 0;
	int total_row_count = 0;
	std::vector<std::pair<std::string, int>> reason_histogram;
	bool provenance_present = false;
	bool renderable_count_present = false;
	bool total_row_count_present = false;
	bool reason_histogram_present = false;
	bool source_packages_present = false;
	bool layout_present = false;
	bool layout_note_present = false;
	bool actor_count_present = false;
	bool unresolved_actor_count_present = false;
	bool player_start_present = false;
	bool vehicle_start_present = false;
	bool bound_count_present = false;
	bool manifest_total_present = false;
	bool hit_rate_present = false;
	std::vector<PlacementRejectedRow> rejected_rows;
	bool json_parsed = false;
	bool valid_manifest = false;
	std::string parse_error;
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

namespace detail {
struct JsonValue {
	enum class Kind { Null, Bool, Number, String, Array, Object };

	Kind kind = Kind::Null;
	bool boolean = false;
	double number = 0.0;
	std::string string;
	std::vector<JsonValue> array;
	std::vector<std::pair<std::string, JsonValue>> object;
};

inline bool IsDigit(char c) {
	return c >= '0' && c <= '9';
}

inline int HexDigit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

inline void AppendUtf8(std::string& out, unsigned int codepoint) {
	if (codepoint <= 0x7f) {
		out.push_back(static_cast<char>(codepoint));
	} else if (codepoint <= 0x7ff) {
		out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	} else if (codepoint <= 0xffff) {
		out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	} else {
		out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
}

class JsonParser {
public:
	explicit JsonParser(const std::string& text) : text_(text) {}

	bool Parse(JsonValue& out, std::string& error) {
		SkipWs();
		if (!ParseValue(out, 0, error)) return false;
		SkipWs();
		if (position_ != text_.size()) return Fail(error, "trailing_data");
		return true;
	}

private:
	static constexpr size_t MaxDepth = 64;
	static constexpr size_t MaxNodes = 250000;

	const std::string& text_;
	size_t position_ = 0;
	size_t nodes_ = 0;

	void SkipWs() {
		while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t'
			|| text_[position_] == '\r' || text_[position_] == '\n')) ++position_;
	}

	bool Fail(std::string& error, const char* reason) const {
		error = std::string("invalid_json:") + reason + ":at=" + std::to_string(position_);
		return false;
	}

	bool ParseValue(JsonValue& out, size_t depth, std::string& error) {
		SkipWs();
		if (depth > MaxDepth) return Fail(error, "max_depth");
		if (++nodes_ > MaxNodes) return Fail(error, "max_nodes");
		if (position_ >= text_.size()) return Fail(error, "unexpected_end");
		switch (text_[position_]) {
		case 'n': return ParseLiteral("null", JsonValue::Kind::Null, out, error);
		case 't': return ParseLiteral("true", JsonValue::Kind::Bool, out, error, true);
		case 'f': return ParseLiteral("false", JsonValue::Kind::Bool, out, error, false);
		case '"':
			out.kind = JsonValue::Kind::String;
			return ParseString(out.string, error);
		case '[':
			return ParseArray(out, depth, error);
		case '{':
			return ParseObject(out, depth, error);
		default:
			if (text_[position_] == '-' || IsDigit(text_[position_])) {
				out.kind = JsonValue::Kind::Number;
				return ParseNumber(out.number, error);
			}
			return Fail(error, "unexpected_token");
		}
	}

	bool ParseLiteral(const char* literal, JsonValue::Kind kind, JsonValue& out,
		std::string& error, bool boolean = false) {
		const size_t length = std::char_traits<char>::length(literal);
		if (text_.compare(position_, length, literal) != 0) return Fail(error, "invalid_literal");
		position_ += length;
		out.kind = kind;
		out.boolean = boolean;
		return true;
	}

	bool ParseString(std::string& out, std::string& error) {
		if (position_ >= text_.size() || text_[position_] != '"') return Fail(error, "expected_string");
		++position_;
		out.clear();
		while (position_ < text_.size()) {
			const unsigned char c = static_cast<unsigned char>(text_[position_++]);
			if (c == '"') return true;
			if (c < 0x20) return Fail(error, "control_in_string");
			if (c != '\\') {
				out.push_back(static_cast<char>(c));
				continue;
			}
			if (position_ >= text_.size()) return Fail(error, "unterminated_escape");
			const char escaped = text_[position_++];
			switch (escaped) {
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			case 'u':
				if (!ParseUnicodeEscape(out, error)) return false;
				break;
			default: return Fail(error, "invalid_escape");
			}
		}
		return Fail(error, "unterminated_string");
	}

	bool ParseUnicodeEscape(std::string& out, std::string& error) {
		unsigned int codepoint = 0;
		for (int n = 0; n < 4; ++n) {
			if (position_ >= text_.size()) return Fail(error, "short_unicode_escape");
			const int digit = HexDigit(text_[position_++]);
			if (digit < 0) return Fail(error, "invalid_unicode_escape");
			codepoint = (codepoint << 4) | static_cast<unsigned int>(digit);
		}
		if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
			if (position_ + 5 >= text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u')
				return Fail(error, "unpaired_high_surrogate");
			position_ += 2;
			unsigned int low = 0;
			for (int n = 0; n < 4; ++n) {
				const int digit = HexDigit(text_[position_++]);
				if (digit < 0) return Fail(error, "invalid_low_surrogate");
				low = (low << 4) | static_cast<unsigned int>(digit);
			}
			if (low < 0xdc00 || low > 0xdfff) return Fail(error, "invalid_low_surrogate");
			codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
		} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
			return Fail(error, "unpaired_low_surrogate");
		}
		AppendUtf8(out, codepoint);
		return true;
	}

	bool ParseNumber(double& out, std::string& error) {
		const size_t start = position_;
		if (text_[position_] == '-') ++position_;
		if (position_ >= text_.size()) return Fail(error, "invalid_number");
		if (text_[position_] == '0') {
			++position_;
			if (position_ < text_.size() && IsDigit(text_[position_])) return Fail(error, "leading_zero");
		} else {
			if (position_ >= text_.size() || text_[position_] < '1' || text_[position_] > '9')
				return Fail(error, "invalid_number");
			while (position_ < text_.size() && IsDigit(text_[position_])) ++position_;
		}
		if (position_ < text_.size() && text_[position_] == '.') {
			++position_;
			if (position_ >= text_.size() || !IsDigit(text_[position_])) return Fail(error, "invalid_fraction");
			while (position_ < text_.size() && IsDigit(text_[position_])) ++position_;
		}
		if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
			++position_;
			if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
			if (position_ >= text_.size() || !IsDigit(text_[position_])) return Fail(error, "invalid_exponent");
			while (position_ < text_.size() && IsDigit(text_[position_])) ++position_;
		}
		const std::string token = text_.substr(start, position_ - start);
		char* end = nullptr;
		errno = 0;
		out = std::strtod(token.c_str(), &end);
		if (errno == ERANGE || end != token.c_str() + token.size() || !std::isfinite(out))
			return Fail(error, "number_out_of_range");
		return true;
	}

	bool ParseArray(JsonValue& out, size_t depth, std::string& error) {
		out.kind = JsonValue::Kind::Array;
		++position_;
		SkipWs();
		if (position_ < text_.size() && text_[position_] == ']') { ++position_; return true; }
		while (true) {
			JsonValue item;
			if (!ParseValue(item, depth + 1, error)) return false;
			out.array.push_back(std::move(item));
			SkipWs();
			if (position_ >= text_.size()) return Fail(error, "unterminated_array");
			if (text_[position_] == ']') { ++position_; return true; }
			if (text_[position_] != ',') return Fail(error, "expected_array_separator");
			++position_;
			SkipWs();
			if (position_ < text_.size() && text_[position_] == ']') return Fail(error, "trailing_array_comma");
		}
	}

	bool ParseObject(JsonValue& out, size_t depth, std::string& error) {
		out.kind = JsonValue::Kind::Object;
		++position_;
		SkipWs();
		if (position_ < text_.size() && text_[position_] == '}') { ++position_; return true; }
		while (true) {
			std::string key;
			if (!ParseString(key, error)) return false;
			for (const auto& existing : out.object) {
				if (existing.first == key) return Fail(error, "duplicate_key");
			}
			SkipWs();
			if (position_ >= text_.size() || text_[position_] != ':') return Fail(error, "expected_object_colon");
			++position_;
			JsonValue value;
			if (!ParseValue(value, depth + 1, error)) return false;
			out.object.emplace_back(std::move(key), std::move(value));
			SkipWs();
			if (position_ >= text_.size()) return Fail(error, "unterminated_object");
			if (text_[position_] == '}') { ++position_; return true; }
			if (text_[position_] != ',') return Fail(error, "expected_object_separator");
			++position_;
			SkipWs();
			if (position_ < text_.size() && text_[position_] == '}') return Fail(error, "trailing_object_comma");
		}
	}
};

inline const JsonValue* FindField(const JsonValue& object, const char* key) {
	if (object.kind != JsonValue::Kind::Object) return nullptr;
	for (const auto& field : object.object) {
		if (field.first == key) return &field.second;
	}
	return nullptr;
}

inline bool ReadString(const JsonValue* value, std::string& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::String) return false;
	out = value->string;
	return true;
}

inline bool ReadNumber(const JsonValue* value, double& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::Number || !std::isfinite(value->number)) return false;
	out = value->number;
	return true;
}

inline bool ReadBool(const JsonValue* value, bool& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::Bool) return false;
	out = value->boolean;
	return true;
}

inline bool ReadInteger(const JsonValue* value, int& out) {
	double number = 0.0;
	if (!ReadNumber(value, number) || number < static_cast<double>(std::numeric_limits<int>::min())
		|| number > static_cast<double>(std::numeric_limits<int>::max()) || std::floor(number) != number) return false;
	out = static_cast<int>(number);
	return true;
}

inline bool ReadVec3(const JsonValue* value, PlacementVec3& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::Array || value->array.size() != 3) return false;
	return ReadNumber(&value->array[0], out.x) && ReadNumber(&value->array[1], out.y)
		&& ReadNumber(&value->array[2], out.z);
}

inline bool ReadStringArray(const JsonValue* value, std::vector<std::string>& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::Array) return false;
	out.clear();
	for (const JsonValue& item : value->array) {
		if (item.kind != JsonValue::Kind::String) return false;
		out.push_back(item.string);
	}
	return true;
}

inline bool ReadReasonHistogram(const JsonValue* value, std::vector<std::pair<std::string, int>>& out) {
	if (value == nullptr || value->kind != JsonValue::Kind::Object) return false;
	out.clear();
	for (const auto& field : value->object) {
		int count = 0;
		if (field.first.empty() || !ReadInteger(&field.second, count) || count < 0) return false;
		out.emplace_back(field.first, count);
	}
	return true;
}

inline void SetSchemaError(DistrictManifestPure& out, const char* reason) {
	if (out.parse_error.empty()) out.parse_error = std::string("schema:") + reason;
}

} // namespace detail

/** Parse district placement JSON (bound or full). Returns true only for valid non-empty manifests. */
inline bool ParsePlacementManifestJson(const std::string& text, DistrictManifestPure& out) {
	using namespace detail;
	out = DistrictManifestPure();
	JsonValue root;
	JsonParser parser(text);
	if (!parser.Parse(root, out.parse_error)) return false;
	out.json_parsed = true;
	if (root.kind != JsonValue::Kind::Object) {
		SetSchemaError(out, "root_not_object");
		return false;
	}

	const JsonValue* districtId = FindField(root, "district_id");
	const JsonValue* sourcePackage = FindField(root, "source_package");
	if (!ReadString(districtId, out.district_id) || out.district_id.empty()) SetSchemaError(out, "invalid_district_id");
	if (!ReadString(sourcePackage, out.source_package) || out.source_package.empty()) SetSchemaError(out, "invalid_source_package");
	const JsonValue* provenance = FindField(root, "provenance");
	if (provenance != nullptr && (!ReadString(provenance, out.provenance)
		|| (out.provenance != "real" && out.provenance != "synthetic"))) SetSchemaError(out, "invalid_provenance");
	out.provenance_present = provenance != nullptr;
	const JsonValue* renderableCount = FindField(root, "renderable_count");
	if (renderableCount != nullptr && (!ReadInteger(renderableCount, out.renderable_count)
		|| out.renderable_count < 0)) SetSchemaError(out, "invalid_renderable_count");
	out.renderable_count_present = renderableCount != nullptr;
	const JsonValue* totalRowCount = FindField(root, "total_row_count");
	if (totalRowCount != nullptr && (!ReadInteger(totalRowCount, out.total_row_count)
		|| out.total_row_count < 0)) SetSchemaError(out, "invalid_total_row_count");
	out.total_row_count_present = totalRowCount != nullptr;
	const JsonValue* reasonHistogram = FindField(root, "reason_histogram");
	if (reasonHistogram != nullptr && !ReadReasonHistogram(reasonHistogram, out.reason_histogram))
		SetSchemaError(out, "invalid_reason_histogram");
	out.reason_histogram_present = reasonHistogram != nullptr;
	const JsonValue* sourcePackages = FindField(root, "source_packages");
	if (sourcePackages != nullptr && !ReadStringArray(sourcePackages, out.source_packages)) SetSchemaError(out, "invalid_source_packages");
	out.source_packages_present = sourcePackages != nullptr;
	const JsonValue* layout = FindField(root, "layout");
	if (layout != nullptr && !ReadString(layout, out.layout)) SetSchemaError(out, "invalid_layout");
	out.layout_present = layout != nullptr;
	const JsonValue* layoutNote = FindField(root, "layout_note");
	if (layoutNote != nullptr && !ReadString(layoutNote, out.layout_note)) SetSchemaError(out, "invalid_layout_note");
	out.layout_note_present = layoutNote != nullptr;
	const JsonValue* actorCount = FindField(root, "actor_count");
	if (actorCount != nullptr && (!ReadInteger(actorCount, out.actor_count) || out.actor_count < 0)) SetSchemaError(out, "invalid_actor_count");
	out.actor_count_present = actorCount != nullptr;
	const JsonValue* unresolvedActorCount = FindField(root, "unresolved_actor_count");
	if (unresolvedActorCount != nullptr && (!ReadInteger(unresolvedActorCount, out.unresolved_actor_count)
		|| out.unresolved_actor_count < 0)) SetSchemaError(out, "invalid_unresolved_actor_count");
	out.unresolved_actor_count_present = unresolvedActorCount != nullptr;

	const JsonValue* playerStart = FindField(root, "player_start");
	if (playerStart != nullptr && !ReadVec3(playerStart, out.player_start)) SetSchemaError(out, "invalid_player_start");
	out.player_start_present = playerStart != nullptr;
	const JsonValue* vehicleStart = FindField(root, "vehicle_start");
	if (vehicleStart != nullptr && !ReadVec3(vehicleStart, out.vehicle_start)) SetSchemaError(out, "invalid_vehicle_start");
	out.vehicle_start_present = vehicleStart != nullptr;

	const JsonValue* boundCount = FindField(root, "bound_count");
	if (boundCount != nullptr && (!ReadInteger(boundCount, out.bound_count) || out.bound_count < 0)) SetSchemaError(out, "invalid_bound_count");
	out.bound_count_present = boundCount != nullptr;
	const JsonValue* manifestTotal = FindField(root, "manifest_total");
	if (manifestTotal != nullptr && (!ReadInteger(manifestTotal, out.manifest_total) || out.manifest_total < 0)) SetSchemaError(out, "invalid_manifest_total");
	out.manifest_total_present = manifestTotal != nullptr;
	const JsonValue* hitRate = FindField(root, "hit_rate");
	if (hitRate != nullptr && !ReadNumber(hitRate, out.hit_rate)) SetSchemaError(out, "invalid_hit_rate");
	out.hit_rate_present = hitRate != nullptr;
	const JsonValue* streamChunks = FindField(root, "stream_chunks");
	if (streamChunks != nullptr) {
		if (streamChunks->kind != JsonValue::Kind::Array) SetSchemaError(out, "invalid_stream_chunks");
		else out.stream_chunk_count = static_cast<int>(streamChunks->array.size());
	}

	const JsonValue* placements = FindField(root, "placements");
	if (placements == nullptr || placements->kind != JsonValue::Kind::Array) {
		SetSchemaError(out, "missing_placements");
		return false;
	}
	for (size_t index = 0; index < placements->array.size(); ++index) {
		const JsonValue& placement = placements->array[index];
		PlacementRejectedRow rejection;
		rejection.placement_index = index;
		if (placement.kind != JsonValue::Kind::Object) {
			rejection.reason = "placement_not_object";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}

		PlacementEntryPure entry;
		const JsonValue* sourceId = FindField(placement, "source_id");
		if (!ReadString(sourceId, entry.source_id) || entry.source_id.empty()) {
			rejection.reason = sourceId == nullptr ? "MissingSourceId" : "InvalidSourceId";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.source_id_present = true;
		const JsonValue* location = FindField(placement, "location");
		if (!ReadVec3(location, entry.location)) {
			rejection.reason = location == nullptr ? "missing_location" : "invalid_location";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		const JsonValue* reason = FindField(placement, "reason");
		if (reason != nullptr) {
			if (!ReadString(reason, entry.reason) || entry.reason.empty()) {
				rejection.reason = "invalid_reason";
			} else {
				entry.reason_present = true;
				rejection.reason = entry.reason;
			}
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		const JsonValue* meshId = FindField(placement, "mesh_id");
		if (!ReadString(meshId, entry.mesh_id) || entry.mesh_id.empty()) {
			rejection.reason = meshId == nullptr ? "missing_mesh_id" : "invalid_mesh_id";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		const JsonValue* uePath = FindField(placement, "ue_path");
		if (uePath != nullptr && !ReadString(uePath, entry.ue_path)) {
			rejection.reason = "invalid_ue_path";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.ue_path_present = uePath != nullptr;
		const JsonValue* package = FindField(placement, "package");
		if (package != nullptr && !ReadString(package, entry.package)) {
			rejection.reason = "invalid_package";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.package_present = package != nullptr;
		const JsonValue* actor = FindField(placement, "actor");
		if (actor != nullptr && !ReadString(actor, entry.actor)) {
			rejection.reason = "invalid_actor";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.actor_present = actor != nullptr;
		const JsonValue* edge = FindField(placement, "edge");
		if (edge != nullptr && !ReadString(edge, entry.edge)) {
			rejection.reason = "invalid_edge";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.edge_present = edge != nullptr;
		const JsonValue* actorClass = FindField(placement, "actor_class");
		if (actorClass != nullptr && !ReadString(actorClass, entry.actor_class)) {
			rejection.reason = "invalid_actor_class";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.actor_class_present = actorClass != nullptr;
		const JsonValue* meshSource = FindField(placement, "mesh_source");
		if (meshSource != nullptr && !ReadString(meshSource, entry.mesh_source)) {
			rejection.reason = "invalid_mesh_source";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.mesh_source_present = meshSource != nullptr;
		const JsonValue* bound = FindField(placement, "bound");
		if (bound != nullptr && !ReadBool(bound, entry.bound)) {
			rejection.reason = "invalid_bound";
			out.rejected_rows.push_back(std::move(rejection));
			continue;
		}
		entry.bound_present = bound != nullptr;
		const JsonValue* rotation = FindField(placement, "rotation");
		if (rotation != nullptr) {
			if (!ReadVec3(rotation, entry.rotation)) {
				rejection.reason = "invalid_rotation";
				out.rejected_rows.push_back(std::move(rejection));
				continue;
			}
			entry.rotation_present = true;
		}
		const JsonValue* scale = FindField(placement, "scale");
		if (scale != nullptr) {
			if (!ReadVec3(scale, entry.scale)) {
				rejection.reason = "invalid_scale";
				out.rejected_rows.push_back(std::move(rejection));
				continue;
			}
			entry.scale_present = true;
		}
		out.placements.push_back(std::move(entry));
	}

	if (boundCount == nullptr) out.bound_count = static_cast<int>(out.placements.size());
	if (manifestTotal == nullptr) out.manifest_total = static_cast<int>(placements->array.size());
	if (renderableCount == nullptr) out.renderable_count = static_cast<int>(out.placements.size());
	if (totalRowCount == nullptr) out.total_row_count = static_cast<int>(placements->array.size());
	if (placements->array.empty()) SetSchemaError(out, "empty_placements");
	if (out.parse_error.empty()) {
		out.valid_manifest = !placements->array.empty();
	} else {
		out.valid_manifest = false;
	}
	return out.valid_manifest;
}

inline std::string PreferredManifestFileName(const std::string& districtId, bool bound) {
	const std::string base = PlacementBaseNameForDistrict(districtId);
	return bound ? (base + "_bound.json") : (base + ".json");
}

} // namespace apb
