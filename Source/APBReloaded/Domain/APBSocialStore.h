#pragma once
// APBSocialStore.h — M14 (D10): file-backed persistence for the social Domain
// services (clans + friends/ignore + mail). Thin filesystem wrapper that delegates
// clans/friends serialization to the services' own pure SaveJson()/LoadJson()
// round-trip, and mail serialization to JsonDomainStore (APBPersistence) so there
// is exactly one mail serialization implementation in the codebase.
//
// Deliberately SEPARATE from JsonDomainStore (APBPersistence): keeping this in its
// own translation unit means the shared $srcs list (APBPersistence.cpp, linked into
// 4 test suites) is untouched — no re-link ripple, merge-friendly for parallel agents.
//
// Layout under the configured dir:
//   <dir>/clans.json    — ClanService state (clans, ranks, members, next_join_seq)
//   <dir>/friends.json  — FriendsService durable state (friendships + ignore lists)
//   <dir>/mail.json     — MailService messages (next_id, per-message flags + attachments)
//
// All Load* functions tolerate a missing/empty file (fresh start = empty state,
// returns false so callers can distinguish "nothing loaded" from a populated load).
#include "APBClan.h"
#include "APBFriends.h"
#include "APBPersistence.h"
#include <string>

namespace apb {

class SocialStore {
public:
	// Creates <dir> if needed and activates the store. Empty dir -> inactive/false.
	bool Init(const std::string& dir);
	bool IsActive() const { return active_; }
	const std::string& Dir() const { return dir_; }

	std::string ClansPath() const;   // <dir>/clans.json
	std::string FriendsPath() const; // <dir>/friends.json
	std::string MailPath() const;    // <dir>/mail.json

	// Clans: write/read the whole ClanService via its JSON round-trip.
	bool SaveClans(const ClanService& clans) const;
	bool LoadClans(ClanService& clans) const;   // false if file missing/empty/no clans

	// Friends + ignore lists.
	bool SaveFriends(const FriendsService& friends) const;
	bool LoadFriends(FriendsService& friends) const; // false if file missing/empty

	// Mail: delegates to JsonDomainStore (single serialization implementation).
	// false if file missing/empty; preserves next_mail_id and per-message flags.
	bool SaveMail(const MailService& mail) const;
	bool LoadMail(MailService& mail) const;

private:
	std::string dir_;
	bool active_ = false;
};

} // namespace apb
