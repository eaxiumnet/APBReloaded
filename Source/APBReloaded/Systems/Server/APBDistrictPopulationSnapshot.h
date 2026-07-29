#pragma once

#include "APBDistrictDirectory.h"

#include <cstdint>
#include <string>
#include <vector>

struct FAPBDistrictPopulationSnapshot
{
	std::string DistrictId;
	int32_t InstanceCount = 0;
	int32_t Population = 0;
};

inline std::vector<FAPBDistrictPopulationSnapshot> MakeDistrictPopulationSnapshots(
	const std::vector<apb::DistrictPopulation>& Aggregates)
{
	std::vector<FAPBDistrictPopulationSnapshot> Snapshots;
	Snapshots.reserve(Aggregates.size());
	for (const apb::DistrictPopulation& Aggregate : Aggregates)
	{
		Snapshots.push_back({Aggregate.district, Aggregate.instances, Aggregate.total_players});
	}
	return Snapshots;
}
