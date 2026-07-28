// run_relay_tests.cpp — M7 N4 (Domain half): RelayCodec encode/decode/stream tests.
// Header-light: links only APBRelayProtocol.cpp. Pattern mirrors the other run_*_tests.cpp
// (static fails counter + CHECK macro + FAILS=<n> tail).
#include "APBRelayProtocol.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

// Field-complete equality for a decode round-trip.
static bool Same(const RelayMessage& a, const RelayMessage& b) {
	return a.verb == b.verb && a.district == b.district && a.numeric_id == b.numeric_id &&
	       a.port == b.port && a.account == b.account && a.character == b.character &&
	       a.faction == b.faction && a.jti == b.jti && a.from == b.from && a.to == b.to &&
	       a.body == b.body && a.player_count == b.player_count && a.seq == b.seq && a.ok == b.ok &&
	       a.operation_id == b.operation_id && a.authority_epoch == b.authority_epoch &&
	       a.revision == b.revision && a.social_op == b.social_op && a.social_status == b.social_status;
}

static bool RoundTrips(const RelayMessage& m) {
	RelayMessage back;
	if (!RelayCodec::Decode(RelayCodec::Encode(m), back)) return false;
	return Same(m, back);
}

static void TestVerbTokens() {
	RelayVerb all[] = { RelayVerb::Register, RelayVerb::RegisterAck, RelayVerb::Heartbeat,
		RelayVerb::ReportLoad, RelayVerb::ExpectTicket, RelayVerb::ExpectAck,
		RelayVerb::ChatRelay, RelayVerb::PlayerJoined, RelayVerb::PlayerLeft,
		RelayVerb::Handoff, RelayVerb::Return,
		RelayVerb::SocialRequest, RelayVerb::SocialResult,
		RelayVerb::SocialProjection, RelayVerb::SocialChat };
	bool sym = true;
	for (RelayVerb v : all)
		if (RelayCodec::VerbFromToken(RelayCodec::VerbToken(v)) != v) sym = false;
	CHECK(sym, "verb <-> token symmetric for all known verbs");
	CHECK(RelayCodec::VerbFromToken("nope") == RelayVerb::Unknown, "unknown token -> Unknown verb");
	CHECK(std::string(RelayCodec::VerbToken(RelayVerb::Unknown)) == "unknown", "Unknown verb token is 'unknown'");
}

static void TestRoundTripAllFactories() {
	CHECK(RoundTrips(RelayCodec::MakeRegister("Financial", 1, 17811)), "register round-trips");
	CHECK(RoundTrips(RelayCodec::MakeRegisterAck(1, true)), "register_ack(ok) round-trips");
	CHECK(RoundTrips(RelayCodec::MakeHeartbeat(11, 42)), "heartbeat round-trips");
	CHECK(RoundTrips(RelayCodec::MakeReportLoad(11, 37)), "report_load round-trips");
	CHECK(RoundTrips(RelayCodec::MakeExpectTicket("acct", "Nina", "Enforcer", "jti-abc", "Financial", 1)),
	      "expect ticket round-trips");
	CHECK(RoundTrips(RelayCodec::MakeExpectAck("jti-abc", true)), "expect_ack round-trips");
	CHECK(RoundTrips(RelayCodec::MakeChatRelay("Nina", "Zed", "meet at armas", 9)), "chat relay round-trips");
	CHECK(RoundTrips(RelayCodec::MakePlayerJoined("acct", "Nina", 1)), "player_joined round-trips");
	CHECK(RoundTrips(RelayCodec::MakePlayerLeft("acct", "Nina", 1)), "player_left round-trips");
}

static void TestFieldFidelity() {
	// A fully populated expect ticket must preserve every claim field verbatim.
	RelayMessage m = RelayCodec::MakeExpectTicket("bigacct", "Officer Down", "Criminal", "JTI-77", "Waterfront", 11);
	RelayMessage back;
	RelayCodec::Decode(RelayCodec::Encode(m), back);
	CHECK(back.account == "bigacct" && back.character == "Officer Down", "expect: account+character intact");
	CHECK(back.faction == "Criminal" && back.jti == "JTI-77", "expect: faction+jti intact");
	CHECK(back.district == "Waterfront" && back.numeric_id == 11, "expect: district+numeric_id intact");
}

