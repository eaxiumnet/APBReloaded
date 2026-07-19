// M2 restart-parity tests for the JSON persistence layer (APBPersistence).
// Instance A mutates state with a temp persist dir; instances B/C are fresh
// WorldService objects pointed at the same dir and must see identical state.
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

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

	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
