// M2 restart-parity tests for the JSON persistence layer (APBPersistence).
// Instance A mutates state with a temp persist dir; instances B/C are fresh
// WorldService objects pointed at the same dir and must see identical state.
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include "../Source/APBReloaded/Domain/APBPersistence.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static std::string DataDir() {
	return R"(D:\APBReloaded\Content\Data)";
}

static bool FileExists(const std::string& path) {
	std::error_code ec;
	return std::filesystem::exists(path, ec);
}

static bool WriteText(const std::string& path, const std::string& text) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << text;
	return (bool)out;
}

static bool SeedPersistenceFixture(const std::string& dir) {
	JsonDomainStore store;
	if (!store.Init(dir)) return false;

	LoginService login;
	AccountRecord account;
	account.account_id = "ACC-fixture";
	account.username = "fixture";
	account.password_hash = "fixture_hash";
	account.password_salt = "fixture_salt";
	login.accounts[account.username] = account;

	CharacterProfile profile;
	profile.name = "FixtureChar";
	profile.faction = Faction::Enforcer;
	profile.cash = 12345;
	profile.g1c = 678;
	CharacterAppearance appearance;
	Inventory inventory;
	inventory.Grant("fixture_item", 2);

	AuctionHouse auction;
	AuctionListing listing;
	listing.listing_id = 41;
	listing.seller = profile.name;
	listing.item_id = "fixture_item";
	listing.quantity = 1;
	listing.buyout_price = 500;
	auction.listings.push_back(listing);
	auction.next_id = 42;

	MailService mail;
	mail.SendMail("FixtureChar", "Recipient", "Fixture subject", "Fixture body");

	return store.SaveAccounts(login)
		&& store.SaveCharacter("fixture", 0, profile, appearance, inventory, 7.5)
		&& store.SaveAuction(auction)
		&& store.SaveMail(mail);
}

template <typename T, typename = void>
struct HasCharacterWriteCount : std::false_type {};

template <typename T>
struct HasCharacterWriteCount<T,
	std::void_t<decltype(std::declval<const T&>().CharacterWriteCount())>> : std::true_type {};

template <typename T>
static size_t CharacterWriteCountOrZero(const T& store) {
	if constexpr (HasCharacterWriteCount<T>::value) return store.CharacterWriteCount();
	return 0;
}

