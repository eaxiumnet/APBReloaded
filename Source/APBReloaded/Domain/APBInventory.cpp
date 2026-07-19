#include "APBInventory.h"
namespace apb {
int32_t Inventory::Count(const std::string& item_id) const {
	int32_t n=0; for(auto& s: slots) if(s.item_id==item_id) n+=s.quantity; return n;
}
bool Inventory::Has(const std::string& item_id, int32_t qty) const { return Count(item_id)>=qty; }
bool Inventory::Grant(const std::string& item_id, int32_t qty) {
	if(qty<=0||item_id.empty()) return false;
	for(auto& s: slots) if(s.item_id==item_id){ s.quantity+=qty; return true; }
	if((int32_t)slots.size()>=max_slots) return false;
	slots.push_back({item_id,qty}); return true;
}
bool Inventory::Consume(const std::string& item_id, int32_t qty) {
	if(qty<=0||!Has(item_id,qty)) return false;
	int32_t remain=qty;
	for(auto& s: slots){ if(s.item_id!=item_id) continue; int32_t take=std::min(s.quantity,remain); s.quantity-=take; remain-=take; if(!remain) break; }
	slots.erase(std::remove_if(slots.begin(),slots.end(),[](const InventorySlot& s){return s.quantity<=0;}), slots.end());
	return true;
}
}
