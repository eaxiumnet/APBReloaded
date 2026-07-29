#include "APBHandoff.h"
#include "APBRelayProtocol.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace apb;
static int fails = 0;
#define CHECK(x, m) do { if (!(x)) { ++fails; std::printf("FAIL: %s\n", m); } else { std::printf("PASS: %s\n", m); } } while (0)

static bool SameSnapshot(const DomainSnapshot& a, const DomainSnapshot& b) {
	return a.has_character == b.has_character &&
		a.character_name == b.character_name &&
		a.faction == b.faction &&
		a.cash == b.cash && a.g1c == b.g1c &&
		a.inventory_slot_count == b.inventory_slot_count &&
		a.inventory_total_qty == b.inventory_total_qty &&
		std::abs(a.threat_points - b.threat_points) < 0.00001 &&
		a.mission_id == b.mission_id && a.mission_title == b.mission_title &&
		a.mission_stage_index == b.mission_stage_index &&
		a.mission_stage_count == b.mission_stage_count &&
		a.mission_status == b.mission_status &&
		a.session_id == b.session_id && a.district_id == b.district_id &&
		a.contact_standings.size() == b.contact_standings.size() &&
		a.role_xp.size() == b.role_xp.size();
}

// S6 (defect 6): RestoreHandoff takes cash wholesale from the district snapshot,
// so a district return carrying a pre-claim cash value silently reverts a credit
// the world already committed and journalled. Reconciliation must reapply every
// committed receipt exactly once, before the snapshot is persisted.
static std::string ClaimTestDir() {
	const char* tmp = std::getenv("TEMP");
	return std::string(tmp ? tmp : ".") + "\\apb_handoff_claim_test";
}

static void SeedCommittedClaim(const std::string& dir, const std::string& character,
	int64_t mail_id, int64_t cash_delta) {
	std::error_code ec;
	std::filesystem::remove_all(std::filesystem::path(dir), ec);
	std::filesystem::create_directories(std::filesystem::path(dir + "\\social"), ec);
	const std::string body = "{\"claims\":[{\"character\":\"" + character +
		"\",\"mail_id\":" + std::to_string(mail_id) +
		",\"state\":\"MailCommitted\",\"cash_delta\":" + std::to_string(cash_delta) +
		",\"claimed_utc\":5000}]}";
	std::FILE* f = nullptr;
	const std::string path = dir + "\\social\\mail_claims.json";
	if (fopen_s(&f, path.c_str(), "wb") == 0 && f) {
		std::fwrite(body.data(), 1, body.size(), f);
		std::fclose(f);
	}
}

static void TestHandoffReconcilesClaim() {
	const std::string dir = ClaimTestDir();
	SeedCommittedClaim(dir, "ProbeChar", 7, 3000);

	DomainSnapshot stale;
	stale.has_character = true;
	stale.character_name = "ProbeChar";
	stale.faction = Faction::Enforcer;
	stale.cash = 10000;
	stale.g1c = 0;
	stale.threat_points = 1.0;

	WorldService svc;
	CHECK(svc.InitSocialPersistence(dir), "social persistence init for claim journal");
	CHECK(svc.ApplyHandoffForAccount(stale, "account"), "stale district return applies");
	CHECK(svc.character.has_value(), "character present after handoff");
	CHECK(svc.character.has_value() && svc.character->cash == 13000,
		"committed claim delta reapplied over stale snapshot cash (10000 + 3000)");

	const DomainSnapshot after = svc.CaptureSnapshot();
	CHECK(after.cash == 13000, "reconciled cash is what gets captured onward");

	CHECK(svc.ApplyHandoffForAccount(stale, "account"), "second stale return applies");
	CHECK(svc.character.has_value() && svc.character->cash == 13000,
		"reconciliation is idempotent - no second credit on repeat handoff");
}

int main() {
	DomainSnapshot snapshot;
	snapshot.has_character = true;
	snapshot.character_name = "ProbeChar";
	snapshot.faction = Faction::Enforcer;
	snapshot.cash = 12345;
	snapshot.g1c = 99;
	snapshot.inventory_slot_count = 2;
	snapshot.inventory_total_qty = 7;
	snapshot.threat_points = 42.5;
	snapshot.mission_id = "mission_probe";
	snapshot.mission_title = "Probe Mission";
	snapshot.mission_stage_index = 2;
	snapshot.mission_stage_count = 4;
	snapshot.mission_status = "Active";
	snapshot.session_id = "session-probe";
	snapshot.district_id = "Financial";
	snapshot.contact_standings.push_back({"Financial_C01", 250});
	snapshot.role_xp.push_back({"Enforcer", 90});
	const std::string encoded = SerializeSnapshot(snapshot);
	DomainSnapshot decoded;
	CHECK(DeserializeSnapshot(encoded, decoded), "snapshot deserialize succeeds");
	CHECK(SameSnapshot(snapshot, decoded), "snapshot full parity holds");
	CharacterHandoff handoff{"account", "ProbeChar", "Enforcer", "jti-1", "nonce-1", 1000, snapshot};
	const std::string signed_handoff = SignHandoff(handoff, "0123456789abcdef0123456789abcdef");
	CharacterHandoff verified;
	CHECK(VerifyHandoff(signed_handoff, "0123456789abcdef0123456789abcdef", verified), "signed handoff verifies");
	CHECK(verified.account == "account" && verified.nonce == "nonce-1" && verified.snapshot.cash == 12345,
		"handoff binding survives verify");
	std::string tampered = signed_handoff;
	tampered[tampered.find("ProbeChar")] = 'X';
	CHECK(!VerifyHandoff(tampered, "0123456789abcdef0123456789abcdef", verified), "tampered handoff rejects");
	RelayMessage relay = RelayCodec::MakeHandoff("account", "ProbeChar", "Enforcer", "jti-1", "nonce-1", signed_handoff, 1, "handoff-1", 1000, "secret");
	RelayMessage back;
	CHECK(RelayCodec::Decode(RelayCodec::Encode(relay), back) && back.verb == RelayVerb::Handoff && back.body == signed_handoff && back.nonce == "nonce-1",
		"handoff relay round-trips");
	relay = RelayCodec::MakeReturn("account", "ProbeChar", "Enforcer", "jti-1", "nonce-return", signed_handoff, 1, "return-1", 1000, "secret");
	CHECK(RelayCodec::Decode(RelayCodec::Encode(relay), back) && back.verb == RelayVerb::Return && back.nonce == "nonce-return",
		"return relay round-trips");

	WorldService applied;
	CHECK(applied.ApplyHandoff(decoded), "snapshot applies to authoritative service");
	CHECK(SameSnapshot(decoded, applied.CaptureSnapshot()), "applied snapshot preserves character handoff parity");
	const DomainSnapshot before_reject = applied.CaptureSnapshot();
	DomainSnapshot invalid = decoded;
	invalid.inventory_slot_count = 3;
	invalid.inventory_total_qty = 2;
	CHECK(!applied.ApplyHandoff(invalid), "invalid handoff rejects before mutation");
	CHECK(SameSnapshot(before_reject, applied.CaptureSnapshot()), "rejected handoff leaves prior state intact");
	TestHandoffReconcilesClaim();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