static void TestOmittedFieldsCompact() {
	// A heartbeat should NOT carry empty string keys on the wire, and must be one '\n' line.
	std::string wire = RelayCodec::Encode(RelayCodec::MakeHeartbeat(5));
	CHECK(!wire.empty() && wire.back() == '\n', "encoded message ends with newline");
	CHECK(wire.find("\"account\"") == std::string::npos, "empty string fields are omitted from the wire");
	CHECK(wire.find('\n') == wire.size() - 1, "encode emits exactly one line");
	// seq==0 default is also omitted.
	CHECK(wire.find("\"seq\"") == std::string::npos, "zero seq omitted");
	// revision==0 on social_proj must be omitted too (same compact-wire contract).
	std::string sproj = RelayCodec::Encode(RelayCodec::MakeSocialProjection("Nina", 0, "epoch-1", "{}", 1));
	CHECK(sproj.find("\"revision\"") == std::string::npos, "zero revision omitted from social_proj wire");
}

static void TestAckExplicit() {
	// ok=false must round-trip for ack verbs (explicit rejection), and be present on the wire.
	RelayMessage reject = RelayCodec::MakeExpectAck("jti-x", false);
	std::string wire = RelayCodec::Encode(reject);
	CHECK(wire.find("\"ok\":false") != std::string::npos, "expect_ack emits ok:false explicitly");
	RelayMessage back;
	RelayCodec::Decode(wire, back);
	CHECK(back.verb == RelayVerb::ExpectAck && back.ok == false, "expect_ack(false) round-trips as rejection");
	// Non-ack verb: ok stays default false and is not emitted.
	CHECK(RelayCodec::Encode(RelayCodec::MakeHeartbeat(1)).find("\"ok\"") == std::string::npos,
	      "non-ack verb does not emit ok");
}

static void TestEscaping() {
	RelayMessage m = RelayCodec::MakeChatRelay("Nina", "Zed", "say \"hi\" then \\escape\\");
	RelayMessage back;
	CHECK(RelayCodec::Decode(RelayCodec::Encode(m), back), "message with quotes/backslashes decodes");
	CHECK(back.body == "say \"hi\" then \\escape\\", "quote + backslash body preserved verbatim");
	// A control char (newline inside body) is neutralised to a space so it can't break framing.
	RelayMessage nl = RelayCodec::MakeChatRelay("a", "b", "line1\nline2");
	std::string wire = RelayCodec::Encode(nl);
	CHECK(wire.find('\n') == wire.size() - 1, "embedded newline in body cannot inject a second line");
}

static void TestDecodeStream() {
	// Two complete messages + a partial third with no trailing newline yet.
	std::string buf = RelayCodec::Encode(RelayCodec::MakeRegister("Financial", 1, 17811));
	buf += RelayCodec::Encode(RelayCodec::MakeHeartbeat(1, 7));
	std::string partial = RelayCodec::Encode(RelayCodec::MakeReportLoad(1, 12));
	partial.pop_back(); // drop the '\n' -> partial line
	buf += partial;

	std::vector<RelayMessage> got = RelayCodec::DecodeStream(buf);
	CHECK(got.size() == 2, "stream yields the two complete messages");
	CHECK(got.size() == 2 && got[0].verb == RelayVerb::Register && got[1].verb == RelayVerb::Heartbeat,
	      "stream preserves message order");
	CHECK(!buf.empty() && buf.find("report_load") != std::string::npos, "partial trailing line retained in buffer");

	// Now the rest of the partial arrives.
	buf += "\n";
	std::vector<RelayMessage> rest = RelayCodec::DecodeStream(buf);
	CHECK(rest.size() == 1 && rest[0].verb == RelayVerb::ReportLoad && rest[0].player_count == 12,
	      "completing the partial line decodes it");
	CHECK(buf.empty(), "buffer fully consumed after final newline");
}

