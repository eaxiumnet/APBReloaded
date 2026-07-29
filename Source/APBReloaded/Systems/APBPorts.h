#pragma once
// APBPorts.h — M7 N1: single source of truth for APB server port allocation.
// Pure constexpr C++17, no UE/platform headers, so world/district/relay code and
// build tools all share identical numbers. Mirrors [APBServer] in
// Config/DefaultGame.ini (keep the two in sync; the ini lets ops override).
//
// Allocation (resolves the BLOCKER in work/m7_spec.md §3):
//   World game / NetDriver (UDP)   : 17778  (M6 contract — unchanged)
//   W<->D control relay (TCP/JSON) : 17800  (NEW; must NOT reuse 17778)
//   District game / NetDriver base : 17810  (per-district = base + numeric_id)
//
// District ports derive from the STABLE apbdb numeric_id in
// Content/Data/districts.json rather than a positional index, so the mapping is
// order-independent and 1:1 with the live game's district ids:
//   Financial(1)      -> 17811    Social(9)        -> 17819
//   FinancialChaos(2) -> 17812    Waterfront(11)   -> 17821
//   PGAsylum(4)       -> 17814    FinancialRiot(12)-> 17822
//   PGBeacon(5)       -> 17815
//   PGCrate(6)        -> 17816
// (17810 itself is reserved — there is no district numeric_id 0.)
//
#include <cstdint>

namespace apb { namespace ports {

inline constexpr int32_t World        = 17778; // world authority NetDriver (UDP)
inline constexpr int32_t Relay        = 17800; // world<->district control channel (TCP/JSON)
inline constexpr int32_t DistrictBase = 17810; // district NetDriver base; add numeric_id

// Resolve a district's game/NetDriver port from its stable apbdb numeric_id.
// Returns 0 for an invalid id (<= 0) so callers can detect a misconfiguration
// instead of silently colliding with the reserved base port.
inline constexpr int32_t DistrictPort(int32_t numeric_id)
{
	return numeric_id > 0 ? (DistrictBase + numeric_id) : 0;
}

}} // namespace apb::ports
