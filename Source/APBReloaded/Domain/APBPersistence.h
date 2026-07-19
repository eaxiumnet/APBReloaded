#pragma once
#include "APBSocial.h"
#include "APBCustomization.h"
#include "APBInventory.h"
#include "APBAuction.h"

namespace apb {

/** Current UTC time as ISO-8601 (e.g. 2026-07-19T12:34:56Z). Pure stdlib. */
std::string NowUtcIso();

/** JSON string escaping for the hand-rolled Domain writer.
 *  Escapes '"' and '\\'; control chars (< 0x20) are replaced with a space so the
 *  existing JsonGetString reader round-trips every printable character exactly. */
std::string JsonEscape(const std::string& s);

/** File-backed JSON persistence (D5 / ARCHITECTURE.md §4), adapted to the actual
 *  Domain types. Pure C++17 stdlib, no Unreal headers.
 *
 *  Layout under the configured dir:
 *    accounts.json                     — LoginService account records
 *    characters\<account>_<slot>.json  — character profile + appearance + inventory
 *    auction.json                      — AuctionHouse listings
 *    mail.json                         — MailService messages
 *
 *  Passwords stay plaintext (private offline port; hashing is M16, documented).
 *  All load functions tolerate missing files (fresh start = empty state). */
class JsonDomainStore {
public:
	/** Creates <dir> and <dir>\characters if needed; activates the store. */
	bool Init(const std::string& dir);
	bool IsActive() const { return active_; }
	const std::string& Dir() const { return dir_; }

	bool LoadAccounts(LoginService& login) const;
	bool SaveAccounts(const LoginService& login) const;

	std::string CharacterPath(const std::string& account, int32_t slot) const;
	bool HasCharacter(const std::string& account, int32_t slot) const;
	bool LoadCharacter(const std::string& account, int32_t slot,
		CharacterProfile& profile, CharacterAppearance& appearance,
		Inventory& inventory, double& threat_points) const;
	bool SaveCharacter(const std::string& account, int32_t slot,
		const CharacterProfile& profile, const CharacterAppearance& appearance,
		const Inventory& inventory, double threat_points) const;

	bool LoadAuction(AuctionHouse& auction) const;
	bool SaveAuction(const AuctionHouse& auction) const;

	bool LoadMail(MailService& mail) const;
	bool SaveMail(const MailService& mail) const;

private:
	std::string dir_;
	bool active_ = false;
};

} // namespace apb
