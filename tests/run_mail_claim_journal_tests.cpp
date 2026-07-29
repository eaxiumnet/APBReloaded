// run_mail_claim_journal_tests.cpp — M14 S10: durable mail-claim journal.
//
// The claim spans two aggregates the world owns separately (the character's cash
// and the mail message's claimed flag). A crash between them must never lose or
// duplicate a credit, so the journal records {character, mail_id} -> state and
// the CHARACTER RECEIPT is the idempotency fact. The key is deliberately NOT an
// operation_id: that neither survives a world restart nor dedups a retry
// re-issued after district travel with a fresh id.
#include "APBMailClaimJournal.h"
#include <cstdio>
#include <filesystem>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("  FAIL: %s\n", msg); } } while (0)

static std::string TestDir() {
	const char* tmp = std::getenv("TEMP");
	return std::string(tmp ? tmp : ".") + "\\apb_mail_claim_test";
}

static void ResetDir() {
	std::error_code ec;
	std::filesystem::remove_all(std::filesystem::path(TestDir()), ec);
	std::filesystem::create_directories(std::filesystem::path(TestDir()), ec);
}

static void WriteFixture(const std::string& text) {
	std::FILE* f = nullptr;
	const std::string path = TestDir() + "\\mail_claims.json";
	if (fopen_s(&f, path.c_str(), "wb") == 0 && f) {
		std::fwrite(text.data(), 1, text.size(), f);
		std::fclose(f);
	}
}

// ---------------------------------------------------------------------------
static void TestInitAndEmptyLoad() {
	std::printf("[Journal] init + tolerant empty load\n");
	ResetDir();
	MailClaimJournal j;
	CHECK(!j.Init(""), "empty dir refused");
	CHECK(!j.IsActive(), "inactive after refused init");
	CHECK(j.Init(TestDir()), "init on real dir ok");
	CHECK(j.IsActive(), "active after init");
	CHECK(j.Path() == TestDir() + "/mail_claims.json", "journal path is <dir>/mail_claims.json");
	CHECK(!j.Load(), "load with no file returns false (nothing loaded)");
	CHECK(j.Count() == 0, "no receipts on fresh start");
}

// ---------------------------------------------------------------------------
static void TestStateMachineOrdering() {
	std::printf("[Journal] Prepared -> CharacterCommitted -> MailCommitted\n");
	ResetDir();
	MailClaimJournal j;
	j.Init(TestDir());

	CHECK(!j.CommitCharacter("bob", 7), "cannot commit character before Prepare");
	CHECK(!j.CommitMail("bob", 7), "cannot commit mail before Prepare");

	CHECK(j.Prepare("bob", 7, 1000, 5000), "prepare ok");
	CHECK(j.Count() == 1, "one receipt after prepare");
	const MailClaimReceipt* r = j.Find("bob", 7);
	CHECK(r && r->state == MailClaimState::Prepared, "state Prepared");
	CHECK(r && r->cash_delta == 1000, "cash delta recorded");
	CHECK(r && r->claimed_utc == 5000, "caller-supplied clock recorded");
	CHECK(!j.Prepare("bob", 7, 1000, 5000), "duplicate prepare refused");

	CHECK(!j.CommitMail("bob", 7), "cannot skip straight to MailCommitted");

	CHECK(j.CommitCharacter("bob", 7), "commit character ok");
	r = j.Find("bob", 7);
	CHECK(r && r->state == MailClaimState::CharacterCommitted, "state CharacterCommitted");
	CHECK(!j.CommitCharacter("bob", 7), "double character commit refused");

	CHECK(j.CommitMail("bob", 7), "commit mail ok");
	r = j.Find("bob", 7);
	CHECK(r && r->state == MailClaimState::MailCommitted, "state MailCommitted");
	CHECK(!j.CommitMail("bob", 7), "double mail commit refused");

	CHECK(j.Find("bob", 8) == nullptr, "other mail id absent");
	CHECK(j.Find("alice", 7) == nullptr, "same id different character is a distinct key");
}

// ---------------------------------------------------------------------------
static void TestPersistenceRoundTrip() {
	std::printf("[Journal] every transition persists immediately\n");
	ResetDir();
	{
		MailClaimJournal j;
		j.Init(TestDir());
		j.Prepare("bob", 7, 1000, 5000);
		j.Prepare("alice", 42, 250, 6000);
		j.CommitCharacter("alice", 42);
	}
	MailClaimJournal reloaded;
	CHECK(reloaded.Init(TestDir()), "reinit ok");
	CHECK(reloaded.Load(), "load finds the persisted journal");
	CHECK(reloaded.Count() == 2, "both receipts survived");
	const MailClaimReceipt* b = reloaded.Find("bob", 7);
	const MailClaimReceipt* a = reloaded.Find("alice", 42);
	CHECK(b && b->state == MailClaimState::Prepared, "bob still Prepared across restart");
	CHECK(b && b->cash_delta == 1000, "bob cash delta survived");
	CHECK(a && a->state == MailClaimState::CharacterCommitted, "alice still CharacterCommitted");
	CHECK(a && a->claimed_utc == 6000, "alice clock survived");
}

