#pragma once
#include "APBCatalog.h"
#include "APBInventory.h"
namespace apb {
struct AuctionListing {
	int64_t listing_id=0; std::string seller, item_id; int32_t quantity=1; int64_t buyout_price=0; bool active=true;
};
struct AuctionResult { bool ok=false; std::string error; int64_t listing_id=0; };
class AuctionHouse {
public:
	const Catalog* catalog=nullptr;
	std::vector<AuctionListing> listings;
	int64_t next_id=1;
	AuctionResult ListItem(const std::string& seller, Inventory& inv, CharacterProfile& seller_profile,
		const std::string& item_id, int32_t qty, int64_t buyout);
	AuctionResult Buyout(const std::string& buyer_name, CharacterProfile& buyer, Inventory& buyer_inv,
		CharacterProfile& seller, Inventory& seller_inv, int64_t listing_id);
};
}
