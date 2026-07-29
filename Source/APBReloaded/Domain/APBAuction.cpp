// APBAuction.cpp — M12 (D7): marketplace / auction house implementation.
// Escrow-on-bid, 5% marketplace fee, expiry settlement, mail delivery of item/cash.
#include "APBAuction.h"
namespace apb {

// --- settlement helpers (mail delivery of item / cash) -----------------------
namespace {
	void MailItem(MailService* mail, const std::string& to, const std::string& subject,
		const std::string& item_id, int32_t count) {
		if (!mail || to.empty() || item_id.empty() || count <= 0) return;
		std::vector<MailAttachment> att{ MailAttachment{ item_id, count, 0 } };
		mail->SendMailWithAttachments("Marketplace", to, subject, "", att);
	}
	void MailCash(MailService* mail, const std::string& to, const std::string& subject, int64_t cash) {
		if (!mail || to.empty() || cash <= 0) return;
		mail->SendMail("Marketplace", to, subject, "", cash);
	}
}

AuctionListing* AuctionHouse::Find(int64_t listing_id) {
	for (auto& x : listings) if (x.listing_id == listing_id) return &x;
	return nullptr;
}

// --- listing -----------------------------------------------------------------

AuctionResult AuctionHouse::ListItemEx(const std::string& seller, Inventory& inv, CharacterProfile&,
	const std::string& item_id, int32_t qty, int64_t buyout, int64_t start_price,
	int64_t expires_utc, int64_t now_utc) {
	AuctionResult r;
	if (!catalog || !catalog->FindItem(item_id)) { r.error = "unknown_item"; return r; }
	if (qty <= 0) { r.error = "invalid_params"; return r; }
	// A listing must be sellable: either an instant buyout price, an opening bid, or both.
	if (buyout <= 0 && start_price <= 0) { r.error = "invalid_params"; return r; }
	if (buyout > 0 && start_price > 0 && start_price > buyout) { r.error = "invalid_params"; return r; }
	if (!inv.Consume(item_id, qty)) { r.error = "missing_item"; return r; }
	AuctionListing L;
	L.listing_id = next_id++;
	L.seller = seller; L.item_id = item_id; L.quantity = qty;
	L.start_price = start_price < 0 ? 0 : start_price;
	L.buyout_price = buyout < 0 ? 0 : buyout;
	L.created_utc = now_utc;
	L.expires_utc = expires_utc < 0 ? 0 : expires_utc;
	L.state = AuctionState::Active; L.active = true;
	listings.push_back(L);
	r.ok = true; r.listing_id = L.listing_id;
	return r;
}

AuctionResult AuctionHouse::ListItem(const std::string& seller, Inventory& inv, CharacterProfile& seller_profile,
	const std::string& item_id, int32_t qty, int64_t buyout) {
	// Legacy buyout-only, never-expiring listing.
	if (buyout <= 0) { AuctionResult r; r.error = "invalid_params"; return r; }
	return ListItemEx(seller, inv, seller_profile, item_id, qty, buyout, /*start*/0, /*expires*/0, /*now*/0);
}

// --- purchase paths ----------------------------------------------------------

AuctionResult AuctionHouse::Buyout(const std::string& buyer_name, CharacterProfile& buyer, Inventory& buyer_inv,
	CharacterProfile& seller, Inventory&, int64_t listing_id) {
	AuctionResult r; r.listing_id = listing_id;
	AuctionListing* L = Find(listing_id);
	if (!L || !L->active || L->state != AuctionState::Active) { r.error = "listing_not_found"; return r; }
	if (L->buyout_price <= 0) { r.error = "no_buyout"; return r; }
	if (L->seller == buyer_name) { r.error = "cannot_buy_own"; return r; }
	if (buyer.cash < L->buyout_price) { r.error = "insufficient_funds"; return r; }
	if (!buyer_inv.Grant(L->item_id, L->quantity)) { r.error = "inventory_full"; return r; }
	const int64_t price = L->buyout_price;
	const int64_t fee = FeeFor(price);
	buyer.cash -= price;
	seller.cash += price - fee;
	L->fee_paid = fee;
	// A standing bidder loses the auction to the buyout: refund their escrow via mail.
	if (!L->high_bidder.empty() && L->current_bid > 0)
		MailCash(mail, L->high_bidder, "Auction: outbid by buyout", L->current_bid);
	L->high_bidder.clear(); L->current_bid = 0;
	L->state = AuctionState::Sold; L->active = false;
	r.ok = true;
	return r;
}

AuctionResult AuctionHouse::PlaceBid(const std::string& bidder_name, CharacterProfile& bidder,
	int64_t listing_id, int64_t amount, int64_t now_utc) {
	AuctionResult r; r.listing_id = listing_id;
	AuctionListing* L = Find(listing_id);
	if (!L || !L->active || L->state != AuctionState::Active) { r.error = "listing_not_found"; return r; }
	if (L->expires_utc > 0 && now_utc >= L->expires_utc) { r.error = "listing_expired"; return r; }
	if (L->seller == bidder_name) { r.error = "cannot_bid_own"; return r; }
	if (amount <= 0) { r.error = "bid_too_low"; return r; }
	if (L->start_price > 0 && amount < L->start_price) { r.error = "bid_too_low"; return r; }
	if (amount <= L->current_bid) { r.error = "bid_too_low"; return r; }
	if (L->buyout_price > 0 && amount >= L->buyout_price) { r.error = "bid_exceeds_buyout"; return r; }
	if (bidder.cash < amount) { r.error = "insufficient_funds"; return r; }
	// Escrow the new bid; refund the previous high bidder their held escrow via mail.
	if (!L->high_bidder.empty() && L->current_bid > 0)
		MailCash(mail, L->high_bidder, "Auction: you were outbid", L->current_bid);
	bidder.cash -= amount;
	L->high_bidder = bidder_name;
	L->current_bid = amount;
	r.ok = true;
	return r;
}

// --- lifecycle ---------------------------------------------------------------

AuctionResult AuctionHouse::Cancel(const std::string& seller_name, int64_t listing_id) {
	AuctionResult r; r.listing_id = listing_id;
	AuctionListing* L = Find(listing_id);
	if (!L || !L->active || L->state != AuctionState::Active) { r.error = "listing_not_found"; return r; }
	if (L->seller != seller_name) { r.error = "not_seller"; return r; }
	if (!mail) { r.error = "no_mail_service"; return r; }
	// Refund a standing bidder's escrow, then return the unsold item to the seller.
	if (!L->high_bidder.empty() && L->current_bid > 0)
		MailCash(mail, L->high_bidder, "Auction: listing cancelled, bid refunded", L->current_bid);
	MailItem(mail, L->seller, "Auction: listing cancelled, item returned", L->item_id, L->quantity);
	L->high_bidder.clear(); L->current_bid = 0;
	L->state = AuctionState::Cancelled; L->active = false;
	r.ok = true;
	return r;
}

int32_t AuctionHouse::SettleExpired(int64_t now_utc) {
	if (!mail) return 0;
	int32_t settled = 0;
	for (auto& L : listings) {
		if (!L.active || L.state != AuctionState::Active) continue;
		if (L.expires_utc <= 0 || now_utc < L.expires_utc) continue;
		if (!L.high_bidder.empty() && L.current_bid > 0) {
			// Sold to the high bidder: item to bidder, proceeds minus fee to seller.
			const int64_t fee = FeeFor(L.current_bid);
			L.fee_paid = fee;
			MailItem(mail, L.high_bidder, "Auction: you won the auction", L.item_id, L.quantity);
			MailCash(mail, L.seller, "Auction: your item sold", L.current_bid - fee);
			L.state = AuctionState::Sold;
		} else {
			// No bids: return the item to the seller.
			MailItem(mail, L.seller, "Auction: listing expired, item returned", L.item_id, L.quantity);
			L.state = AuctionState::Expired;
		}
		L.active = false;
		++settled;
	}
	return settled;
}

} // namespace apb
