#pragma once
// APBChat.h — M7 N5: pure-C++17 chat domain (channels, routing, ignore list,
// profanity filter, flood control, presence, slash commands). No platform/UE
// headers — unit-testable in isolation like WorldService/TicketService.
//
// The service is the AUTHORITATIVE router: given a roster of players present on
// a district (each with prefs: ignore set, profanity toggle, presence, group/
// clan/faction membership), Submit() produces a per-recipient delivery list.
// Networking (UE district GM multicast) lives above this layer; proximity radius
// for the Local channel is a UE-side spatial concern (domain treats Local as
// roster-wide and keeps the channel tag distinct for the UE layer to narrow).
//
// Grounded 1:1 on retail APB strings (APBUserInterface.int [Chat]):
//   - ignore list blocks "communications ... including mail and chat"
//   - "kicked ... too many chat messages in a short period" -> flood control
//   - presence: Available / Away / Do Not Disturb
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace apb {

// Canonical APB chat channels. District/Whisper/Group are wired for M7; the
// remaining channels share the same routing primitives and are enabled as the
// social systems (clan/faction/trade) land in later milestones.
enum class ChatChannel {
	Local,     // proximity / nearby (UE narrows by radius; domain = roster-wide)
	District,  // whole district instance
	Group,     // mission group members
	Whisper,   // private direct message to one player
	Faction,   // enforcer- or criminal-wide
	Clan,      // clan members
	Trade,     // trade channel
	System     // server/system broadcast (no filter/flood/ignore applied)
};

enum class PresenceStatus { Available, Away, DoNotDisturb };

// Per-submission outcome for the sender.
enum class ChatResult {
	Delivered,
	Empty,             // blank body after trim
	Muted,             // sender exceeded the flood threshold
	RecipientOffline,  // whisper target not present on this district
	RecipientBusy,     // whisper target is Do-Not-Disturb
	BadChannel         // scope has no destination (e.g. group/clan with no id)
};

struct ChatMessage {
	ChatChannel channel = ChatChannel::Local;
	std::string sender;   // "" for System
	std::string target;   // whisper recipient / scope id; "" for broadcast scopes
	std::string body;     // final text (may be profanity-masked per recipient)
	int64_t     timestamp = 0;
};

// One resolved delivery: who receives it and the (per-recipient) message copy.
struct ChatDelivery {
	std::string  recipient;
	ChatMessage  message;
};

struct SubmitResult {
	ChatResult status = ChatResult::Delivered;
	std::vector<ChatDelivery> deliveries;
};

// Parsed slash command / plain line.
struct ParsedChat {
	ChatChannel channel = ChatChannel::District;
	std::string target;  // whisper recipient (from "/w <name> ...")
	std::string body;
	bool ok = false;     // false if command malformed (e.g. "/w" with no name)
};

// Per-player chat preferences + membership, tracked while present on a district.
struct PlayerChatState {
	PresenceStatus presence = PresenceStatus::Available;
	bool profanity_filter = true;               // default ON (retail default)
	std::unordered_set<std::string> ignored;    // lowercased names this player ignores
	std::string group_id;                       // "" = no group
	std::string clan_id;                         // "" = no clan
	std::string faction;                         // "Enforcer" / "Criminal" / ""
};

class ChatService {
public:
	// Flood policy (retail: kicked for too many msgs in a short period). Domain
	// mutes the sender for the remainder of the window instead of kicking.
	int32_t flood_limit    = 8;      // messages allowed per window
	int64_t flood_window_ms = 10000; // sliding window length

	// --- roster / prefs ---
	void AddPlayer(const std::string& name);
	void RemovePlayer(const std::string& name);
	bool HasPlayer(const std::string& name) const;
	PlayerChatState* Player(const std::string& name);             // nullptr if absent
	const PlayerChatState* Player(const std::string& name) const;

	void SetPresence(const std::string& name, PresenceStatus s);
	void SetProfanityFilter(const std::string& name, bool on);
	void SetGroup(const std::string& name, const std::string& group_id);
	void SetClan(const std::string& name, const std::string& clan_id);
	void SetFaction(const std::string& name, const std::string& faction);
	void AddIgnore(const std::string& name, const std::string& ignored_name);
	void RemoveIgnore(const std::string& name, const std::string& ignored_name);

	// Profanity list management (default set seeded in ctor; extendable at runtime).
	void AddProfanity(const std::string& word);

	// Parse a raw chat line into channel/target/body. Recognised slash commands:
	//   /w <name> <msg>  or /whisper /t /tell   -> Whisper
	//   /d <msg> /district                       -> District
	//   /l <msg> /local /s /say                   -> Local
	//   /g <msg> /group                           -> Group
	//   /f <msg> /faction                          -> Faction
	//   /c <msg> /clan                             -> Clan
	//   /trade <msg>                               -> Trade
	// Any other leading token or plain text uses `default_channel`.
	static ParsedChat ParseCommand(const std::string& raw, ChatChannel default_channel);

	// Route a message. Applies flood control, ignore lists, profanity masking and
	// presence rules; returns the resolved per-recipient deliveries.
	SubmitResult Submit(const std::string& sender, ChatChannel channel,
	                    const std::string& target, const std::string& body, int64_t now_ms);

	// Convenience: parse `raw` then Submit.
	SubmitResult SubmitRaw(const std::string& sender, const std::string& raw,
	                       ChatChannel default_channel, int64_t now_ms);

	// Server/system broadcast to everyone (bypasses flood/ignore/profanity).
	SubmitResult SystemBroadcast(const std::string& body, int64_t now_ms);

private:
	std::unordered_map<std::string, PlayerChatState> roster_;
	std::unordered_map<std::string, std::deque<int64_t>> send_times_; // sender -> recent send stamps
	std::unordered_set<std::string> profanity_;

	static std::string ToLower(const std::string& s);
	static std::string Trim(const std::string& s);
	std::string ApplyProfanity(const std::string& body) const;
	bool RegisterSendAndCheckFlood(const std::string& sender, int64_t now_ms); // true = allowed
	// Append a delivery to `out` for `recipient`, honoring their profanity pref/ignore.
	void EmitTo(std::vector<ChatDelivery>& out, const std::string& recipient,
	            const ChatMessage& base) const;
};

} // namespace apb
