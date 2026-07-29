#pragma once
// APBAuction.h — M12 (D7): marketplace / auction house.
// Retail model: a seller lists an item with an optional instant buyout price and/or an
// opening bid price + expiry. Buyers bid (funds escrowed by the house, previous high
// bidder refunded) or buy out instantly. The house charges a 5% marketplace fee on the
// sale proceeds. Item + cash settlement is delivered through the mail service (retail
// delivers marketplace results to the mailbox, so an offline counterparty still receives
// them). Pure C++17 — no UE/platform deps.
#include "APBCatalog.h"
#include "APBInventory.h"
#include "APBSocial.h"   // MailService — settlement is delivered via mail (header-only)
namespace apb {

// Lifecycle of a listing (§4 auction schema: active|sold|expired|cancelled).
enum class AuctionState { Active, Sold, Expired, Cancelled };

struct AuctionListing {
	int64_t listing_id = 0;
	std::string seller, item_id;
	int32_t quantity = 1;
	int64_t start_price = 0;   // minimum opening bid (0 = buyout-only listing)
	int64_t buyout_price = 0;  // instant purchase price (0 = bid-only listing)
	int64_t current_bid = 0;   // highest live bid amount (escrowed by the house)
	std::string high_bidder;   // character holding the current high bid ("" = no bids)
	int64_t created_utc = 0;
	int64_t expires_utc = 0;   // 0 = never expires
	int64_t fee_paid = 0;      // marketplace fee taken on settlement (0 until sold)
	AuctionState state = AuctionState::Active;
	bool active = true;        // kept for persistence back-compat (active == state==Active)
};

struct AuctionResult { bool ok=false; std::string error; int64_t listing_id=0; };

class AuctionHouse {
public:
	const Catalog* catalog = nullptr;
	MailService* mail = nullptr;      // when set, item/cash settlement is delivered via mail
	std::vector<AuctionListing> listings;
	int64_t next_id = 1;
	int32_t fee_bps = 500;            // 5% marketplace fee (basis points)

	// 5% (fee_bps) of a sale price, floored; never negative.
	int64_t FeeFor(int64_t price) const { return price <= 0 ? 0 : price * fee_bps / 10000; }

	// --- listing ---
	// Buyout-only listing (legacy signature, preserved for existing callers/tests).
	AuctionResult ListItem(const std::string& seller, Inventory& inv, CharacterProfile& seller_profile,
		const std::string& item_id, int32_t qty, int64_t buyout);
	// Full listing: optional opening bid (start_price) and/or buyout, with an expiry.
	AuctionResult ListItemEx(const std::string& seller, Inventory& inv, CharacterProfile& seller_profile,
		const std::string& item_id, int32_t qty, int64_t buyout, int64_t start_price,
		int64_t expires_utc, int64_t now_utc);

	// --- purchase paths ---
	// Instant buyout: item to buyer, proceeds (minus 5% fee) to seller. Kept signature.
	AuctionResult Buyout(const std::string& buyer_name, CharacterProfile& buyer, Inventory& buyer_inv,
		CharacterProfile& seller, Inventory& seller_inv, int64_t listing_id);
	// Place a bid: funds escrowed from the bidder now; any previous high bidder is refunded
	// their escrow via mail. Bid must beat the current bid, meet the opening price, and stay
	// below any buyout price. Rejects the seller's own listing and expired listings.
	AuctionResult PlaceBid(const std::string& bidder_name, CharacterProfile& bidder,
		int64_t listing_id, int64_t amount, int64_t now_utc);

	// --- lifecycle ---
	// Seller cancels an own active listing. Any standing high bid is refunded via mail and
	// the item is returned to the seller via mail. Requires a mail service.
	AuctionResult Cancel(const std::string& seller_name, int64_t listing_id);
	// Settle every active listing whose expiry has passed. A listing with a high bidder is
	// SOLD (item -> bidder, proceeds minus fee -> seller, both via mail); a listing with no
	// bids is EXPIRED (item returned to seller via mail). Requires a mail service. Returns
	// the number of listings settled.
	int32_t SettleExpired(int64_t now_utc);

	AuctionListing* Find(int64_t listing_id);
};

} // namespace apb
