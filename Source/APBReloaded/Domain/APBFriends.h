#pragma once
// APBFriends.h — M14 (D10): pure-C++17 friends + ignore (block) domain.
// No platform/UE headers — unit-testable in isolation like Clan/Group/Chat.
//
// Grounded 1:1 on retail APB strings (APBUserInterface.int):
//   - MUTUAL, invite-based friendship: "accepted_friend_invite" /
//     "declined_friend_invite" / "<character>_declined_your_friend_invite";
//     confirmation "<character>_added_to_friend_list".
//   - caps: "Your_friends_list_is_full", "<character>_has_a_full_friend_list".
//   - no self ("You_can_not_add_yourself_to_your_friends_list"),
//     no duplicate invite ("You_already_invited_<character>"),
//     already friends ("<character>_is_already_in_your_friends_list").
//   - removal is MUTUAL ("<character>_removed_you_from_friend_list").
//   - IGNORE list (a.k.a. block): "<character>_added_to_ignore_list", caps
//     ("Your_ignore_list_is_full"), no self, already-ignored.
//   - Two mutual-exclusion invariants:
//       * cannot friend someone you ignore  ("You_are_currently_ignoring_<character>. …
//         remove them from your Ignore list first")   -> YouIgnoreTarget
//       * cannot ignore an existing friend  ("You_are_currently_friends_with_<character>. …
//         remove them from your friends list first")   -> TargetIsFriend
//       * a target ignoring the sender cannot receive the invite
//         ("<character>_could_not_receive_your_friend_invite")  -> TargetIgnoresYou
//
// Presence (online/offline) is transient session state fed by the M7 W<->D relay;
// it is NOT persisted. Friendships + ignore lists ARE persisted (friends.json).
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace apb {

enum class FriendResult {
	Ok,
	InvalidArg,        // empty player/target
	SelfTarget,        // cannot friend/ignore yourself
	AlreadyFriends,    // already on each other's friends list
	AlreadyInvited,    // duplicate outstanding request from->to
	FriendsListFull,   // sender's friends list at cap
	TargetListFull,    // recipient's friends list at cap
	TargetIgnoresYou,  // recipient is ignoring the sender -> invite not deliverable
	YouIgnoreTarget,   // sender is ignoring the target -> must un-ignore first
	NoSuchInvite,      // accept/decline with no matching pending request
	NotFriends,        // remove-friend when not friends
	AlreadyIgnored,    // target already on ignore list
	IgnoreListFull,    // ignore list at cap
	TargetIsFriend,    // cannot ignore an existing friend (unfriend first)
	NotIgnored         // un-ignore when target isn't ignored
};

class FriendsService {
public:
	int32_t max_friends = 50; // retail-scale caps (configurable)
	int32_t max_ignores = 50;

	// --- friend request / accept flow (mutual) --------------------------------
	// Sends a friend request from `from` to `to`. If `to` already has an
	// outstanding request to `from`, the two are befriended immediately (auto-accept).
	FriendResult SendRequest(const std::string& from, const std::string& to);
	FriendResult AcceptRequest(const std::string& invitee, const std::string& inviter);
	FriendResult DeclineRequest(const std::string& invitee, const std::string& inviter);
	// Mutual removal — both sides lose the friendship.
	FriendResult RemoveFriend(const std::string& player, const std::string& other);

	// --- ignore (block) list --------------------------------------------------
	// Adds `target` to `player`'s ignore list. Fails if they are currently friends
	// (TargetIsFriend). Cancels any pending requests between the two.
	FriendResult Ignore(const std::string& player, const std::string& target);
	FriendResult Unignore(const std::string& player, const std::string& target);

	// --- presence (transient; not persisted) ----------------------------------
	void SetOnline(const std::string& player, bool online);

	// --- queries (const) ------------------------------------------------------
	bool AreFriends(const std::string& a, const std::string& b) const;
	bool IsIgnoring(const std::string& player, const std::string& target) const;
	bool IsOnline(const std::string& player) const;
	int32_t FriendCount(const std::string& player) const;
	std::vector<std::string> FriendsOf(const std::string& player) const;        // sorted
	std::vector<std::string> OnlineFriendsOf(const std::string& player) const;  // sorted
	std::vector<std::string> IgnoredBy(const std::string& player) const;        // sorted
	std::vector<std::string> IncomingRequests(const std::string& invitee) const; // inviters
	std::vector<std::string> OutgoingRequests(const std::string& inviter) const; // invitees
	bool HasIncoming(const std::string& invitee, const std::string& inviter) const;

	// --- persistence (pure string round-trip) ---------------------------------
	// Serializes DURABLE state only: friendships + ignore lists. Pending requests and
	// online presence are transient and are NOT persisted. File IO lives in the store layer.
	std::string SaveJson() const;
	bool LoadJson(const std::string& text);

private:
	// friendships are mutual — stored in both directions for O(1) lookup.
	std::unordered_map<std::string, std::set<std::string>> friends_;
	// ignore is directional — `player` -> set of players they ignore.
	std::unordered_map<std::string, std::set<std::string>> ignores_;
	// pending requests: invitee -> set of inviters.
	std::unordered_map<std::string, std::set<std::string>> requests_;
	std::unordered_set<std::string> online_;

	bool FriendsFull(const std::string& player) const;
	void ClearRequestPair(const std::string& a, const std::string& b);
	static const std::set<std::string>* Get(
		const std::unordered_map<std::string, std::set<std::string>>& m, const std::string& k);
};

} // namespace apb
