#pragma once
#include "APBTypes.h"
#include <cstdlib>
namespace apb {
enum class MissionStatus { Inactive, Active, Completed, Failed };
struct MissionStageDef {
	int32_t index = 0;
	std::string type;
	std::string name;
	double target_progress = 1.0;
	std::string win_condition;
	std::string fail_condition;
	double time_limit_sec = 0;
};
struct MissionScriptDef {
	std::string id, title, contact_id, last_stage_label, source;
	int32_t faction_mask = 0, group_min = 1, group_max = 4, takeout_count = 0;
	bool opposition_on_takeouts = false, opposition_auto = true;
	double owning_side_bias = 1.0;
	std::vector<MissionStageDef> stages;
};
struct MissionStageRuntime { MissionStageDef def; double progress = 0; bool done = false; };
class MissionScriptLibrary {
public:
	std::unordered_map<std::string, MissionScriptDef> scripts;
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const MissionScriptDef* Find(const std::string& id) const;
	std::vector<std::string> ListIds() const;
};
class MissionRun {
public:
	std::string id, title, contact_id, source_script_id;
	Faction owner = Faction::Criminal;
	std::vector<MissionStageRuntime> stages;
	MissionStatus status = MissionStatus::Inactive;
	int32_t current_index = 0, takeouts = 0, takeout_fail_at = 0;
	bool opposition_contesting = false, opposition_on_takeouts = false;
	static MissionRun FromScript(const MissionScriptDef& script, Faction owner);
	static MissionRun MakeDefault(const std::string& mid, const std::string& title, Faction owner);
	void Start();
	bool Progress(double amount = 1.0);
	void RegisterOppositionTakeout();
	void Fail(const std::string& reason = "fail");
	MissionStageRuntime* Current();
	const MissionStageRuntime* Current() const;
	int32_t StageCount() const { return (int32_t)stages.size(); }
	bool IsTerminal() const { return status == MissionStatus::Completed || status == MissionStatus::Failed; }
};
}
