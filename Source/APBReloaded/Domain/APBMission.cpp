#include "APBMission.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <algorithm>
namespace apb {
namespace {
std::string ReadFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary); if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
std::string JStr(const std::string& obj, const std::string& key, const std::string& def = "") {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def;
	size_t q = p + 1; while (q < obj.size() && isspace((unsigned char)obj[q])) ++q;
	if (q < obj.size() && obj[q] == '"') {
		++q; std::string out;
		while (q < obj.size() && obj[q] != '"') {
			if (obj[q] == '\\' && q + 1 < obj.size()) { out.push_back(obj[q+1]); q += 2; continue; }
			out.push_back(obj[q++]);
		}
		return out.empty() ? def : out;
	}
	return def;
}
double JNum(const std::string& obj, const std::string& key, double def = 0) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	char* end = nullptr; const char* start = obj.c_str() + p;
	double v = strtod(start, &end); if (end == start) return def; return v;
}
bool JBool(const std::string& obj, const std::string& key, bool def = false) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	if (obj.compare(p, 4, "true") == 0) return true;
	if (obj.compare(p, 5, "false") == 0) return false;
	return JNum(obj, key, def ? 1 : 0) != 0;
}
std::vector<std::string> SplitObjects(const std::string& text) {
	std::vector<std::string> out; int depth = 0; size_t start = std::string::npos;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '{') { if (depth == 0) start = i; ++depth; }
		else if (text[i] == '}') { --depth; if (depth == 0 && start != std::string::npos) { out.push_back(text.substr(start, i - start + 1)); start = std::string::npos; } }
	}
	return out;
}
std::string ExtractArray(const std::string& obj, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return {};
	p = obj.find('[', p); if (p == std::string::npos) return {};
	int depth = 0; size_t start = p;
	for (size_t i = p; i < obj.size(); ++i) {
		if (obj[i] == '[') ++depth; else if (obj[i] == ']') { --depth; if (depth == 0) return obj.substr(start, i - start + 1); }
	}
	return {};
}
}
bool MissionScriptLibrary::LoadFromJsonFile(const std::string& path) { return LoadFromJsonText(ReadFile(path)); }
bool MissionScriptLibrary::LoadFromJsonText(const std::string& text) {
	if (text.empty()) return false; /* merge: do not clear existing scripts */
	for (const auto& obj : SplitObjects(text)) {
		MissionScriptDef d; d.id = JStr(obj, "id"); if (d.id.empty()) continue;
		d.title = JStr(obj, "title", d.id);
		d.contact_id = JStr(obj, "contact_id", JStr(obj, "contact", ""));
		d.faction_mask = (int32_t)JNum(obj, "faction", JNum(obj, "faction_mask", 0));
		d.group_min = (int32_t)JNum(obj, "group_min", 1); d.group_max = (int32_t)JNum(obj, "group_max", 4);
		d.takeout_count = (int32_t)JNum(obj, "takeout_count", 0);
		d.opposition_on_takeouts = JBool(obj, "opposition_on_takeouts", false);
		d.opposition_auto = JBool(obj, "opposition_auto", true);
		d.owning_side_bias = JNum(obj, "owning_side_bias", 1.0);
		d.last_stage_label = JStr(obj, "last_stage", "");
		d.source = JStr(obj, "source", "https://apbdb.com/missions");
		std::string stagesArr = ExtractArray(obj, "stages"); int idx = 0;
		for (const auto& sobj : SplitObjects(stagesArr)) {
			MissionStageDef s; s.index = (int32_t)JNum(sobj, "index", idx);
			s.type = JStr(sobj, "type", "objective"); s.name = JStr(sobj, "name", "Stage " + std::to_string(s.index + 1));
			s.target_progress = JNum(sobj, "target_progress", JNum(sobj, "target", 1.0));
			s.win_condition = JStr(sobj, "win_condition", "progress");
			s.fail_condition = JStr(sobj, "fail_condition", "");
			s.time_limit_sec = JNum(sobj, "time_limit_sec", 0);
			d.stages.push_back(s); ++idx;
		}
		if (d.stages.size() < 3) {
			const char* types[] = {"contact","travel","objective","defend","extract"};
			d.stages.clear();
			for (int i = 0; i < 5; ++i) {
				MissionStageDef s; s.index = i; s.type = types[i]; s.name = std::string("Stage ") + std::to_string(i + 1);
				s.target_progress = (s.type == "defend") ? 5.0 : 1.0; s.win_condition = "progress"; d.stages.push_back(s);
			}
		}
		scripts[d.id] = d;
	}
	return !scripts.empty();
}
const MissionScriptDef* MissionScriptLibrary::Find(const std::string& id) const {
	auto it = scripts.find(id); return it == scripts.end() ? nullptr : &it->second;
}
std::vector<std::string> MissionScriptLibrary::ListIds() const {
	std::vector<std::string> ids; for (const auto& kv : scripts) ids.push_back(kv.first); return ids;
}
bool MissionTitleCatalog::LoadFromJsonFile(const std::string& path) { return LoadFromJsonText(ReadFile(path)); }
bool MissionTitleCatalog::LoadFromJsonText(const std::string& text) {
	if (text.empty()) return false; /* merge: do not clear existing titles */
	int added = 0;
	for (const auto& obj : SplitObjects(text)) {
		std::string id = JStr(obj, "id"); if (id.empty()) continue;
		titles[id] = JStr(obj, "title", id); ++added;
	}
	return added > 0;
}
const std::string* MissionTitleCatalog::Find(const std::string& id) const {
	auto it = titles.find(id); return it == titles.end() ? nullptr : &it->second;
}
std::string MissionTitleCatalog::TitleFor(const std::string& id, const std::string& def) const {
	auto it = titles.find(id); return it == titles.end() ? def : it->second;
}
int32_t MissionTitleCatalog::ApplyTo(MissionScriptLibrary& lib) const {
	int32_t applied = 0;
	for (auto& kv : lib.scripts) {
		auto it = titles.find(kv.first);
		if (it != titles.end() && kv.second.title != it->second) { kv.second.title = it->second; ++applied; }
	}
	return applied;
}
bool MissionBriefCatalog::LoadFromJsonFile(const std::string& path) { return LoadFromJsonText(ReadFile(path)); }
bool MissionBriefCatalog::LoadFromJsonText(const std::string& text) {
	if (text.empty()) return false; /* merge: do not clear existing briefs */
	int added = 0;
	for (const auto& obj : SplitObjects(text)) {
		std::string id = JStr(obj, "id"); if (id.empty()) continue;
		MissionBrief b;
		b.id = id;
		b.template_id = JStr(obj, "template_id");
		b.stage = (int32_t)JNum(obj, "stage", 0);
		b.owner_brief = JStr(obj, "owner_brief");
		b.dispatch_brief = JStr(obj, "dispatch_brief");
		briefs[id] = b; ++added;
	}
	return added > 0;
}
const MissionBrief* MissionBriefCatalog::Find(const std::string& id) const {
	auto it = briefs.find(id); return it == briefs.end() ? nullptr : &it->second;
}
std::vector<const MissionBrief*> MissionBriefCatalog::ForTemplate(const std::string& template_id) const {
	std::vector<const MissionBrief*> out;
	for (const auto& kv : briefs) if (kv.second.template_id == template_id) out.push_back(&kv.second);
	std::sort(out.begin(), out.end(), [](const MissionBrief* a, const MissionBrief* b) { return a->stage < b->stage; });
	return out;
}
bool MissionOperationCatalog::LoadFromJsonFile(const std::string& path) { return LoadFromJsonText(ReadFile(path)); }
bool MissionOperationCatalog::LoadFromJsonText(const std::string& text) {
	if (text.empty()) return false; /* merge: do not clear existing ops */
	int added = 0;
	for (const auto& obj : SplitObjects(text)) {
		std::string id = JStr(obj, "id"); if (id.empty()) continue;
		std::string label = JStr(obj, "ui_description"); if (label.empty()) continue;
		ops[id] = label; ++added;
	}
	return added > 0;
}
const std::string* MissionOperationCatalog::Find(const std::string& id) const {
	auto it = ops.find(id); return it == ops.end() ? nullptr : &it->second;
}
std::string MissionOperationCatalog::LabelFor(const std::string& id, const std::string& def) const {
	auto it = ops.find(id); return it == ops.end() ? def : it->second;
}
bool MissionResultReasonCatalog::LoadFromJsonFile(const std::string& path) { return LoadFromJsonText(ReadFile(path)); }
bool MissionResultReasonCatalog::LoadFromJsonText(const std::string& text) {
	if (text.empty()) return false; /* merge: do not clear existing reasons */
	int added = 0;
	for (const auto& obj : SplitObjects(text)) {
		std::string id = JStr(obj, "id"); if (id.empty()) continue;
		MissionResultReason r;
		r.id = id;
		r.win_message = JStr(obj, "win_message");
		r.lose_message = JStr(obj, "lose_message");
		r.draw_message = JStr(obj, "draw_message");
		reasons[id] = r; ++added;
	}
	return added > 0;
}
const MissionResultReason* MissionResultReasonCatalog::Find(const std::string& id) const {
	auto it = reasons.find(id); return it == reasons.end() ? nullptr : &it->second;
}
std::string MissionResultReasonCatalog::WinMessage(const std::string& id, const std::string& def) const {
	auto it = reasons.find(id); return it == reasons.end() ? def : it->second.win_message;
}
std::string MissionResultReasonCatalog::LoseMessage(const std::string& id, const std::string& def) const {
	auto it = reasons.find(id); return it == reasons.end() ? def : it->second.lose_message;
}
std::string MissionResultReasonCatalog::DrawMessage(const std::string& id, const std::string& def) const {
	auto it = reasons.find(id); return it == reasons.end() ? def : it->second.draw_message;
}
MissionRun MissionRun::FromScript(const MissionScriptDef& script, Faction owner) {
	MissionRun m; m.id = script.id; m.title = script.title; m.owner = owner; m.contact_id = script.contact_id;
	m.source_script_id = script.id; m.opposition_on_takeouts = script.opposition_on_takeouts;
	m.takeout_fail_at = script.opposition_on_takeouts ? script.takeout_count : 0;
	m.opposition_contesting = script.opposition_auto;
	for (const auto& sd : script.stages) { MissionStageRuntime rt; rt.def = sd; m.stages.push_back(rt); }
	return m;
}
MissionRun MissionRun::MakeDefault(const std::string& mid, const std::string& title, Faction owner) {
	MissionScriptDef d; d.id = mid; d.title = title; d.opposition_auto = true;
	const char* types[] = {"contact","travel","objective","defend","extract"};
	for (int i = 0; i < 5; ++i) {
		MissionStageDef s; s.index = i; s.type = types[i]; s.name = std::string("Stage ") + std::to_string(i + 1);
		s.target_progress = (s.type == "defend") ? 5.0 : 1.0; s.win_condition = "progress"; d.stages.push_back(s);
	}
	return FromScript(d, owner);
}
void MissionRun::Start() { status = MissionStatus::Active; current_index = 0; if (!stages.empty()) opposition_contesting = true; }
MissionStageRuntime* MissionRun::Current() {
	if (status != MissionStatus::Active) return nullptr;
	if (current_index < 0 || current_index >= (int32_t)stages.size()) return nullptr;
	return &stages[current_index];
}
const MissionStageRuntime* MissionRun::Current() const {
	if (status != MissionStatus::Active) return nullptr;
	if (current_index < 0 || current_index >= (int32_t)stages.size()) return nullptr;
	return &stages[current_index];
}
bool MissionRun::Progress(double amount) {
	MissionStageRuntime* s = Current(); if (!s) return false;
	s->progress = (s->progress + amount < s->def.target_progress) ? s->progress + amount : s->def.target_progress;
	if (s->progress + 1e-9 >= s->def.target_progress) {
		s->done = true;
		if (current_index >= (int32_t)stages.size() - 1) status = MissionStatus::Completed; else ++current_index;
		return true;
	}
	return false;
}
void MissionRun::RegisterOppositionTakeout() {
	++takeouts; opposition_contesting = true;
	if (takeout_fail_at > 0 && takeouts >= takeout_fail_at) status = MissionStatus::Failed;
}
bool MissionRun::AdvanceOpposition(double amount) {
	if (status != MissionStatus::Active) return false;
	if (!opposition_contesting || amount <= 0) return false;
	MissionStageRuntime* s = Current(); if (!s) return false;
	s->opp_progress = (s->opp_progress + amount < s->def.target_progress) ? s->opp_progress + amount : s->def.target_progress;
	if (s->opp_progress + 1e-9 >= s->def.target_progress) {
		opposition_won = true;
		status = MissionStatus::Failed;
		return true;
	}
	return false;
}
bool MissionRun::CheckTimeout(double now_sec) {
	if (status != MissionStatus::Active) return false;
	MissionStageRuntime* s = Current(); if (!s) return false;
	if (s->def.time_limit_sec <= 0) { timed_stage_index = current_index; current_stage_deadline_sec = 0; return false; }
	if (timed_stage_index != current_index) {
		// Arm the countdown for the stage that just became current.
		timed_stage_index = current_index;
		current_stage_deadline_sec = now_sec + s->def.time_limit_sec;
		return false;
	}
	if (now_sec > current_stage_deadline_sec) {
		timed_out = true;
		status = MissionStatus::Failed;
		return true;
	}
	return false;
}
void MissionRun::Fail(const std::string&) { status = MissionStatus::Failed; }
}
