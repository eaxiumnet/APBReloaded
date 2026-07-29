// run_auction_tests.cpp — M12 (D7): marketplace bid/buyout/expiry/fee/cancel tests.
// Links APBAuction.cpp + APBInventory.cpp + APBCatalog.cpp (MailService is header-only).
#include "APBAuction.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

// Build a catalog holding one sellable item.
static Catalog MakeCatalog() {
	Catalog c;
	ItemDef d; d.id = "test_gun"; d.name = "Test Gun"; d.category = "weapon";
	c.items[d.id] = d;
	return c;
}

// Count cash attachments delivered to a character across their inbox.
static int64_t MailCashTo(const MailService& m, const std::string& who) {
	int64_t total = 0;
	for (auto* msg : m.InboxFor(who))
		for (const auto& a : msg->attachments) total += a.cash;
	return total;
}
static int64_t MailItemCountTo(const MailService& m, const std::string& who, const std::string& item) {
	int64_t n = 0;
	for (auto* msg : m.InboxFor(who))
		for (const auto& a : msg->attachments) if (a.item_id == item) n += a.count;
	return n;
}

static AuctionHouse MakeHouse(Catalog& cat, MailService& mail) {
	AuctionHouse h; h.catalog = &cat; h.mail = &mail; return h;
}

static void TestBuyoutFee() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sellerInv; sellerInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller"; seller.cash = 0;
	CharacterProfile buyer;  buyer.name = "Buyer";  buyer.cash = 5000;
	Inventory buyerInv;
	auto lr = h.ListItem("Seller", sellerInv, seller, "test_gun", 1, 1000);
	CHECK(lr.ok, "list buyout-only ok");
	CHECK(sellerInv.Count("test_gun") == 0, "item escrowed out of seller inventory on list");
	auto br = h.Buyout("Buyer", buyer, buyerInv, seller, sellerInv, lr.listing_id);
	CHECK(br.ok, "buyout ok");
	CHECK(buyer.cash == 4000, "buyer charged full buyout price");
	CHECK(seller.cash == 950, "seller receives price minus 5% fee (1000 - 50)");
	CHECK(buyerInv.Count("test_gun") == 1, "buyer receives the item");
	CHECK(h.Find(lr.listing_id)->state == AuctionState::Sold, "listing marked sold");
	CHECK(h.Find(lr.listing_id)->fee_paid == 50, "fee_paid recorded");
}

static void TestBidRejections() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sInv; sInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller";
	auto lr = h.ListItemEx("Seller", sInv, seller, "test_gun", 1, /*buyout*/2000, /*start*/500, /*expires*/1000, /*now*/0);
	CHECK(lr.ok, "list bid+buyout ok");
	CharacterProfile s2 = seller; // same name
	CHECK(!h.PlaceBid("Seller", s2, lr.listing_id, 600, 0).ok, "cannot bid on own listing");
	CharacterProfile bidder; bidder.name = "Bidder"; bidder.cash = 10000;
	CHECK(!h.PlaceBid("Bidder", bidder, lr.listing_id, 400, 0).ok, "bid below start_price rejected");
	CHECK(!h.PlaceBid("Bidder", bidder, lr.listing_id, 2000, 0).ok, "bid >= buyout rejected (use buyout)");
	CHECK(bidder.cash == 10000, "rejected bids do not escrow funds");
	CHECK(!h.PlaceBid("Bidder", bidder, lr.listing_id, 600, 1000).ok, "bid at/after expiry rejected");
}