static void TestStreamRobustness() {
	std::string buf;
	buf += RelayCodec::Encode(RelayCodec::MakeHeartbeat(3, 0, "crlf-1", 0, ""));
	buf.back() = '\r';
	buf.push_back('\n');
	buf += "not json at all\n";
	buf += RelayCodec::Encode(RelayCodec::MakePlayerLeft("acct", "Nina", 3, "leave-1"));
	std::vector<RelayMessage> got = RelayCodec::DecodeStream(buf);
	CHECK(got.size() == 2, "CRLF line + valid line parsed, garbage skipped");
	CHECK(got.size() == 2 && got[0].verb == RelayVerb::Heartbeat && got[0].numeric_id == 3,
	      "CRLF-terminated line parsed with fields intact");
	CHECK(got.size() == 2 && got[1].verb == RelayVerb::PlayerLeft, "valid line after garbage still parsed");
}

static void TestDecodeRejects() {
	RelayMessage m;
	CHECK(!RelayCodec::Decode("", m), "empty line rejected");
	CHECK(!RelayCodec::Decode("{}", m), "object with no verb rejected");
	CHECK(!RelayCodec::Decode("{\"v\":\"bogus\"}", m), "unknown verb rejected");
	CHECK(RelayCodec::Decode("{\"version\":1,\"request_id\":\"minimal-1\",\"sent_ms\":0,\"auth\":\"\",\"verb\":\"heartbeat\",\"numeric_id\":9}", m) && m.numeric_id == 9,
	      "minimal valid heartbeat accepted");
	RelayMessage sr;
	CHECK(RelayCodec::Decode("{\"version\":1,\"request_id\":\"sreq-min\",\"sent_ms\":0,\"auth\":\"\",\"verb\":\"social_req\",\"operation_id\":\"op-min\",\"social_op\":\"clan.invite\"}", sr)
	      && sr.verb == RelayVerb::SocialRequest && sr.operation_id == "op-min" && sr.social_op == "clan.invite",
	      "hand-written minimal social_req decodes with operation_id and social_op");
}

static void TestRelayContractValidation() {
	RelayValidationOptions options;
	options.now_ms = 1000;
	options.expected_auth = "relay-secret";
	RelayInbox inbox(options);
	const std::string valid = RelayCodec::Encode(RelayCodec::MakeHeartbeat(9, 1, "req-1", 1000, "relay-secret"));
	CHECK(inbox.Submit(valid) == RelayRejectReason::None, "authenticated v1 frame accepted");
	CHECK(inbox.Submit(valid) == RelayRejectReason::DuplicateRequestId, "duplicate request id rejected");
	RelayMessage popped;
	CHECK(inbox.Pop(popped) && popped.request_id == "req-1", "accepted frame dequeued");
	CHECK(inbox.Submit(RelayCodec::Encode(RelayCodec::MakeHeartbeat(9, 2, "req-auth", 1000, "bad"))) == RelayRejectReason::AuthFailed,
	      "wrong auth rejected");
	CHECK(inbox.Submit(RelayCodec::Encode(RelayCodec::MakeHeartbeat(9, 3, "req-timeout", 3001, "relay-secret"))) == RelayRejectReason::TimedOut,
	      "timed-out request rejected");
	CHECK(inbox.Submit("{\"version\":2,\"request_id\":\"req-v2\",\"sent_ms\":1000,\"auth\":\"relay-secret\",\"verb\":\"heartbeat\"}") == RelayRejectReason::UnknownVersion,
	      "unknown protocol version rejected as unknown_version");
	CHECK(inbox.Submit("{\"version\":1,\"sent_ms\":1000,\"auth\":\"relay-secret\",\"verb\":\"heartbeat\"}") == RelayRejectReason::MissingRequestId,
	      "missing request id rejected");
	CHECK(inbox.Submit("not json") == RelayRejectReason::Malformed, "malformed frame rejected");
	CHECK(inbox.Submit(std::string(kRelayMaxFrameBytes + 1, 'x')) == RelayRejectReason::Oversize,
	      "oversize frame rejected as oversize");
	const std::string stale = RelayCodec::Encode(RelayCodec::MakeExpectTicket("acct", "Nina", "Enforcer", "jti-stale", "Financial", 1, "req-stale", 1000, 1000, "relay-secret"));
	CHECK(inbox.Submit(stale) == RelayRejectReason::StaleJti, "stale JTI rejected as stale_jti");
	const std::string ticket = RelayCodec::Encode(RelayCodec::MakeExpectTicket("acct", "Nina", "Enforcer", "jti-live", "Financial", 1, "req-jti-1", 1000, 2000, "relay-secret"));
	CHECK(inbox.Submit(ticket) == RelayRejectReason::None, "fresh JTI accepted");
	CHECK(inbox.Submit(RelayCodec::Encode(RelayCodec::MakeExpectTicket("acct", "Nina", "Enforcer", "jti-live", "Financial", 1, "req-jti-2", 1000, 2000, "relay-secret"))) == RelayRejectReason::ReplayJti,
	      "replayed JTI rejected");
	RelayInbox queue(options);
	int accepted = 0;
	for (int i = 0; i < static_cast<int>(kRelayMaxQueueDepth) + 1; ++i) {
		const std::string frame = RelayCodec::Encode(RelayCodec::MakeHeartbeat(9, i, "queue-" + std::to_string(i), 1000, "relay-secret"));
		if (queue.Submit(frame) == RelayRejectReason::None) ++accepted;
	}
	CHECK(accepted == static_cast<int>(kRelayMaxQueueDepth), "queue accepts exactly 256 frames");
	CHECK(queue.Size() == kRelayMaxQueueDepth, "queue depth is bounded at 256");
	CHECK(RelayReconnectDelayMs(0) == 250 && RelayReconnectDelayMs(1) == 500 && RelayReconnectDelayMs(5) == 5000,
	      "reconnect backoff is deterministic and capped");
}