int main() {
	std::cout << "APB Persistence tests (restart parity)\n";
	const std::string persistDir = (std::filesystem::temp_directory_path() / "apb_persist_test").string();
	{
		std::error_code ec;
		std::filesystem::remove_all(persistDir, ec); // clean start
	}

	std::string clothItem, armasItem, armasItem2;
	int64_t expCash = 0, expG1c = 0, expListing = 0, expMailId = 0;
	int32_t expArmasCount = 0;
	double expThreat = 12.5;
	std::string expBlob;

	// ---- Instance A: mutate and persist ------------------------------------
	{
		WorldService a;
		CHECK(a.InitFromDataDir(DataDir()), "A init catalog");
		CHECK(!a.PersistenceActive(), "persistence inactive before InitPersistence");
		CHECK(a.InitPersistence(persistDir), "A persistence init");
		CHECK(a.PersistenceActive(), "persistence active after InitPersistence");

		for (const auto& kv : a.catalog.items) {
			if (clothItem.empty() && kv.second.category == "Clothing") clothItem = kv.first;
			if (kv.second.armas_listed) {
				if (armasItem.empty()) armasItem = kv.first;
				else if (armasItem2.empty() && kv.first != armasItem) armasItem2 = kv.first;
			}
		}
		CHECK(!armasItem.empty() && !armasItem2.empty(), "two distinct armas items available");

		CHECK(a.RegisterAccount("persist_alice", "pw1"), "A register alice");
		CHECK(a.LoginAccount("persist_alice", "pw1"), "A login alice");
		CHECK(a.CreateCharacter("PAli", Faction::Criminal), "A create criminal char");
		if (!clothItem.empty())
			CHECK(a.EquipClothing("top", clothItem, 3, 4), "A equip clothing");
		CHECK(a.GrantItem(armasItem, 3), "A grant item x3");
		a.character->g1c = 20000; // ensure funds for the buy; flushed by SaveAllNow below
		CHECK(a.ArmasBuy(armasItem2).ok, "A armas buy (g1c spend)");
		const int32_t armasBeforeList = a.inventory.Count(armasItem);
		CHECK(a.AuctionList(armasItem, 1, 1234).ok, "A auction list");
		expListing = a.auction.listings.back().listing_id;
		a.character->cash = 7777;
		a.threat.points = expThreat;
		a.progress.AddContactStanding("Financial_C1", 2500);
		a.progress.AddRoleXp("role_shotgun", 900);
		a.SaveAllNow();
		CHECK(a.SendMail("PBob", "hello bob", "body text", 50), "A send mail to PBob");
		expMailId = a.mail.messages.back().id;

		expCash = a.character->cash;
		expG1c = a.character->g1c;
		expArmasCount = a.inventory.Count(armasItem);
		expBlob = a.SaveAppearanceBlob();
		CHECK(expArmasCount == armasBeforeList - 1, "A armas count decremented by listing");
		CHECK(expArmasCount >= 2, "A armas stock remains after listing");
		a.LogoutAccount();
		CHECK(!a.character.has_value(), "A character cleared on logout");

		CHECK(a.RegisterAccount("persist_bob", "pw2"), "A register bob");
		CHECK(a.LoginAccount("persist_bob", "pw2"), "A login bob");
		CHECK(a.CreateCharacter("PBob", Faction::Enforcer), "A create enforcer char");
		a.LogoutAccount();
	}

	// ---- Files on disk ------------------------------------------------------
	CHECK(FileExists(persistDir + "/accounts.json"), "accounts.json exists");
	CHECK(FileExists(persistDir + "/characters/persist_alice_0.json"), "alice character file exists");
	CHECK(FileExists(persistDir + "/characters/persist_bob_0.json"), "bob character file exists");
	CHECK(FileExists(persistDir + "/auction.json"), "auction.json exists");
	CHECK(FileExists(persistDir + "/mail.json"), "mail.json exists");

	// ---- Instance B: restart parity -----------------------------------------
	{
		WorldService b;
		CHECK(b.InitFromDataDir(DataDir()), "B init catalog");
		CHECK(b.InitPersistence(persistDir), "B persistence init");

		// mail store loaded at init (no login required)
		auto pbobInbox = b.mail.InboxFor("PBob");
		CHECK(pbobInbox.size() == 1, "B mail restored for PBob");
		CHECK(!pbobInbox.empty() && !pbobInbox[0]->read, "B mail unread before read-flag test");
		CHECK(!pbobInbox.empty() && pbobInbox[0]->subject == "hello bob", "B mail subject matches");
		CHECK(!pbobInbox.empty() && pbobInbox[0]->body == "body text", "B mail body matches");
		CHECK(!pbobInbox.empty() && pbobInbox[0]->attachments.size() == 1
			&& pbobInbox[0]->attachments[0].cash == 50, "B mail cash attachment matches");

		// Retail "Take All": claim the cash attachment; claimed state must persist.
		CHECK(!pbobInbox.empty() && b.mail.ClaimAttachments(pbobInbox[0]->id).size() == 1,
			"B claim mail cash attachment");
		CHECK(!b.mail.HasUnclaimedAttachments(expMailId), "B attachment claimed (no unclaimed)");

		// auction restored
		bool listingFound = false;
		for (const auto& L : b.auction.listings)
			if (L.listing_id == expListing && L.active && L.buyout_price == 1234 && L.item_id == armasItem)
				listingFound = true;
		CHECK(listingFound, "B auction listing restored active");

		// login restores the persisted character
		CHECK(b.LoginAccount("persist_alice", "pw1"), "B login alice (account persisted)");
		CHECK(b.character.has_value(), "B character auto-loaded on login");
		CHECK(b.character && b.character->name == "PAli", "B character name matches");
		CHECK(b.character && b.character->faction == Faction::Criminal, "B faction matches");
		CHECK(b.character && b.character->cash == expCash, "B cash matches");
		CHECK(b.character && b.character->g1c == expG1c, "B g1c matches");
		CHECK(b.inventory.Count(armasItem) == expArmasCount, "B inventory armas count matches");
		CHECK(b.inventory.Has(armasItem2, 1), "B inventory has armas-bought item");
		CHECK(std::fabs(b.threat.points - expThreat) < 1e-9, "B threat points match");
		CHECK(b.SaveAppearanceBlob() == expBlob, "B appearance blob matches");
		CHECK(b.progress.ContactStanding("Financial_C1") == 2500, "B progress contact standing restored");
		CHECK(b.progress.RoleXp("role_shotgun") == 900, "B progress role xp restored");
		CHECK(b.progress.ContactStanding("Unknown_C9") == 0, "B progress unknown contact defaults 0");

		CHECK(b.MarkMailRead(expMailId), "B mark mail read");
		b.LogoutAccount();
	}

	// ---- Instance C: read flag + second faction survive another restart -----
	{
		WorldService c;
		CHECK(c.InitFromDataDir(DataDir()), "C init catalog");
		CHECK(c.InitPersistence(persistDir), "C persistence init");
		CHECK(c.LoginAccount("persist_bob", "pw2"), "C login bob");
		CHECK(c.character && c.character->name == "PBob", "C bob character restored");
		CHECK(c.character && c.character->faction == Faction::Enforcer, "C enforcer faction persisted");
		auto inbox = c.MailInbox();
		CHECK(inbox.size() == 1 && inbox[0]->read, "C mail read flag persisted");
		CHECK(!inbox.empty() && inbox[0]->claimed, "C mail claimed flag persisted");
	}

	// ---- Instance D: no persistence — in-memory behavior unchanged ----------
	{
		WorldService d;
		CHECK(d.InitFromDataDir(DataDir()), "D init catalog");
		CHECK(!d.PersistenceActive(), "D persistence inactive (no InitPersistence)");
		CHECK(d.RegisterAccount("nopersist", "pw"), "D register in-memory");
		CHECK(d.LoginAccount("nopersist", "pw"), "D login in-memory");
		CHECK(d.CreateCharacter("NoPersist", Faction::Criminal), "D create char in-memory");
		CHECK(d.GrantItem(armasItem, 1), "D grant item in-memory");
		CHECK(d.SendMail("Someone", "s", "b"), "D send mail in-memory");
		d.SaveAllNow(); // must be a safe no-op without persistence
		CHECK(d.character && d.character->name == "NoPersist", "D state intact after SaveAllNow no-op");
	}

	// ---- Instance E: CharacterProgress sidecar persistence round-trip ------
	// M15 progression state (contact standing + role XP) must survive a restart the same
	// way accounts/characters/auction/mail do. Exercises JsonDomainStore directly.
	{
		JsonDomainStore store;
		CHECK(store.Init(persistDir), "E store init");
		CHECK(!store.HasProgress("persist_erin", 0), "E no progress file yet");

		CharacterProgress prog;
		prog.AddContactStanding("Financial_C1", 1500);
		prog.AddContactStanding("Waterfront_C2", 300);
		prog.AddRoleXp("role_shotgun", 4200);
		CHECK(store.SaveProgress("persist_erin", 0, prog), "E save progress");
		CHECK(store.HasProgress("persist_erin", 0), "E progress file created");

		CharacterProgress loaded;
		CHECK(store.LoadProgress("persist_erin", 0, loaded), "E load progress");
		CHECK(loaded.ContactStanding("Financial_C1") == 1500, "E contact standing restored");
		CHECK(loaded.ContactStanding("Waterfront_C2") == 300, "E second contact restored");
		CHECK(loaded.RoleXp("role_shotgun") == 4200, "E role xp restored");
		CHECK(loaded.ContactStanding("Nonexistent") == 0, "E unknown contact defaults 0");

		// tolerate-missing: an unwritten slot leaves the passed-in progress empty, returns false
		CharacterProgress empty;
		CHECK(!store.LoadProgress("persist_erin", 9, empty), "E missing progress -> false");
		CHECK(empty.contact_standing.empty() && empty.role_xp.empty(), "E missing leaves empty");
	}

	// ---- Instance F: reject corrupt files without harming sibling state -----
	{
		const std::string dir = persistDir + "/corrupt_accounts";
		CHECK(SeedPersistenceFixture(dir), "F accounts fixture seeded");
		CHECK(WriteText(dir + "/accounts.json", "{\"accounts\":[{\"id\":\"ACC-corrupt\""),
			"F accounts corrupt file written");
		JsonDomainStore store;
		CHECK(store.Init(dir), "F accounts fresh store init");
		LoginService login;
		AccountRecord sentinel;
		sentinel.username = "sentinel";
		login.accounts[sentinel.username] = sentinel;
		CHECK(!store.LoadAccounts(login), "F accounts corrupt load rejected");
		CHECK(login.accounts.size() == 1 && login.accounts.count("sentinel") == 1,
			"F accounts corrupt load leaves destination untouched");
		AuctionHouse auction;
		MailService mail;
		CharacterProfile profile;
		CharacterAppearance appearance;
		Inventory inventory;
		double threat = 0;
		CHECK(store.LoadAuction(auction) && auction.listings.size() == 1,
			"F accounts valid auction sibling loads");
		CHECK(store.LoadMail(mail) && mail.messages.size() == 1,
			"F accounts valid mail sibling loads");
		CHECK(store.LoadCharacter("fixture", 0, profile, appearance, inventory, threat)
			&& profile.name == "FixtureChar", "F accounts valid character sibling loads");
	}
	{
		const std::string dir = persistDir + "/corrupt_character";
		CHECK(SeedPersistenceFixture(dir), "F character fixture seeded");
		CHECK(WriteText(dir + "/characters/fixture_0.json",
			"{\"name\":\"Partial\",\"faction\":\"Criminal\",\"inventory\":[{\"item_id\":\"x\""),
			"F character corrupt file written");
		JsonDomainStore store;
		CHECK(store.Init(dir), "F character fresh store init");
		CharacterProfile profile;
		profile.name = "SentinelChar";
		CharacterAppearance appearance;
		appearance.body.height = 1.25f;
		Inventory inventory;
		inventory.Grant("sentinel_item", 3);
		double threat = 99.0;
		CHECK(!store.LoadCharacter("fixture", 0, profile, appearance, inventory, threat),
			"F character corrupt load rejected");
		CHECK(profile.name == "SentinelChar" && appearance.body.height == 1.25f
			&& inventory.Count("sentinel_item") == 3 && threat == 99.0,
			"F character corrupt load leaves destination untouched");
		LoginService login;
		AuctionHouse auction;
		MailService mail;
		CHECK(store.LoadAccounts(login) && login.accounts.count("fixture") == 1,
			"F character valid accounts sibling loads");
		CHECK(store.LoadAuction(auction) && auction.listings.size() == 1,
			"F character valid auction sibling loads");
		CHECK(store.LoadMail(mail) && mail.messages.size() == 1,
			"F character valid mail sibling loads");
	}
	{
		const std::string dir = persistDir + "/corrupt_auction";
		CHECK(SeedPersistenceFixture(dir), "F auction fixture seeded");
		CHECK(WriteText(dir + "/auction.json",
			"{\"next_id\":72,\"listings\":[{\"id\":7,\"item_id\":\"partial\""),
			"F auction corrupt file written");
		JsonDomainStore store;
		CHECK(store.Init(dir), "F auction fresh store init");
		AuctionHouse auction;
		AuctionListing sentinel;
		sentinel.listing_id = 900;
		sentinel.item_id = "sentinel_item";
		auction.listings.push_back(sentinel);
		auction.next_id = 901;
		CHECK(!store.LoadAuction(auction), "F auction corrupt load rejected");
		CHECK(auction.listings.size() == 1 && auction.listings[0].listing_id == 900
			&& auction.next_id == 901, "F auction corrupt load leaves destination untouched");
		LoginService login;
		MailService mail;
		CharacterProfile profile;
		CharacterAppearance appearance;
		Inventory inventory;
		double threat = 0;
		CHECK(store.LoadAccounts(login) && login.accounts.count("fixture") == 1,
			"F auction valid accounts sibling loads");
		CHECK(store.LoadMail(mail) && mail.messages.size() == 1,
			"F auction valid mail sibling loads");
		CHECK(store.LoadCharacter("fixture", 0, profile, appearance, inventory, threat)
			&& profile.name == "FixtureChar", "F auction valid character sibling loads");
	}
	{
		const std::string dir = persistDir + "/corrupt_mail";
		CHECK(SeedPersistenceFixture(dir), "F mail fixture seeded");
		CHECK(WriteText(dir + "/mail.json",
			"{\"next_id\":82,\"messages\":[{\"id\":8,\"to_char\":\"Partial\""),
			"F mail corrupt file written");
		JsonDomainStore store;
		CHECK(store.Init(dir), "F mail fresh store init");
		MailService mail;
		mail.SendMail("SentinelFrom", "SentinelTo", "SentinelSubject", "SentinelBody");
		const int64_t sentinelNextId = mail.next_mail_id;
		CHECK(!store.LoadMail(mail), "F mail corrupt load rejected");
		CHECK(mail.messages.size() == 1 && mail.messages[0].subject == "SentinelSubject"
			&& mail.next_mail_id == sentinelNextId,
			"F mail corrupt load leaves destination untouched");
		LoginService login;
		AuctionHouse auction;
		CharacterProfile profile;
		CharacterAppearance appearance;
		Inventory inventory;
		double threat = 0;
		CHECK(store.LoadAccounts(login) && login.accounts.count("fixture") == 1,
			"F mail valid accounts sibling loads");
		CHECK(store.LoadAuction(auction) && auction.listings.size() == 1,
			"F mail valid auction sibling loads");
		CHECK(store.LoadCharacter("fixture", 0, profile, appearance, inventory, threat)
			&& profile.name == "FixtureChar", "F mail valid character sibling loads");
	}

	// ---- Instance G: interrupted final file is atomically replaced -----------
	{
		const std::string dir = persistDir + "/atomic_replace";
		JsonDomainStore store;
		CHECK(store.Init(dir), "G store init");
		const std::string finalPath = dir + "/accounts.json";
		const std::string tempPath = finalPath + ".tmp";
		CHECK(WriteText(finalPath, "{\"accounts\":[{\"id\":\"interrupted\""),
			"G interrupted final file written");
		CHECK(WriteText(tempPath, "stale temp from prior crash"), "G stale temp file written");

		LoginService source;
		AccountRecord coherent;
		coherent.account_id = "ACC-coherent";
		coherent.username = "coherent";
		coherent.password_hash = "coherent_hash";
		coherent.password_salt = "coherent_salt";
		source.accounts[coherent.username] = coherent;
		CHECK(store.SaveAccounts(source), "G save replaces interrupted final file");

		LoginService loaded;
		CHECK(store.LoadAccounts(loaded), "G load after atomic replacement");
		CHECK(loaded.accounts.size() == 1 && loaded.accounts.count("coherent") == 1
			&& loaded.accounts.at("coherent").account_id == "ACC-coherent",
			"G replacement record is coherent");
		CHECK(!FileExists(tempPath), "G atomic save removes stale temp file");
	}

	// ---- Instance H: next IDs continue beyond pre-restart maxima ------------
	{
		const std::string dir = persistDir + "/next_id_restart";
		JsonDomainStore firstStore;
		CHECK(firstStore.Init(dir), "H first store init");
		AuctionHouse firstAuction;
		AuctionListing oldListing;
		oldListing.listing_id = 41;
		oldListing.seller = "OldSeller";
		oldListing.item_id = "restart_item";
		firstAuction.listings.push_back(oldListing);
		firstAuction.next_id = 42;
		MailService firstMail;
		CHECK(firstMail.SendMail("OldSender", "OldRecipient", "OldSubject", "OldBody"),
			"H pre-restart mail created");
		const int64_t highestAuctionId = firstAuction.listings.back().listing_id;
		const int64_t highestMailId = firstMail.messages.back().id;
		CHECK(firstStore.SaveAuction(firstAuction), "H pre-restart auction saved");
		CHECK(firstStore.SaveMail(firstMail), "H pre-restart mail saved");

		JsonDomainStore restartedStore;
		CHECK(restartedStore.Init(dir), "H restarted store init");
		AuctionHouse restartedAuction;
		MailService restartedMail;
		CHECK(restartedStore.LoadAuction(restartedAuction), "H restarted auction loaded");
		CHECK(restartedStore.LoadMail(restartedMail), "H restarted mail loaded");
		Catalog catalog;
		ItemDef restartItem;
		restartItem.id = "restart_item";
		catalog.items[restartItem.id] = restartItem;
		restartedAuction.catalog = &catalog;
		Inventory sellerInventory;
		sellerInventory.Grant("restart_item", 1);
		CharacterProfile seller;
		seller.name = "NewSeller";
		AuctionResult listed = restartedAuction.ListItem(
			"NewSeller", sellerInventory, seller, "restart_item", 1, 250);
		CHECK(listed.ok, "H post-restart auction created");
		CHECK(listed.listing_id > highestAuctionId,
			"H post-restart auction id exceeds pre-restart maximum");
		CHECK(restartedMail.SendMail("NewSender", "NewRecipient", "NewSubject", "NewBody"),
			"H post-restart mail created");
		CHECK(restartedMail.messages.back().id > highestMailId,
			"H post-restart mail id exceeds pre-restart maximum");
	}

	// ---- Instance I: logout writes one character snapshot -------------------
	{
		const std::string dir = persistDir + "/logout_write_once";
		WorldService service;
		CHECK(service.InitFromDataDir(DataDir()), "I init catalog");
		CHECK(service.InitPersistence(dir), "I persistence init");
		CHECK(service.RegisterAccount("write_once", "pw"), "I register account");
		CHECK(service.LoginAccount("write_once", "pw"), "I login account");
		CHECK(service.CreateCharacter("WriteOnce", Faction::Criminal), "I create character");
		CHECK(HasCharacterWriteCount<JsonDomainStore>::value,
			"I character physical-write counter is observable");
		const size_t beforeLogout = CharacterWriteCountOrZero(service.store);
		service.LogoutAccount();
		const size_t afterLogout = CharacterWriteCountOrZero(service.store);
		CHECK(afterLogout == beforeLogout + 1,
			"I one LogoutAccount triggers exactly one character snapshot write");
	}

	// ---- Instance J: general persistence does not own social state -----------
	{
		const std::string dir = persistDir + "/social_authority";
		WorldService playerService;
		CHECK(playerService.InitPersistence(dir), "J player persistence init");
		CHECK(!playerService.social_store.IsActive(),
			"J player persistence leaves social store inactive");

		WorldService authority;
		CHECK(authority.InitSocialPersistence(dir), "J social authority persistence init");
		CHECK(authority.social_store.Dir() == dir + "/social",
			"J social authority appends social directory");
		CHECK(authority.clans.CreateClan("C1", "Authority", "AUTH", Faction::Enforcer, "alice")
			== ClanResult::Ok, "J authority creates clan");
		CHECK(authority.friends_svc.SendRequest("alice", "bob") == FriendResult::Ok,
			"J authority sends friend request");
		CHECK(authority.friends_svc.AcceptRequest("bob", "alice") == FriendResult::Ok,
			"J authority accepts friend request");
		authority.SaveSocialNow();
		CHECK(FileExists(dir + "/social/clans.json"), "J authority writes clans.json");
		CHECK(FileExists(dir + "/social/friends.json"), "J authority writes friends.json");

		WorldService restartedAuthority;
		CHECK(restartedAuthority.InitSocialPersistence(dir), "J restarted social authority init");
		CHECK(restartedAuthority.clans.InClan("alice"), "J restarted authority loads clans");
		CHECK(restartedAuthority.friends_svc.AreFriends("alice", "bob"),
			"J restarted authority loads friends");
	}

	// ---- Instance K: social authority owns mail persistence -----------------
	// S7 restart parity: mail sent through the social seam survives a world restart.
	// S10: the claimed flag survives restart so attachments cannot be double-granted,
	// and an item-bearing message stays fail-closed (claimed=false) across restart so
	// it remains reclaimable once inventory granting exists.
	{
		const std::string dir = persistDir + "/social_mail";

		// K-1: fresh-start tolerance — InitSocialPersistence on empty dir is safe.
		{
			WorldService fresh;
			CHECK(fresh.InitFromDataDir(DataDir()), "K fresh-start catalog init");
			CHECK(fresh.InitSocialPersistence(dir), "K fresh-start social persistence init");
			CHECK(fresh.mail.messages.empty(), "K fresh-start yields zero messages");
		}

		// K-2: send mail with cash + item attachment through the social authority,
		//      mark one read, claim another, then SaveSocialNow.
		int64_t kMailCashId = 0, kMailItemId = 0, kMailClaimedId = 0;
		int64_t kNextIdAfterSave = 0;
		{
			WorldService authority;
			CHECK(authority.InitFromDataDir(DataDir()), "K authority catalog init");
			CHECK(authority.InitSocialPersistence(dir), "K authority social persistence init");

			// Send a cash-only mail and an item-attachment mail.
			MailAttachment cashAtt;
			cashAtt.item_id = "";
			cashAtt.count   = 0;
			cashAtt.cash    = 500;
			authority.mail.SendMailWithAttachments(
				"System", "KPlayer", "You have cash", "Take it", {cashAtt}, 1700000000LL);
			kMailCashId = authority.mail.messages.back().id;

			MailAttachment itemAtt;
			itemAtt.item_id = "weapon_123";
			itemAtt.count   = 1;
			itemAtt.cash    = 0;
			authority.mail.SendMailWithAttachments(
				"System", "KPlayer", "You have an item", "Take it", {itemAtt}, 1700000001LL);
			kMailItemId = authority.mail.messages.back().id;

			MailAttachment claimedAtt;
			claimedAtt.item_id = "";
			claimedAtt.count   = 0;
			claimedAtt.cash    = 750;
			authority.mail.SendMailWithAttachments(
				"System", "KPlayer", "You have more cash", "Take it", {claimedAtt}, 1700000002LL);
			kMailClaimedId = authority.mail.messages.back().id;

			// Mark the cash mail read; claim the cash-only mail; the item mail is
			// refused fail-closed because inventory granting does not exist yet.
			CHECK(authority.mail.MarkRead(kMailCashId), "K mark cash mail read");
			auto claimed = authority.mail.ClaimAttachments(kMailClaimedId);
			CHECK(claimed.size() == 1 && claimed[0].cash == 750,
				"K cash-only attachment claimed");
			auto refused = authority.mail.ClaimAttachments(kMailItemId);
			CHECK(refused.empty(), "K item attachment claim refused fail-closed (S10)");

			kNextIdAfterSave = authority.mail.next_mail_id;
			authority.SaveSocialNow();
			CHECK(FileExists(dir + "/social/mail.json"), "K mail.json written to social dir");
		}

		// K-3: fresh WorldService with InitSocialPersistence on same dir sees
		//      identical messages including all flags and next_mail_id.
		{
			WorldService restarted;
			CHECK(restarted.InitFromDataDir(DataDir()), "K restarted catalog init");
			CHECK(restarted.InitSocialPersistence(dir), "K restarted social persistence init");

			// next_mail_id must not collide with existing ids.
			CHECK(restarted.mail.next_mail_id >= kNextIdAfterSave,
				"K next_mail_id after restart does not collide");

			// Cash mail: read flag preserved.
			const MailMessage* cashMsg = restarted.mail.Find(kMailCashId);
			CHECK(cashMsg != nullptr, "K cash mail restored after restart");
			CHECK(cashMsg && cashMsg->read,        "K cash mail read flag preserved");
			CHECK(cashMsg && !cashMsg->claimed,    "K cash mail claimed=false preserved");
			CHECK(cashMsg && cashMsg->created_utc == 1700000000LL,
				"K cash mail created_utc preserved");
			CHECK(cashMsg && cashMsg->attachments.size() == 1
				&& cashMsg->attachments[0].cash == 500,
				"K cash mail attachment cash preserved");

			// Claimed cash mail: claimed + read flags survive restart, and the
			// double-grant guard still refuses a second claim.
			const MailMessage* claimedMsg = restarted.mail.Find(kMailClaimedId);
			CHECK(claimedMsg != nullptr,             "K claimed mail restored after restart");
			CHECK(claimedMsg && claimedMsg->claimed, "K claimed mail claimed flag preserved (S10)");
			CHECK(claimedMsg && claimedMsg->read,    "K claimed mail read flag preserved");
			CHECK(claimedMsg && claimedMsg->created_utc == 1700000002LL,
				"K claimed mail created_utc preserved");
			auto reclaim = restarted.mail.ClaimAttachments(kMailClaimedId);
			CHECK(reclaim.empty(), "K claimed attachment cannot be re-claimed after restart (S10)");

			// Item mail: fail-closed state survives restart, so it stays undeletable
			// and reclaimable rather than having silently destroyed the item.
			const MailMessage* itemMsg = restarted.mail.Find(kMailItemId);
			CHECK(itemMsg != nullptr,             "K item mail restored after restart");
			CHECK(itemMsg && !itemMsg->claimed,   "K item mail claimed=false preserved (S10 fail-closed)");
			CHECK(itemMsg && !itemMsg->read,      "K item mail read=false preserved");
			CHECK(itemMsg && itemMsg->created_utc == 1700000001LL,
				"K item mail created_utc preserved");
			CHECK(itemMsg && itemMsg->attachments.size() == 1
				&& itemMsg->attachments[0].item_id == "weapon_123",
				"K item mail attachment item_id preserved");
			CHECK(restarted.mail.HasItemAttachments(kMailItemId),
				"K item payload still detected after restart");
			CHECK(!restarted.mail.Delete(kMailItemId),
				"K fail-closed item mail still undeletable after restart (S10)");
		}

		// K-4: isolation invariant — InitPersistence alone does NOT activate social_store.
		{
			WorldService playerService;
			CHECK(playerService.InitPersistence(dir), "K player InitPersistence succeeds");
			CHECK(!playerService.social_store.IsActive(),
				"K InitPersistence leaves social_store inactive (J invariant preserved)");
		}
	}

	{
		std::error_code ec;
		std::filesystem::remove_all(persistDir, ec);
	}
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
