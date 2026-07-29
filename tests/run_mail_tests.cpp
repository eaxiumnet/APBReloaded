// run_mail_tests.cpp — M14 (D10): retail mail attachment/claim/delete semantics.
// Header-only: MailService lives entirely in APBSocial.h (inline), so this suite
// needs NO .cpp sources — it exercises the pure in-memory mailbox behavior.
//
// Grounded on retail APB mail: cash+item attachments are transferred via a single
// "Take All" claim; a message with unclaimed attachments cannot be discarded.
#include "APBSocial.h"
#include <cstdio>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("  FAIL: %s\n", msg); } } while (0)

// ---------------------------------------------------------------------------
static void TestSendInboxUnread() {
	std::printf("[Mail] send + inbox + unread count\n");
	MailService m;
	CHECK(!m.SendMail("", "bob", "s", "b"), "empty sender rejected");
	CHECK(!m.SendMail("alice", "", "s", "b"), "empty recipient rejected");
	CHECK(m.SendMail("alice", "bob", "hi", "text"), "plain send ok");
	CHECK(m.SendMail("alice", "bob", "cash", "here", 500), "cash send ok");
	CHECK(m.SendMail("alice", "carol", "hey", "yo"), "send to carol");
	CHECK(m.InboxFor("bob").size() == 2, "bob has 2 messages");
	CHECK(m.InboxFor("carol").size() == 1, "carol has 1 message");
	CHECK(m.UnreadCount("bob") == 2, "bob 2 unread");
	auto inbox = m.InboxFor("bob");
	CHECK(m.MarkRead(inbox[0]->id), "mark first read");
	CHECK(m.UnreadCount("bob") == 1, "bob 1 unread after read");
}

// ---------------------------------------------------------------------------
static void TestClaimSemantics() {
	std::printf("[Mail] claim attachments (Take All) once\n");
	MailService m;
	// cash-only send produces exactly one attachment.
	m.SendMail("system", "bob", "reward", "gz", 1000);
	int64_t id = m.messages.back().id;
	CHECK(m.HasUnclaimedAttachments(id), "has unclaimed attachment");
	CHECK(m.UnreadCount("bob") == 1, "unread before claim");

	auto got = m.ClaimAttachments(id);
	CHECK(got.size() == 1, "claim returns one attachment");
	CHECK(got.size() == 1 && got[0].cash == 1000, "claimed cash amount matches");
	CHECK(!m.HasUnclaimedAttachments(id), "no unclaimed after claim");
	CHECK(m.UnreadCount("bob") == 0, "claim marks message read");

	// second claim yields nothing (no double-dip).
	auto again = m.ClaimAttachments(id);
	CHECK(again.empty(), "second claim returns empty");
	// attachment record is retained on the message for history.
	CHECK(!m.messages.back().attachments.empty(), "attachment record retained after claim");

	// claiming a non-existent id and a no-attachment message returns empty.
	m.SendMail("alice", "bob", "plain", "nothing here");
	int64_t plainId = m.messages.back().id;
	CHECK(m.ClaimAttachments(plainId).empty(), "claim on no-attachment mail empty");
	CHECK(m.ClaimAttachments(999999).empty(), "claim on missing id empty");
	CHECK(!m.HasUnclaimedAttachments(plainId), "no-attachment mail has nothing to claim");
}

// ---------------------------------------------------------------------------
static void TestDeleteRules() {
	std::printf("[Mail] delete requires claim first\n");
	MailService m;
	// message with cash attachment: cannot delete until claimed.
	m.SendMail("system", "bob", "loot", "take it", 250);
	int64_t withAtt = m.messages.back().id;
	CHECK(!m.Delete(withAtt), "cannot delete unclaimed-attachment mail");
	CHECK(m.InboxFor("bob").size() == 1, "message still present after blocked delete");
	m.ClaimAttachments(withAtt);
	CHECK(m.Delete(withAtt), "delete allowed after claim");
	CHECK(m.InboxFor("bob").empty(), "message removed after delete");

	// no-attachment message: deletable immediately.
	m.SendMail("alice", "bob", "note", "read me");
	int64_t plain = m.messages.back().id;
	CHECK(m.Delete(plain), "no-attachment mail deletes immediately");
	CHECK(!m.Delete(plain), "deleting missing id fails");
}