static void TestSocialRoundTrips() {
	CHECK(RoundTrips(RelayCodec::MakeSocialRequest("acct", "Nina", "clan.invite", "op-1", "{\"target\":\"Zed\"}", 7)),
	      "social_req round-trips");
	CHECK(RoundTrips(RelayCodec::MakeSocialResult("Nina", "op-1", "ok", 42, "epoch-boot1", "{}", 7)),
	      "social_res round-trips");
	CHECK(RoundTrips(RelayCodec::MakeSocialProjection("Nina", 42, "epoch-boot1", "{\"friends\":[]}", 7)),
	      "social_proj round-trips");
	CHECK(RoundTrips(RelayCodec::MakeSocialChat("Nina", "clan", "sup", "group.chat", 7)),
	      "social_chat round-trips");
}

static void TestSocialFieldFidelity() {
	RelayMessage m = RelayCodec::MakeSocialResult("Officer Down", "op-xyz", "domain_rejected",
		99, "epoch-2026", "{\"reason\":\"cooldown\"}", 11, "rid-1", 1000, "secret");
	RelayMessage back;
	RelayCodec::Decode(RelayCodec::Encode(m), back);
	CHECK(back.character == "Officer Down", "social_res: character intact");
	CHECK(back.operation_id == "op-xyz", "social_res: operation_id intact");
	CHECK(back.social_status == "domain_rejected", "social_res: social_status intact");
	CHECK(back.revision == 99, "social_res: revision intact");
	CHECK(back.authority_epoch == "epoch-2026", "social_res: authority_epoch intact");
	CHECK(back.body == "{\"reason\":\"cooldown\"}", "social_res: json body intact");
}

static void TestSocialBodyEscaping() {
	RelayMessage m = RelayCodec::MakeSocialRequest("acct", "Nina", "friend.add", "op-esc",
		"say \"hi\" then \\escape\\\nsame frame", 0, "rid-esc");
	RelayMessage back;
	CHECK(RelayCodec::Decode(RelayCodec::Encode(m), back), "social body with quotes/backslash/newline decodes");
	std::string wire = RelayCodec::Encode(m);
	CHECK(wire.find('\n') == wire.size() - 1, "embedded newline in social body cannot inject a second frame");
	CHECK(back.body == "say \"hi\" then \\escape\\\nsame frame", "social body preserved verbatim");
}