// S2: crash after Prepared. Neither aggregate moved, so recovery re-executes the
// whole claim; the credit must land exactly once.
static void TestCrashAfterPrepared() {
	std::printf("[Journal] S2 crash after Prepared -> re-execute, credit once\n");
	ResetDir();
	WriteFixture("{\"claims\":[{\"character\":\"bob\",\"mail_id\":7,\"state\":\"Prepared\",\"cash_delta\":1000,\"claimed_utc\":5000}]}");
	MailClaimJournal j;
	j.Init(TestDir());
	CHECK(j.Load(), "post-crash journal loaded");
	const MailClaimReceipt* r = j.Find("bob", 7);
	CHECK(r && r->state == MailClaimState::Prepared, "recovered state is Prepared");
	CHECK(!j.CashAlreadyCredited("bob", 7), "Prepared means cash NOT yet credited");
	CHECK(!j.MailAlreadyCommitted("bob", 7), "Prepared means mail NOT yet committed");
	CHECK(j.CommitCharacter("bob", 7), "recovery credits and commits character");
	CHECK(j.CashAlreadyCredited("bob", 7), "credited after recovery");
	CHECK(j.CommitMail("bob", 7), "recovery commits mail");
}

// S3: crash after CharacterCommitted. The cash is already durably credited, so
// recovery must NOT credit again - it only finishes the mail flag.
static void TestCrashAfterCharacterCommitted() {
	std::printf("[Journal] S3 crash after CharacterCommitted -> no double credit\n");
	ResetDir();
	WriteFixture("{\"claims\":[{\"character\":\"bob\",\"mail_id\":7,\"state\":\"CharacterCommitted\",\"cash_delta\":1000,\"claimed_utc\":5000}]}");
	MailClaimJournal j;
	j.Init(TestDir());
	CHECK(j.Load(), "post-crash journal loaded");
	CHECK(j.CashAlreadyCredited("bob", 7), "cash already credited - must not re-credit");
	CHECK(!j.MailAlreadyCommitted("bob", 7), "mail flag not yet committed");
	CHECK(!j.CommitCharacter("bob", 7), "re-crediting the character is refused");
	CHECK(j.CommitMail("bob", 7), "recovery finishes the mail commit");
	CHECK(j.MailAlreadyCommitted("bob", 7), "terminal after recovery");
}

// S4: crash after MailCommitted. Terminal state - a replayed claim is a no-op
// that reports the committed result rather than crediting a second time.
static void TestCrashAfterMailCommitted() {
	std::printf("[Journal] S4 crash after MailCommitted -> replay is a no-op\n");
	ResetDir();
	WriteFixture("{\"claims\":[{\"character\":\"bob\",\"mail_id\":7,\"state\":\"MailCommitted\",\"cash_delta\":1000,\"claimed_utc\":5000}]}");
	MailClaimJournal j;
	j.Init(TestDir());
	CHECK(j.Load(), "post-crash journal loaded");
	CHECK(j.CashAlreadyCredited("bob", 7), "cash credited");
	CHECK(j.MailAlreadyCommitted("bob", 7), "mail committed");
	CHECK(!j.Prepare("bob", 7, 1000, 9999), "replayed claim refuses a fresh prepare");
	CHECK(!j.CommitCharacter("bob", 7), "replayed credit refused");
	CHECK(!j.CommitMail("bob", 7), "replayed mail commit refused");
	const MailClaimReceipt* r = j.Find("bob", 7);
	CHECK(r && r->cash_delta == 1000, "original delta untouched by replay");
	CHECK(r && r->claimed_utc == 5000, "original clock untouched by replay");
}

// Defect 6 support: the handoff reconciler asks for every committed receipt so a
// stale district snapshot cannot silently revert a world-committed credit.
static void TestCommittedReceiptsForCharacter() {
	std::printf("[Journal] committed receipts drive handoff reconciliation\n");
	ResetDir();
	MailClaimJournal j;
	j.Init(TestDir());
	j.Prepare("bob", 1, 100, 1000);
	j.Prepare("bob", 2, 250, 1000); j.CommitCharacter("bob", 2);
	j.Prepare("bob", 3, 400, 1000); j.CommitCharacter("bob", 3); j.CommitMail("bob", 3);
	j.Prepare("alice", 4, 999, 1000); j.CommitCharacter("alice", 4);

	std::vector<MailClaimReceipt> bob = j.CommittedReceiptsFor("bob");
	CHECK(bob.size() == 2, "only committed receipts returned (Prepared excluded)");
	int64_t total = 0;
	for (const auto& r : bob) total += r.cash_delta;
	CHECK(total == 650, "committed deltas sum to 650, excluding the Prepared 100");
	CHECK(j.CommittedReceiptsFor("alice").size() == 1, "alice receipts isolated");
	CHECK(j.CommittedReceiptsFor("nobody").empty(), "unknown character has none");
}

int main() {
	std::printf("=== APB Mail Claim Journal Tests (M14 S10) ===\n");
	TestInitAndEmptyLoad();
	TestStateMachineOrdering();
	TestPersistenceRoundTrip();
	TestCrashAfterPrepared();
	TestCrashAfterCharacterCommitted();
	TestCrashAfterMailCommitted();
	TestCommittedReceiptsForCharacter();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
