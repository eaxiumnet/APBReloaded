// run_social_store_tests.cpp — M14 (D10): filesystem round-trip for SocialStore.
// Verifies the on-disk clans.json/friends.json wrappers over the pure
// ClanService/FriendsService SaveJson()/LoadJson() serializers.
//
// Links: APBSocialStore.cpp + APBClan.cpp + APBFriends.cpp (self-contained; does
// NOT touch the shared $srcs list). Follows the CHECK/FAILS= harness convention.
#include "APBSocialStore.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("  FAIL: %s\n", msg); } } while (0)

static bool Has(const std::vector<std::string>& v, const std::string& s) {
	for (const auto& e : v) if (e == s) return true;
	return false;
}

// A fresh, unique scratch dir under the OS temp path, removed on scope exit.
struct ScratchDir {
	std::filesystem::path path;
	ScratchDir() {
		auto base = std::filesystem::temp_directory_path();
		for (int i = 0; ; ++i) {
			auto cand = base / ("apb_socialstore_test_" + std::to_string(i));
			std::error_code ec;
			if (!std::filesystem::exists(cand, ec)) { path = cand; break; }
		}
	}
	~ScratchDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
	std::string str() const { return path.generic_string(); }
};

// ---------------------------------------------------------------------------
static void TestInitAndPaths() {
	std::printf("[SocialStore] init + path derivation\n");
	SocialStore empty;
	CHECK(!empty.Init(""), "Init(\"\") must fail");
	CHECK(!empty.IsActive(), "empty store inactive after failed init");
	CHECK(!empty.SaveClans(ClanService{}), "SaveClans on inactive store -> false");
	{ ClanService c; CHECK(!empty.LoadClans(c), "LoadClans on inactive store -> false"); }

	ScratchDir sd;
	SocialStore store;
	CHECK(store.Init(sd.str()), "Init(<tmp>) succeeds");
	CHECK(store.IsActive(), "store active after Init");
	CHECK(std::filesystem::exists(sd.path), "Init created the directory");
	CHECK(store.ClansPath() == sd.str() + "/clans.json", "ClansPath layout");
	CHECK(store.FriendsPath() == sd.str() + "/friends.json", "FriendsPath layout");
}

// ---------------------------------------------------------------------------
static void TestMissingFileTolerance() {
	std::printf("[SocialStore] missing-file tolerance (fresh start)\n");
	ScratchDir sd;
	SocialStore store;
	store.Init(sd.str());
	ClanService clans;
	FriendsService friends;
	// No file written yet: Load must return false and not crash / not populate.
	CHECK(!store.LoadClans(clans), "LoadClans missing -> false");
	CHECK(!store.LoadFriends(friends), "LoadFriends missing -> false");
	CHECK(!clans.InClan("nobody"), "clans untouched by failed load");
	CHECK(friends.FriendCount("nobody") == 0, "friends untouched by failed load");
}

// ---------------------------------------------------------------------------
static void TestClansRoundTrip() {
	std::printf("[SocialStore] clans.json save -> load round-trip\n");
	ScratchDir sd;
	SocialStore store;
	store.Init(sd.str());

	ClanService src;
	CHECK(src.CreateClan("c1", "Praetorians", "PRAE", Faction::Enforcer, "alice") == ClanResult::Ok, "create clan");
	CHECK(src.Invite("alice", "bob", Faction::Enforcer) == ClanResult::Ok, "invite bob");
	CHECK(src.AcceptInvite("bob") == ClanResult::Ok, "bob accepts");
	CHECK(src.SetMotd("alice", "hold the line") == ClanResult::Ok, "set motd");

	CHECK(store.SaveClans(src), "SaveClans writes file");
	CHECK(std::filesystem::exists(store.ClansPath()), "clans.json exists on disk");

	ClanService dst;
	CHECK(store.LoadClans(dst), "LoadClans reads file");
	CHECK(dst.InClan("alice") && dst.InClan("bob"), "members restored");
	CHECK(dst.ClanOf("alice") == "c1", "clan id restored");
	CHECK(dst.IsLeader("alice"), "leadership restored");
	CHECK(dst.Size("c1") == 2, "member count restored");
	CHECK(dst.MotdOf("c1") == "hold the line", "motd restored");
	const Clan* c = dst.Find("c1");
	CHECK(c && c->faction == Faction::Enforcer, "faction restored");
	CHECK(c && c->name == "Praetorians" && c->tag == "PRAE", "name/tag restored");
}

// ---------------------------------------------------------------------------
static void TestFriendsRoundTrip() {
	std::printf("[SocialStore] friends.json save -> load round-trip\n");
	ScratchDir sd;
	SocialStore store;
	store.Init(sd.str());

	FriendsService src;
	CHECK(src.SendRequest("alice", "bob") == FriendResult::Ok, "alice->bob request");
	CHECK(src.AcceptRequest("bob", "alice") == FriendResult::Ok, "bob accepts");
	CHECK(src.Ignore("alice", "carol") == FriendResult::Ok, "alice ignores carol");
	src.SetOnline("bob", true); // transient — must NOT persist

	CHECK(store.SaveFriends(src), "SaveFriends writes file");
	CHECK(std::filesystem::exists(store.FriendsPath()), "friends.json exists on disk");

	FriendsService dst;
	CHECK(store.LoadFriends(dst), "LoadFriends reads file");
	CHECK(dst.AreFriends("alice", "bob"), "friendship restored (a->b)");
	CHECK(dst.AreFriends("bob", "alice"), "friendship restored symmetric (b->a)");
	CHECK(dst.IsIgnoring("alice", "carol"), "ignore restored");
	CHECK(!dst.IsOnline("bob"), "presence is transient — NOT persisted");
	CHECK(Has(dst.FriendsOf("alice"), "bob"), "FriendsOf restored");
}

// ---------------------------------------------------------------------------
static void TestOverwriteAndIdempotency() {
	std::printf("[SocialStore] overwrite existing file + idempotent bytes\n");
	ScratchDir sd;
	SocialStore store;
	store.Init(sd.str());

	ClanService a;
	a.CreateClan("cx", "First", "FST", Faction::Criminal, "zed");
	CHECK(store.SaveClans(a), "first save");
	{
		std::ofstream stale(store.ClansPath() + ".tmp", std::ios::binary | std::ios::trunc);
		stale << "stale temp from prior crash";
	}

	// Overwrite with different content.
	ClanService b;
	b.CreateClan("cy", "Second", "SND", Faction::Enforcer, "yan");
	CHECK(store.SaveClans(b), "overwrite save");

	ClanService loaded;
	CHECK(store.LoadClans(loaded), "load after overwrite");
	CHECK(loaded.Find("cy") != nullptr, "overwritten clan present");
	CHECK(loaded.Find("cx") == nullptr, "old clan gone (truncated write)");
	CHECK(!std::filesystem::exists(store.ClansPath() + ".tmp"), "overwrite removes stale temp file");

	// save -> load -> save produces identical bytes.
	ClanService rt;
	store.LoadClans(rt);
	std::string once = rt.SaveJson();
	ClanService rt2;
	rt2.LoadJson(once);
	CHECK(rt2.SaveJson() == once, "clans SaveJson idempotent through store");
}

int main() {
	std::printf("=== APB SocialStore Tests (M14 file round-trip) ===\n");
	TestInitAndPaths();
	TestMissingFileTolerance();
	TestClansRoundTrip();
	TestFriendsRoundTrip();
	TestOverwriteAndIdempotency();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
