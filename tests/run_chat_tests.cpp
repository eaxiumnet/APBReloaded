// run_chat_tests.cpp — M7 N5 domain suite for apb::ChatService.
// Compile (see tests\build_and_run.ps1):
//   cl /nologo /EHsc /std:c++17 /I"...\Domain" run_chat_tests.cpp ...\APBChat.cpp
#include "../Source/APBReloaded/Domain/APBChat.h"
#include <iostream>
#include <string>
#include <algorithm>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static bool HasDelivery(const SubmitResult& r, const std::string& who) {
	return std::any_of(r.deliveries.begin(), r.deliveries.end(),
		[&](const ChatDelivery& d){ return d.recipient == who; });
}
static const ChatDelivery* Find(const SubmitResult& r, const std::string& who) {
	for (const auto& d : r.deliveries) if (d.recipient == who) return &d;
	return nullptr;
}

// --- Test 1: District broadcast reaches everyone on the roster --------------
void TestDistrictBroadcast() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob"); c.AddPlayer("carol");
	auto r = c.Submit("alice", ChatChannel::District, "", "hello district", 1000);
	CHECK(r.status == ChatResult::Delivered, "T1: district delivered");
	CHECK(r.deliveries.size() == 3, "T1: reaches all 3 players");
	CHECK(HasDelivery(r, "bob") && HasDelivery(r, "carol") && HasDelivery(r, "alice"), "T1: everyone incl sender");
}

// --- Test 2: Whisper only to target + echo to sender ------------------------
void TestWhisperRouting() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob"); c.AddPlayer("carol");
	auto r = c.Submit("alice", ChatChannel::Whisper, "bob", "hi bob", 1000);
	CHECK(r.status == ChatResult::Delivered, "T2: whisper delivered");
	CHECK(HasDelivery(r, "bob"), "T2: target gets it");
	CHECK(HasDelivery(r, "alice"), "T2: sender gets echo");
	CHECK(!HasDelivery(r, "carol"), "T2: bystander does NOT");

	auto off = c.Submit("alice", ChatChannel::Whisper, "nobody", "hi", 1000);
	CHECK(off.status == ChatResult::RecipientOffline, "T2: offline target rejected");

	c.SetPresence("carol", PresenceStatus::DoNotDisturb);
	auto dnd = c.Submit("alice", ChatChannel::Whisper, "carol", "busy?", 1000);
	CHECK(dnd.status == ChatResult::RecipientBusy, "T2: DND target rejected");
}

// --- Test 3: Ignore list blocks a sender's messages -------------------------
void TestIgnoreList() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob");
	c.AddIgnore("bob", "Alice"); // case-insensitive
	auto r = c.Submit("alice", ChatChannel::District, "", "hey", 1000);
	CHECK(r.status == ChatResult::Delivered, "T3: sender still succeeds");
	CHECK(!HasDelivery(r, "bob"), "T3: ignorer does not receive");
	CHECK(HasDelivery(r, "alice"), "T3: others (self) still receive");

	auto w = c.Submit("alice", ChatChannel::Whisper, "bob", "psst", 1001);
	CHECK(w.status == ChatResult::Delivered, "T3: whisper reports delivered");
	CHECK(!HasDelivery(w, "bob"), "T3: ignored whisper dropped for target");
	CHECK(HasDelivery(w, "alice"), "T3: sender still sees own whisper echo");
}

// --- Test 4: Profanity filter masks per recipient preference ----------------
void TestProfanityFilter() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob"); c.AddPlayer("carol");
	c.AddProfanity("badword");
	c.SetProfanityFilter("carol", false); // carol opts out
	auto r = c.Submit("alice", ChatChannel::District, "", "you badword here", 1000);
	const ChatDelivery* toBob = Find(r, "bob");
	const ChatDelivery* toCarol = Find(r, "carol");
	CHECK(toBob && toBob->message.body == "you ******* here", "T4: filtered for bob");
	CHECK(toCarol && toCarol->message.body == "you badword here", "T4: raw for carol (opted out)");
}

