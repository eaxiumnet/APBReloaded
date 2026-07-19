#pragma once
#include "APBTypes.h"
namespace apb {
struct InventorySlot { std::string item_id; int32_t quantity=0; };
class Inventory {
public:
	std::vector<InventorySlot> slots; int32_t max_slots=64;
	int32_t Count(const std::string& item_id) const;
	bool Grant(const std::string& item_id, int32_t qty);
	bool Consume(const std::string& item_id, int32_t qty);
	bool Has(const std::string& item_id, int32_t qty=1) const;
};
}
