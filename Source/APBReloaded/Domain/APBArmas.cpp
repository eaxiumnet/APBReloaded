#include "APBArmas.h"
namespace apb {
ArmasResult ArmasStore::Purchase(CharacterProfile& buyer, Inventory& inv, const std::string& item_id) const {
	ArmasResult r; r.item_id=item_id;
	if(!catalog){ r.error="no_catalog"; return r; }
	const ItemDef* def=catalog->FindItem(item_id);
	if(!def||!def->armas_listed){ r.error="item_not_in_armas"; return r; }
	if(buyer.g1c < def->armas_price){ r.error="insufficient_g1c"; return r; }
	buyer.g1c -= def->armas_price;
	if(!inv.Grant(item_id,1)){ buyer.g1c += def->armas_price; r.error="inventory_full"; return r; }
	r.ok=true; r.g1c_spent=def->armas_price; return r;
}
}