// --- Test 5: Flood control mutes after the limit within the window ----------
void TestFloodControl() {
	ChatService c;
	c.flood_limit = 3;
	c.flood_window_ms = 1000;
	c.AddPlayer("spammer"); c.AddPlayer("bob");
	CHECK(c.Submit("spammer", ChatChannel::District, "", "1", 100).status == ChatResult::Delivered, "T5: msg1 ok");
	CHECK(c.Submit("spammer", ChatChannel::District, "", "2", 200).status == ChatResult::Delivered, "T5: msg2 ok");
	CHECK(c.Submit("spammer", ChatChannel::District, "", "3", 300).status == ChatResult::Delivered, "T5: msg3 ok");
	CHECK(c.Submit("spammer", ChatChannel::District, "", "4", 400).status == ChatResult::Muted, "T5: msg4 muted");
	// After the window slides, sending is allowed again.
	CHECK(c.Submit("spammer", ChatChannel::District, "", "5", 1500).status == ChatResult::Delivered, "T5: recovers after window");
}

// --- Test 6: Group scope only reaches same-group members --------------------
void TestGroupScope() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob"); c.AddPlayer("carol");
	c.SetGroup("alice", "G1"); c.SetGroup("bob", "G1"); c.SetGroup("carol", "G2");
	auto r = c.Submit("alice", ChatChannel::Group, "", "team up", 1000);
	CHECK(r.status == ChatResult::Delivered, "T6: group delivered");
	CHECK(HasDelivery(r, "alice") && HasDelivery(r, "bob"), "T6: same-group members receive");
	CHECK(!HasDelivery(r, "carol"), "T6: other group excluded");

	c.SetGroup("dave", "G1"); // no such player -> no-op
	auto none = c.Submit("carol", ChatChannel::Faction, "", "faction?", 1001);
	CHECK(none.status == ChatResult::BadChannel, "T6: no faction set -> BadChannel");
}

// --- Test 7: Slash command parsing ------------------------------------------
void TestSlashParsing() {
	auto w = ChatService::ParseCommand("/w Bob hello there", ChatChannel::District);
	CHECK(w.ok && w.channel == ChatChannel::Whisper && w.target == "Bob" && w.body == "hello there", "T7: /w parses target+body");
	auto d = ChatService::ParseCommand("/d spread out", ChatChannel::Local);
	CHECK(d.ok && d.channel == ChatChannel::District && d.body == "spread out", "T7: /d district");
	auto g = ChatService::ParseCommand("/g on me", ChatChannel::District);
	CHECK(g.ok && g.channel == ChatChannel::Group && g.body == "on me", "T7: /g group");
	auto plain = ChatService::ParseCommand("just talking", ChatChannel::District);
	CHECK(plain.ok && plain.channel == ChatChannel::District && plain.body == "just talking", "T7: plain uses default");
	auto bad = ChatService::ParseCommand("/w Bob", ChatChannel::District);
	CHECK(!bad.ok, "T7: /w with no message is malformed");
	auto empty = ChatService::ParseCommand("   ", ChatChannel::District);
	CHECK(!empty.ok, "T7: blank line not ok");
}

// --- Test 8: SubmitRaw end-to-end + System broadcast ------------------------
void TestSubmitRawAndSystem() {
	ChatService c;
	c.AddPlayer("alice"); c.AddPlayer("bob");
	auto r = c.SubmitRaw("alice", "/w bob yo", ChatChannel::District, 1000);
	CHECK(r.status == ChatResult::Delivered && HasDelivery(r, "bob"), "T8: SubmitRaw whisper routes");

	c.AddIgnore("bob", "alice");
	auto sys = c.SystemBroadcast("Server restarting in 5 minutes", 2000);
	CHECK(sys.status == ChatResult::Delivered, "T8: system broadcast delivered");
	CHECK(HasDelivery(sys, "alice") && HasDelivery(sys, "bob"), "T8: system bypasses ignore -> reaches all");
}

int main() {
	std::cout << "APB Chat tests (M7 N5)\n";
	TestDistrictBroadcast();
	TestWhisperRouting();
	TestIgnoreList();
	TestProfanityFilter();
	TestFloodControl();
	TestGroupScope();
	TestSlashParsing();
	TestSubmitRawAndSystem();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