// ---------------------------------------------------------------------------
static void TestExpiry() {
	std::printf("[Mail] expiry purge (caller-supplied clock)\n");
	MailService m;
	// created_utc supplied as epoch seconds (5th arg = cash, 6th = created_utc).
	m.SendMail("system", "bob", "old", "stale", 0, 1000);   // old
	m.SendMail("system", "bob", "new", "fresh", 0, 5000);   // recent
	m.SendMail("system", "bob", "legacy", "no ts");         // created_utc = 0 -> never expires
	CHECK(m.InboxFor("bob").size() == 3, "three messages before purge");

	// ttl 3000s at now=5000: old (age 4000) expires; new (age 0) + legacy stay.
	int32_t removed = m.PurgeExpired(5000, 3000);
	CHECK(removed == 1, "one expired message purged");
	CHECK(m.InboxFor("bob").size() == 2, "two messages remain");
	auto left = m.InboxFor("bob");
	bool oldGone = true;
	for (const auto* mm : left) if (mm->subject == "old") oldGone = false;
	CHECK(oldGone, "expired 'old' message removed");
	// legacy (created_utc==0) is immune even at a huge now.
	CHECK(m.PurgeExpired(999999999, 1) == 1, "recent 'new' now expires; legacy immune");
	auto after = m.InboxFor("bob");
	CHECK(after.size() == 1 && after[0]->subject == "legacy", "only timeless legacy mail survives");
}

// ---------------------------------------------------------------------------
// S5 (M14 S10): item-bearing mail is FAIL-CLOSED. Inventory integration does not
// exist yet, so a claim carrying a non-empty item_id must refuse and change
// NOTHING — the old code flipped claimed=true and dropped the item on the floor.
// Refusing while leaving claimed=false also keeps Delete() blocked, so the
// message stays reclaimable once inventory lands.
static void TestItemAttachmentFailClosed() {
	std::printf("[Mail] item-bearing claim is fail-closed (S5)\n");
	MailService m;
	std::vector<MailAttachment> att;
	att.push_back(MailAttachment{"weapon_stabba_pig", 1, 0});
	att.push_back(MailAttachment{"", 0, 750});
	CHECK(m.SendMailWithAttachments("system", "bob", "loot", "take it", att), "item mail sent");
	const int64_t id = m.messages.back().id;

	CHECK(m.HasItemAttachments(id), "item payload detected");
	CHECK(!m.HasItemAttachments(0), "missing id has no item payload");

	CHECK(m.ClaimAttachments(id).empty(), "item-bearing claim returns nothing");

	const MailMessage* msg = m.Find(id);
	CHECK(msg && !msg->claimed, "claimed stays false after refused item claim");
	CHECK(msg && !msg->read, "read stays false after refused item claim");
	CHECK(msg && msg->attachments.size() == 2, "attachments retained intact");
	CHECK(m.UnreadCount("bob") == 1, "still unread after refused claim");

	CHECK(m.HasUnclaimedAttachments(id), "still has unclaimed attachments");
	CHECK(!m.Delete(id), "refused item mail cannot be discarded");
	CHECK(m.ClaimAttachments(id).empty(), "repeat claim still refuses, still no mutation");
	CHECK(m.Find(id) && !m.Find(id)->claimed, "repeat refusal left claimed false");

	m.SendMail("system", "bob", "cash", "gz", 250);
	const int64_t cashId = m.messages.back().id;
	CHECK(!m.HasItemAttachments(cashId), "cash-only mail has no item payload");
	auto got = m.ClaimAttachments(cashId);
	CHECK(got.size() == 1 && got[0].cash == 250, "cash-only claim still succeeds");
	CHECK(m.Find(cashId) && m.Find(cashId)->claimed, "cash-only claim commits the flag");
}

// ---------------------------------------------------------------------------
// CommitClaimed is the flag-only half of the journaled claim: the UE bridge
// credits cash and persists a receipt BEFORE the mail flag is committed, so the
// flag flip has to be callable on its own.
static void TestCommitClaimedStep() {
	std::printf("[Mail] CommitClaimed is a separate committable step\n");
	MailService m;
	m.SendMail("system", "bob", "reward", "gz", 1000);
	const int64_t id = m.messages.back().id;

	CHECK(!m.CommitClaimed(999999), "commit on missing id fails");
	CHECK(m.HasUnclaimedAttachments(id), "unclaimed before commit");
	CHECK(m.CommitClaimed(id), "commit flips the flag");
	const MailMessage* msg = m.Find(id);
	CHECK(msg && msg->claimed, "claimed true after commit");
	CHECK(msg && msg->read, "read true after commit");
	CHECK(msg && msg->attachments.size() == 1, "attachment record retained for history");
	CHECK(!m.CommitClaimed(id), "second commit refuses (idempotency guard)");
	CHECK(m.Delete(id), "committed mail is deletable");
}

int main() {
	std::printf("=== APB Mail Tests (M14 attachment claim + delete) ===\n");
	TestSendInboxUnread();
	TestClaimSemantics();
	TestDeleteRules();
	TestExpiry();
	TestItemAttachmentFailClosed();
	TestCommitClaimedStep();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