static void TestSocialInboxContract() {
	RelayValidationOptions opts;
	opts.now_ms = 5000;
	opts.expected_auth = "relay-secret";
	RelayInbox inbox(opts);
	CHECK(inbox.Submit(std::string(kRelayMaxFrameBytes + 1, 'x')) == RelayRejectReason::Oversize,
	      "oversize social frame rejected as oversize");
	CHECK(inbox.Submit("{\"version\":1,\"request_id\":\"bad-sf\",\"sent_ms\":5000,\"auth\":\"relay-secret\",\"verb\":\"social_req\",BROKEN") == RelayRejectReason::Malformed,
	      "malformed social frame rejected as malformed");
	// Same operation_id under different request_ids must BOTH pass the inbox: operation_id
	// dedup is the world dispatcher's responsibility, not a transport-layer concern.
	const std::string a = RelayCodec::Encode(RelayCodec::MakeSocialRequest(
		"acct", "Nina", "clan.invite", "op-same", "{}", 9, "req-soc-a", 5000, "relay-secret"));
	const std::string b = RelayCodec::Encode(RelayCodec::MakeSocialRequest(
		"acct", "Nina", "clan.invite", "op-same", "{}", 9, "req-soc-b", 5000, "relay-secret"));
	CHECK(inbox.Submit(a) == RelayRejectReason::None, "first social_req with op-same accepted by inbox");
	CHECK(inbox.Submit(b) == RelayRejectReason::None,
	      "duplicate operation_id under a new request_id still accepted (dispatcher dedups, not inbox)");
}

static void TestSocialProjectionOrdering() {
	CHECK(ShouldApplySocialProjection("", 0, "epoch-1", 5),
	      "ordering: empty current epoch accepts any projection");
	CHECK(ShouldApplySocialProjection("epoch-1", 4, "epoch-1", 5),
	      "ordering: same epoch + higher revision applies");
	CHECK(!ShouldApplySocialProjection("epoch-1", 5, "epoch-1", 5),
	      "ordering: same epoch + equal revision rejected as duplicate");
	CHECK(!ShouldApplySocialProjection("epoch-1", 6, "epoch-1", 5),
	      "ordering: same epoch + lower revision rejected as stale");
	CHECK(ShouldApplySocialProjection("epoch-1", 100, "epoch-2", 1),
	      "ordering: epoch change wins even when revision goes backwards");
}

static void TestSocialDecodeStream() {
	std::string buf = RelayCodec::Encode(RelayCodec::MakeSocialRequest(
		"acct", "Nina", "clan.invite", "op-ds1", "{}", 3, "req-ds1"));
	buf += "this is garbage\n";
	std::string partial = RelayCodec::Encode(RelayCodec::MakeSocialProjection(
		"Nina", 7, "epoch-ds", "{}", 3, "req-ds2"));
	partial.pop_back();
	buf += partial;

	std::vector<RelayMessage> got = RelayCodec::DecodeStream(buf);
	CHECK(got.size() == 1, "social stream: complete social_req decoded, garbage skipped");
	CHECK(got.size() == 1 && got[0].verb == RelayVerb::SocialRequest && got[0].social_op == "clan.invite",
	      "social stream: decoded social_req keeps verb and social_op");
	CHECK(buf.find("social_proj") != std::string::npos, "social stream: partial social_proj retained in buffer");

	buf += "\n";
	std::vector<RelayMessage> rest = RelayCodec::DecodeStream(buf);
	CHECK(rest.size() == 1 && rest[0].verb == RelayVerb::SocialProjection && rest[0].revision == 7,
	      "social stream: completed social_proj decodes with revision intact");
	CHECK(buf.empty(), "social stream: buffer fully consumed");
}

int main() {
	std::printf("=== APB Relay Protocol Tests (M7 N4 codec) ===\n");
	TestVerbTokens();
	TestRoundTripAllFactories();
	TestFieldFidelity();
	TestOmittedFieldsCompact();
	TestAckExplicit();
	TestEscaping();
	TestDecodeStream();
	TestStreamRobustness();
	TestDecodeRejects();
	TestRelayContractValidation();
	TestSocialRoundTrips();
	TestSocialFieldFidelity();
	TestSocialBodyEscaping();
	TestSocialInboxContract();
	TestSocialProjectionOrdering();
	TestSocialDecodeStream();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
