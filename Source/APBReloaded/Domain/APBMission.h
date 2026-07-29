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
struct MissionStageRuntime { MissionStageDef def; double progress = 0; double opp_progress = 0; bool done = false; };
class MissionScriptLibrary {
public:
	std::unordered_map<std::string, MissionScriptDef> scripts;
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const MissionScriptDef* Find(const std::string& id) const;
	std::vector<std::string> ListIds() const;
};
// Canonical retail mission-title roster parsed from Content/Data/mission_templates.json
// (extracted from the retail MissionTemplates.INT, the shipped mirror of the SDD table
// MissionTemplate). Read-only display-title lookup keyed by template id
// (e.g. "DB_BCS4_Del1" -> "PIMP MY CRIB"). Additive/merge-by-id like the other catalogs.
class MissionTitleCatalog {
public:
	std::unordered_map<std::string, std::string> titles; // template id -> canonical display title
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const std::string* Find(const std::string& id) const;
	// Canonical title for a template id, or def when the id has no entry.
	std::string TitleFor(const std::string& id, const std::string& def = std::string()) const;
	// Stamp the canonical retail title onto every loaded script whose id has a catalog entry
	// (mission-script ids share the MissionTemplate id space 1:1). Returns the number of scripts
	// whose title was changed. Deterministic; server-authoritative.
	int32_t ApplyTo(MissionScriptLibrary& lib) const;
	int32_t Count() const { return (int32_t)titles.size(); }
};
// One mission stage's owner/dispatch briefings, parsed from Content/Data/task_objectives.json
// (extracted from the retail TaskObjectives.INT, mirror of the SDD table TaskObjective). The
// OwnerBrief is shown to the mission owner (attacker); the DispatchBrief to the dispatched
// opposition (defender). Retail markup like "<Col: StageText>...</Col>" is preserved verbatim.
struct MissionBrief {
	std::string id;           // "<template_id>_Stage<NN>"
	std::string template_id;  // shares the MissionTemplate id space 1:1
	int32_t stage = 0;
	std::string owner_brief;
	std::string dispatch_brief;
};
// Per-stage mission-brief roster keyed by "<template_id>_Stage<NN>". Additive/merge-by-id.
class MissionBriefCatalog {
public:
	std::unordered_map<std::string, MissionBrief> briefs;
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const MissionBrief* Find(const std::string& id) const;
	// All briefs for a template id, ordered ascending by stage.
	std::vector<const MissionBrief*> ForTemplate(const std::string& template_id) const;
	int32_t Count() const { return (int32_t)briefs.size(); }
};
// Per-operation objective-type HUD labels, parsed from Content/Data/task_operations.json
// (extracted from the retail TaskOperations.INT, mirror of the SDD table TaskOperation). Keyed
// by operation id (e.g. "AntiGraffiti10NoHoldPoints" -> "Graffiti Target",
// "CheckpointAllAtOnce05" -> "Checkpoint", "Escape120" -> "Escape!"). This is the short label a
// mission stage of that operation type shows on the HUD. Additive/merge-by-id like the other
// catalogs; placeholder ops with no UIDescription are absent by design.
class MissionOperationCatalog {
public:
	std::unordered_map<std::string, std::string> ops; // operation id -> UI objective label
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const std::string* Find(const std::string& id) const;
	// UI objective label for an operation id, or def when the id has no entry.
	std::string LabelFor(const std::string& id, const std::string& def = std::string()) const;
	int32_t Count() const { return (int32_t)ops.size(); }
};
// One mission-end result reason's perspective-specific messages, parsed from
// Content/Data/mission_result_reasons.json (extracted from the retail MissionResultReasons.INT,
// mirror of the SDD table MissionResultReason). The WinMessage is shown to the side that won,
// the LoseMessage to the side that lost, the DrawMessage on a draw. Any perspective may be empty.
struct MissionResultReason {
	std::string id;            // e.g. "TimedOut", "WonFinalObjective", "CompletedUnopposed"
	std::string win_message;
	std::string lose_message;
	std::string draw_message;
};
// The authentic retail mission end-screen messages keyed by result-reason id. Additive/merge-by-id.
class MissionResultReasonCatalog {
public:
	std::unordered_map<std::string, MissionResultReason> reasons;
	bool LoadFromJsonFile(const std::string& path);
	bool LoadFromJsonText(const std::string& text);
	const MissionResultReason* Find(const std::string& id) const;
	// Perspective-specific message for a reason id, or def when the id/perspective has no entry.
	std::string WinMessage(const std::string& id, const std::string& def = std::string()) const;
	std::string LoseMessage(const std::string& id, const std::string& def = std::string()) const;
	std::string DrawMessage(const std::string& id, const std::string& def = std::string()) const;
	int32_t Count() const { return (int32_t)reasons.size(); }
};
class MissionRun {
public:
	std::string id, title, contact_id, source_script_id;
	Faction owner = Faction::Criminal;
	std::vector<MissionStageRuntime> stages;
	MissionStatus status = MissionStatus::Inactive;
	int32_t current_index = 0, takeouts = 0, takeout_fail_at = 0;
	bool opposition_contesting = false, opposition_on_takeouts = false, opposition_won = false;
	// Stage countdown (APB mission timers): armed lazily on the first CheckTimeout tick after a
	// stage becomes current; running past the deadline fails the mission (timed_out=true).
	double current_stage_deadline_sec = 0;
	int32_t timed_stage_index = -1;
	bool timed_out = false;
	static MissionRun FromScript(const MissionScriptDef& script, Faction owner);
	static MissionRun MakeDefault(const std::string& mid, const std::string& title, Faction owner);
	void Start();
	bool Progress(double amount = 1.0);
	void RegisterOppositionTakeout();
	bool AdvanceOpposition(double amount = 1.0);
	/** Deterministic stage countdown: arms the current stage's time_limit_sec on first call,
	 *  then fails the mission (timed_out) if now_sec passes the deadline. Returns true only on
	 *  the tick that times the mission out. Caller supplies the clock. */
	bool CheckTimeout(double now_sec);
	void Fail(const std::string& reason = "fail");
	MissionStageRuntime* Current();
	const MissionStageRuntime* Current() const;
	int32_t StageCount() const { return (int32_t)stages.size(); }
	bool IsTerminal() const { return status == MissionStatus::Completed || status == MissionStatus::Failed; }
};
}
