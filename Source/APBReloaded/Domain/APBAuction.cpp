#include "APBAuction.h"
namespace apb {
AuctionResult AuctionHouse::ListItem(const std::string& seller, Inventory& inv, CharacterProfile&,
	const std::string& item_id, int32_t qty, int64_t buyout) {
	AuctionResult r;
	if(!catalog||!catalog->FindItem(item_id)){ r.error="unknown_item"; return r; }
	if(qty<=0||buyout<=0){ r.error="invalid_params"; return r; }
	if(!inv.Consume(item_id, qty)){ r.error="missing_item"; return r; }
	AuctionListing L; L.listing_id=next_id++; L.seller=seller; L.item_id=item_id; L.quantity=qty; L.buyout_price=buyout; L.active=true;
	listings.push_back(L); r.ok=true; r.listing_id=L.listing_id; return r;
}
AuctionResult AuctionHouse::Buyout(const std::string& buyer_name, CharacterProfile& buyer, Inventory& buyer_inv,
	CharacterProfile& seller, Inventory&, int64_t listing_id) {
	AuctionResult r; r.listing_id=listing_id;
	AuctionListing* L=nullptr; for(auto& x: listings) if(x.listing_id==listing_id){ L=&x; break; }
	if(!L||!L->active){ r.error="listing_not_found"; return r; }
	if(L->seller==buyer_name){ r.error="cannot_buy_own"; return r; }
	if(buyer.cash < L->buyout_price){ r.error="insufficient_funds"; return r; }
	buyer.cash -= L->buyout_price; seller.cash += L->buyout_price;
	if(!buyer_inv.Grant(L->item_id, L->quantity)){
		buyer.cash += L->buyout_price; seller.cash -= L->buyout_price; r.error="inventory_full"; return r;
	}
	L->active=false; r.ok=true; return r;
}
}
