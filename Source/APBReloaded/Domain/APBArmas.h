#pragma once
#include "APBCatalog.h"
#include "APBInventory.h"
namespace apb {
struct ArmasResult { bool ok=false; std::string error; int64_t g1c_spent=0; std::string item_id; };
class ArmasStore {
public:
	const Catalog* catalog=nullptr;
	ArmasResult Purchase(CharacterProfile& buyer, Inventory& inv, const std::string& item_id) const;
};
}
