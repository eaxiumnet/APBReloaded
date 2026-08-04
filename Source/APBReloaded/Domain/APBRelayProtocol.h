#pragma once
// APBRelayProtocol.h - M7 N4 (Domain half): pure-C++17 codec for the world<->district
// control channel messages. NO platform/UE headers - unit-testable in isolation like
// ChatService/TicketService. This is the MESSAGE-FORMAT layer only; the FSocket TCP
// transport + role wiring (N4 proper) live in Systems/Server/ and are Sisyphus's task.
// A district's recv loop can DecodeStream() a running byte buffer; the world can Encode()
// a message straight to send(). Keeping the wire format here means both ends share one
// tested definition instead of hand-rolling JSON in the socket handler.
//
// Grounded on the M7 relay verbs named in work/_active.md (M7 Files: APBServerControl
// "register/heartbeat/ticket/char.handoff/chat.relay") and work/m7_spec.md sections 4/7
// (ASK_DISTRICT_EXPECT{acct,jti}, REPORT_LOAD), plus the chat handoff (cross-district
// whisper forward). Ports/addresses are NOT resolved here — numeric_id/port are carried
// as plain ints so the Domain stays free of Systems/APBPorts.h (which lives above it).
//
// Wire format: one compact JSON envelope per line, '\n'-terminated (TCP is a stream, so
// framing is line-delimited). Version, request_id, sent_ms, and auth are mandatory on
// every frame. The payload fields are flat and verb-specific.
#include <string>
#include <vector>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_set>

namespace apb {

// Relay contract constants. Keep these in the Domain so every transport can enforce the
// same limits without depending on UE or the APBPorts header.
inline constexpr int32_t kRelayProtocolVersion = 1;
inline constexpr size_t kRelayMaxFrameBytes = 64u * 1024u;
inline constexpr int64_t kRelayRequestTimeoutMs = 2000;
inline constexpr size_t kRelayMaxQueueDepth = 256u;
inline constexpr int64_t kRelayHeartbeatIntervalMs = 5000;
inline constexpr int32_t kRelayEvictionHeartbeats = 2;
inline constexpr int64_t kRelayReconnectInitialDelayMs = 250;
inline constexpr int64_t kRelayReconnectMaxDelayMs = 5000;

// Control-channel verbs. Direction annotations: D->W district to world, W->D world to
// district, W<->D either way.
enum class RelayVerb {
	Unknown,       // parse failure / unrecognised verb
	Register,      // D->W: district instance comes online (district,numeric_id,port)
	RegisterAck,   // W->D: world accepted/rejected the registration (ok)
	Heartbeat,     // D->W: periodic liveness (numeric_id[,seq]); numeric_id is an opaque, positive, globally-unique relay district-INSTANCE id — NOT districts.json numeric_id; district TYPE is carried by district (canonical name)
	ReportLoad,    // D->W: current occupancy for load balancing (numeric_id,player_count)
	ExpectTicket,  // W->D: ASK_DISTRICT_EXPECT — pre-authorise a jti (account,character,faction,jti,district,numeric_id)
	ExpectAck,     // D->W: district will honour (ok=true) or refuses (ok=false) the jti
	ChatRelay,     // W<->D: cross-district message forward (from,to,body[,numeric_id])
	PlayerJoined,  // D->W: a redeemed ticket spawned a player (presence up) (account,character,numeric_id)
	PlayerLeft,    // D->W: a player disconnected (presence down) (account,character,numeric_id)
	Handoff,       // W->D: signed world character snapshot (account,character,faction,jti,nonce,body)
	Return,        // D->W: signed final character snapshot (account,character,faction,jti,nonce,body)
	SocialRequest, // D->W: district asks the world social authority to run one op (account,character,social_op,operation_id,body)
	SocialResult,  // W->D: authoritative outcome for one operation_id (character,operation_id,social_status,revision,authority_epoch,body)
	SocialProjection, // W->D: authoritative social state for a character (character,revision,authority_epoch,body)
	SocialChat     // W<->D: social-channel chat fan-out (from,to,body,social_op)
};

// Flat message record. A single struct covers every verb; each verb populates a documented
// subset (see the enum comments). Defaults are chosen so an omitted field is unambiguous.
struct RelayMessage {
	int32_t     version = kRelayProtocolVersion;
	std::string request_id;   // unique per connection; duplicate IDs are rejected
	int64_t     sent_ms = 0;  // sender monotonic timestamp used for timeout validation
	std::string auth;         // shared deployment secret (never logged)
	RelayVerb   verb = RelayVerb::Unknown;
	std::string district;     // district id/name, e.g. "Financial"
	int32_t     numeric_id = 0; // opaque, positive, globally-unique relay district-INSTANCE id — NOT districts.json numeric_id; district TYPE is carried by district (canonical name)
	int32_t     port = 0;       // district NetDriver port (world hands this to the client)
	std::string target_district_epoch; // Unique district instance boot identifier
	std::string account;      // account login
	std::string character;    // selected character
	std::string faction;      // "Enforcer" / "Criminal" / ""
	std::string jti;          // ticket id being pre-authorised / redeemed
	std::string nonce;        // random handoff/return anti-replay identifier
	int64_t     jti_expires_ms = 0; // absolute expiry for ExpectTicket relay validation
	std::string from;         // chat sender
	std::string to;           // chat recipient
	std::string body;         // chat text / free-form payload
	int32_t     player_count = 0; // ReportLoad occupancy
	int64_t     seq = 0;        // optional correlation / heartbeat sequence
	bool        ok = false;     // ack outcome (RegisterAck / ExpectAck)
	std::string operation_id;  // district-generated idempotency key; the world dedups on {district, operation_id}
	std::string authority_epoch; // world social-authority incarnation; changes on world restart
	int64_t     revision = 0;  // monotonic social-state revision within one authority_epoch
	std::string social_op;     // namespaced operation, e.g. "clan.invite" / "friend.add" / "group.chat"
	std::string social_status; // outcome token for SocialResult, e.g. "ok" / "domain_rejected"
};

// Stateless codec. All methods are static; no instance state to share across threads.
class RelayCodec {
public:
	// Verb <-> stable short wire token ("register","expect","chat",...).
	static const char* VerbToken(RelayVerb v);
	static RelayVerb    VerbFromToken(const std::string& tok);

