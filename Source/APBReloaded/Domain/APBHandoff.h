#pragma once
#include "APBWorldService.h"
#include <string>

namespace apb {

struct CharacterHandoff {
	std::string account;
	std::string character;
	std::string faction;
	std::string jti;
	std::string nonce;
	int64_t sent_ms = 0;
	DomainSnapshot snapshot;
};

std::string SerializeSnapshot(const DomainSnapshot& snapshot);
bool DeserializeSnapshot(const std::string& json, DomainSnapshot& out);
std::string SignHandoff(const CharacterHandoff& handoff, const std::string& secret_hex);
bool VerifyHandoff(const std::string& signed_handoff, const std::string& secret_hex,
	CharacterHandoff& out);

}