static void TestBidEscrowAndOutbidRefund() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sInv; sInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller";
	auto lr = h.ListItemEx("Seller", sInv, seller, "test_gun", 1, /*buyout*/0, /*start*/500, /*expires*/1000, /*now*/0);
	CHECK(lr.ok, "list bid-only ok");
	CharacterProfile b1; b1.name = "B1"; b1.cash = 3000;
	CharacterProfile b2; b2.name = "B2"; b2.cash = 3000;
	CHECK(h.PlaceBid("B1", b1, lr.listing_id, 600, 10).ok, "first bid accepted");
	CHECK(b1.cash == 2400, "first bidder escrowed 600");
	CHECK(h.PlaceBid("B2", b2, lr.listing_id, 800, 20).ok, "higher bid accepted");
	CHECK(b2.cash == 2200, "second bidder escrowed 800");
	CHECK(MailCashTo(mail, "B1") == 600, "outbid first bidder refunded 600 via mail");
	CHECK(h.Find(lr.listing_id)->high_bidder == "B2" && h.Find(lr.listing_id)->current_bid == 800,
	      "high bid updated to B2/800");
	CHECK(!h.PlaceBid("B1", b1, lr.listing_id, 700, 30).ok, "bid below current high rejected");
}

static void TestSettleSoldViaMail() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sInv; sInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller";
	auto lr = h.ListItemEx("Seller", sInv, seller, "test_gun", 1, 0, 500, /*expires*/1000, 0);
	CharacterProfile bidder; bidder.name = "Winner"; bidder.cash = 5000;
	CHECK(h.PlaceBid("Winner", bidder, lr.listing_id, 1000, 10).ok, "winning bid placed");
	CHECK(h.SettleExpired(500) == 0, "not settled before expiry");
	CHECK(h.SettleExpired(1000) == 1, "settled at expiry");
	CHECK(MailItemCountTo(mail, "Winner", "test_gun") == 1, "winner receives item via mail");
	CHECK(MailCashTo(mail, "Seller") == 950, "seller receives winning bid minus 5% fee (1000-50)");
	CHECK(h.Find(lr.listing_id)->state == AuctionState::Sold, "listing sold on settle");
	CHECK(h.SettleExpired(2000) == 0, "already-settled listing not re-settled");
}

static void TestSettleExpiredNoBidsReturnsItem() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sInv; sInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller";
	auto lr = h.ListItemEx("Seller", sInv, seller, "test_gun", 1, 0, 500, /*expires*/1000, 0);
	CHECK(h.SettleExpired(1500) == 1, "unsold listing settles at expiry");
	CHECK(MailItemCountTo(mail, "Seller", "test_gun") == 1, "unsold item returned to seller via mail");
	CHECK(h.Find(lr.listing_id)->state == AuctionState::Expired, "listing marked expired");
}

static void TestCancelRefundsAndReturns() {
	Catalog cat = MakeCatalog(); MailService mail;
	AuctionHouse h = MakeHouse(cat, mail);
	Inventory sInv; sInv.Grant("test_gun", 1);
	CharacterProfile seller; seller.name = "Seller";
	auto lr = h.ListItemEx("Seller", sInv, seller, "test_gun", 1, 0, 500, /*expires*/0, 0);
	CharacterProfile bidder; bidder.name = "Bidder"; bidder.cash = 3000;
	CHECK(h.PlaceBid("Bidder", bidder, lr.listing_id, 700, 0).ok, "bid placed before cancel");
	CHECK(!h.Cancel("NotSeller", lr.listing_id).ok, "only the seller can cancel");
	CHECK(h.Cancel("Seller", lr.listing_id).ok, "seller cancels listing");
	CHECK(MailCashTo(mail, "Bidder") == 700, "standing bidder refunded on cancel");
	CHECK(MailItemCountTo(mail, "Seller", "test_gun") == 1, "item returned to seller on cancel");
	CHECK(h.Find(lr.listing_id)->state == AuctionState::Cancelled, "listing marked cancelled");
	CHECK(!h.Buyout("Buyer", bidder, sInv, seller, sInv, lr.listing_id).ok, "cancelled listing cannot be bought");
}

int main() {
	std::printf("=== APB Auction Tests (M12 marketplace bid/buyout/expiry/fee/cancel) ===\n");
	TestBuyoutFee();
	TestBidRejections();
	TestBidEscrowAndOutbidRefund();
	TestSettleSoldViaMail();
	TestSettleExpiredNoBidsReturnsItem();
	TestCancelRefundsAndReturns();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