	// Serialise one message to a single '\n'-terminated JSON line.
	static std::string Encode(const RelayMessage& m);

	// Parse one JSON object (a single line, with or without the trailing '\n').
	// Returns false (and leaves out.verb == Unknown) if no/unknown verb.
	static bool Decode(const std::string& line, RelayMessage& out);

	// Return false and leave out reset when the frame exceeds the contract size.
	static bool IsFrameSizeValid(const std::string& line);

	// De-frame a running TCP receive buffer: extract every complete '\n'-terminated
	// line into `out`, then ERASE the consumed prefix from `buffer`, leaving any partial
	// trailing line for the next recv. Unparseable lines are skipped (not fatal to the
	// stream). This is exactly what an FSocket read loop needs.
	static std::vector<RelayMessage> DecodeStream(std::string& buffer);

	// --- factories (make N4 wiring self-documenting) ---
	static RelayMessage MakeRegister(const std::string& district, int32_t numeric_id, int32_t port,
	                                 const std::string& request_id = "", int64_t sent_ms = 0,
	                                 const std::string& auth = "");
	static RelayMessage MakeRegisterAck(int32_t numeric_id, bool ok,
	                                    const std::string& request_id = "", int64_t sent_ms = 0,
	                                    const std::string& auth = "");
	static RelayMessage MakeHeartbeat(int32_t numeric_id, int64_t seq = 0,
	                                  const std::string& request_id = "", int64_t sent_ms = 0,
	                                  const std::string& auth = "");
	static RelayMessage MakeReportLoad(int32_t numeric_id, int32_t player_count,
	                                   const std::string& request_id = "", int64_t sent_ms = 0,
	                                   const std::string& auth = "");
	static RelayMessage MakeExpectTicket(const std::string& account, const std::string& character,
	                                     const std::string& faction, const std::string& jti,
	                                     const std::string& district, int32_t numeric_id,
	                                     const std::string& request_id = "", int64_t sent_ms = 0,
	                                     int64_t jti_expires_ms = 0, const std::string& auth = "");
	static RelayMessage MakeExpectAck(const std::string& jti, bool ok,
	                                  const std::string& request_id = "", int64_t sent_ms = 0,
	                                  const std::string& auth = "");
	static RelayMessage MakeChatRelay(const std::string& from, const std::string& to,
 	                                  const std::string& body, int32_t numeric_id = 0,
 	                                  const std::string& request_id = "", int64_t sent_ms = 0,
 	                                  const std::string& auth = "");
	static RelayMessage MakePlayerJoined(const std::string& account, const std::string& character,
	                                     int32_t numeric_id, const std::string& request_id = "",
	                                     int64_t sent_ms = 0, const std::string& auth = "");
	static RelayMessage MakePlayerLeft(const std::string& account, const std::string& character,
	                                   int32_t numeric_id, const std::string& request_id = "",
	                                   int64_t sent_ms = 0, const std::string& auth = "");
	static RelayMessage MakeHandoff(const std::string& account, const std::string& character,
		const std::string& faction, const std::string& jti, const std::string& nonce,
		const std::string& body, int32_t numeric_id,
		const std::string& request_id = "", int64_t sent_ms = 0,
		const std::string& auth = "");
	static RelayMessage MakeReturn(const std::string& account, const std::string& character,
		const std::string& faction, const std::string& jti, const std::string& nonce,
		const std::string& body, int32_t numeric_id,
		const std::string& request_id = "", int64_t sent_ms = 0,
		const std::string& auth = "");
	static RelayMessage MakeSocialRequest(const std::string& account, const std::string& character,
		const std::string& social_op, const std::string& operation_id, const std::string& body,
		int32_t numeric_id = 0, const std::string& request_id = "", int64_t sent_ms = 0,
		const std::string& auth = "");
	static RelayMessage MakeSocialResult(const std::string& character, const std::string& operation_id,
		const std::string& social_status, int64_t revision, const std::string& authority_epoch,
		const std::string& body, int32_t numeric_id = 0, const std::string& request_id = "",
		int64_t sent_ms = 0, const std::string& auth = "");
	static RelayMessage MakeSocialProjection(const std::string& character, int64_t revision,
		const std::string& authority_epoch, const std::string& body, int32_t numeric_id = 0,
		const std::string& request_id = "", int64_t sent_ms = 0, const std::string& auth = "");
	static RelayMessage MakeSocialChat(const std::string& from, const std::string& to,
		const std::string& body, const std::string& social_op, int32_t numeric_id = 0,
		const std::string& request_id = "", int64_t sent_ms = 0, const std::string& auth = "");
};

enum class RelayRejectReason {
	None,
	Malformed,
	Oversize,
	UnknownVersion,
	MissingRequestId,
	DuplicateRequestId,
	TimedOut,
	AuthFailed,
	StaleJti,
	ReplayJti,
	QueueFull,
	VerbDirection,
	IdentityMismatch
};

const char* RelayRejectReasonName(RelayRejectReason reason);

struct RelayValidationOptions {
	int64_t now_ms = 0;
	std::string expected_auth;
	bool require_auth = true;
	std::optional<bool> is_world_server;
	int32_t expected_numeric_id = 0;
};

// Stateful receive-side contract enforcement. It bounds memory and rejects malformed,
// stale, unauthenticated, duplicate, and over-capacity relay traffic before the transport
// hands a message to a Domain service.
class RelayInbox {
public:
	explicit RelayInbox(RelayValidationOptions options = {});

	RelayRejectReason Submit(const std::string& frame);
	bool Pop(RelayMessage& out);
	void Clear();
	size_t Size() const { return queue_.size(); }
	const RelayValidationOptions& Options() const { return options_; }

private:
	void RememberRequest(const std::string& request_id);
	void RememberJti(const std::string& jti);
	RelayValidationOptions options_;
	std::deque<RelayMessage> queue_;
	std::deque<std::string> request_history_;
	std::unordered_set<std::string> request_ids_;
	std::deque<std::string> jti_history_;
	std::unordered_set<std::string> jtis_;
};

// Deterministic exponential reconnect schedule for a district relay client.
int64_t RelayReconnectDelayMs(int32_t attempt);

// Projection ordering guard. A district applies an incoming social projection only when it
// is strictly newer than what it already holds. An authority_epoch change always wins:
// revisions from different world incarnations are not comparable, so a lower revision under
// a new epoch is still newer. An empty current epoch means the district knows nothing yet.
bool ShouldApplySocialProjection(const std::string& current_epoch, int64_t current_revision,
	const std::string& incoming_epoch, int64_t incoming_revision);

} // namespace apb
