// APBFriends.cpp — M14 (D10) friends + ignore domain implementation.
// Pure C++17, no UE/platform deps. Mirrors APBClan.cpp shape (incl. JSON helpers).
#include "APBFriends.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace apb {

// --- self-contained JSON helpers (internal linkage) --------------------------
namespace {

std::string JsonEscapeF(const std::string& s) {
	std::string o; o.reserve(s.size());
	for (char c : s) {
		if (c == char(34)) o += "\\\"";
		else if (c == char(92)) o += "\\\\";
		else if ((unsigned char)c < 0x20) o.push_back(' ');
		else o.push_back(c);
	}
	return o;
}

std::string JsonGetStr(const std::string& obj, const std::string& key, const std::string& def = "") {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def;
	p = obj.find(char(34), p + 1); if (p == std::string::npos) return def;
	size_t e = p + 1; std::string out;
	while (e < obj.size()) {
		if (obj[e] == char(92) && e + 1 < obj.size()) { out.push_back(obj[e + 1]); e += 2; continue; }
		if (obj[e] == char(34)) break;
		out.push_back(obj[e]); ++e;
	}
	return out;
}

// Bracket-inclusive body of the array under "key", string-aware.
std::string ExtractArrayBody(const std::string& text, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = text.find(pat); if (p == std::string::npos) return {};
	p = text.find('[', p + pat.size()); if (p == std::string::npos) return {};
	int depth = 0; bool instr = false;
	for (size_t i = p; i < text.size(); ++i) {
		char c = text[i];
		if (instr) { if (c == char(92)) { ++i; continue; } if (c == char(34)) instr = false; continue; }
		if (c == char(34)) { instr = true; continue; }
		if (c == '[') ++depth;
		else if (c == ']') { --depth; if (depth == 0) return text.substr(p, i - p + 1); }
	}
	return {};
}

// Depth-0 {...} spans, string-aware.
std::vector<std::string> SplitObjects(const std::string& text) {
	std::vector<std::string> out;
	int depth = 0; bool instr = false; size_t start = std::string::npos;
	for (size_t i = 0; i < text.size(); ++i) {
		char c = text[i];
		if (instr) { if (c == char(92)) { ++i; continue; } if (c == char(34)) instr = false; continue; }
		if (c == char(34)) { instr = true; continue; }
		if (c == '{') { if (depth == 0) start = i; ++depth; }
		else if (c == '}') { --depth; if (depth == 0 && start != std::string::npos) { out.push_back(text.substr(start, i - start + 1)); start = std::string::npos; } }
	}
	return out;
}

// String-aware "..." token extraction from an array body (for the members list).
std::vector<std::string> SplitStringArray(const std::string& body) {
	std::vector<std::string> out;
	bool instr = false; std::string cur;
	for (size_t i = 0; i < body.size(); ++i) {
		char c = body[i];
		if (instr) {
			if (c == char(92) && i + 1 < body.size()) { cur.push_back(body[i + 1]); ++i; continue; }
			if (c == char(34)) { out.push_back(cur); cur.clear(); instr = false; continue; }
			cur.push_back(c);
		} else if (c == char(34)) {
			instr = true;
		}
	}
	return out;
}

} // namespace

// --- private helpers ---------------------------------------------------------

const std::set<std::string>* FriendsService::Get(
	const std::unordered_map<std::string, std::set<std::string>>& m, const std::string& k) {
	auto it = m.find(k);
	return it == m.end() ? nullptr : &it->second;
}

bool FriendsService::FriendsFull(const std::string& player) const {
	const std::set<std::string>* s = Get(friends_, player);
	return s && (int32_t)s->size() >= max_friends;
}

void FriendsService::ClearRequestPair(const std::string& a, const std::string& b) {
	auto ra = requests_.find(a);
	if (ra != requests_.end()) { ra->second.erase(b); if (ra->second.empty()) requests_.erase(ra); }
	auto rb = requests_.find(b);
	if (rb != requests_.end()) { rb->second.erase(a); if (rb->second.empty()) requests_.erase(rb); }
}

// --- friend request / accept flow --------------------------------------------

FriendResult FriendsService::SendRequest(const std::string& from, const std::string& to) {
	if (from.empty() || to.empty()) return FriendResult::InvalidArg;
	if (from == to) return FriendResult::SelfTarget;
	if (AreFriends(from, to)) return FriendResult::AlreadyFriends;
	if (IsIgnoring(from, to)) return FriendResult::YouIgnoreTarget;
	if (IsIgnoring(to, from)) return FriendResult::TargetIgnoresYou;
	if (FriendsFull(from)) return FriendResult::FriendsListFull;
	if (FriendsFull(to)) return FriendResult::TargetListFull;

	// Reciprocal pending request -> auto-accept into a mutual friendship.
	if (HasIncoming(from, to)) {
		ClearRequestPair(from, to);
		friends_[from].insert(to);
		friends_[to].insert(from);
		return FriendResult::Ok;
	}
	auto& inc = requests_[to];
	if (inc.count(from)) return FriendResult::AlreadyInvited;
	inc.insert(from);
	return FriendResult::Ok;
}

FriendResult FriendsService::AcceptRequest(const std::string& invitee, const std::string& inviter) {
	if (invitee.empty() || inviter.empty()) return FriendResult::InvalidArg;
	if (!HasIncoming(invitee, inviter)) return FriendResult::NoSuchInvite;
	// Ignore invariants may have changed since the request was sent.
	if (IsIgnoring(invitee, inviter)) { ClearRequestPair(invitee, inviter); return FriendResult::YouIgnoreTarget; }
	if (IsIgnoring(inviter, invitee)) { ClearRequestPair(invitee, inviter); return FriendResult::TargetIgnoresYou; }
	if (FriendsFull(invitee)) return FriendResult::FriendsListFull;
	if (FriendsFull(inviter)) return FriendResult::TargetListFull;

	ClearRequestPair(invitee, inviter);
	friends_[invitee].insert(inviter);
	friends_[inviter].insert(invitee);
	return FriendResult::Ok;
}

FriendResult FriendsService::DeclineRequest(const std::string& invitee, const std::string& inviter) {
	if (invitee.empty() || inviter.empty()) return FriendResult::InvalidArg;
	if (!HasIncoming(invitee, inviter)) return FriendResult::NoSuchInvite;
	auto it = requests_.find(invitee);
	it->second.erase(inviter);
	if (it->second.empty()) requests_.erase(it);
	return FriendResult::Ok;
}

FriendResult FriendsService::RemoveFriend(const std::string& player, const std::string& other) {
	if (player.empty() || other.empty()) return FriendResult::InvalidArg;
	if (!AreFriends(player, other)) return FriendResult::NotFriends;
	auto pit = friends_.find(player);
	if (pit != friends_.end()) { pit->second.erase(other); if (pit->second.empty()) friends_.erase(pit); }
	auto oit = friends_.find(other);
	if (oit != friends_.end()) { oit->second.erase(player); if (oit->second.empty()) friends_.erase(oit); }
	return FriendResult::Ok;
}

// --- ignore (block) list -----------------------------------------------------

FriendResult FriendsService::Ignore(const std::string& player, const std::string& target) {
	if (player.empty() || target.empty()) return FriendResult::InvalidArg;
	if (player == target) return FriendResult::SelfTarget;
	if (AreFriends(player, target)) return FriendResult::TargetIsFriend; // unfriend first
	if (IsIgnoring(player, target)) return FriendResult::AlreadyIgnored;
	const std::set<std::string>* ig = Get(ignores_, player);
	if (ig && (int32_t)ig->size() >= max_ignores) return FriendResult::IgnoreListFull;
	ignores_[player].insert(target);
	// Ignoring drops any pending request between the two (either direction).
	ClearRequestPair(player, target);
	return FriendResult::Ok;
}

FriendResult FriendsService::Unignore(const std::string& player, const std::string& target) {
	if (player.empty() || target.empty()) return FriendResult::InvalidArg;
	auto it = ignores_.find(player);
	if (it == ignores_.end() || !it->second.count(target)) return FriendResult::NotIgnored;
	it->second.erase(target);
	if (it->second.empty()) ignores_.erase(it);
	return FriendResult::Ok;
}

// --- presence ----------------------------------------------------------------

void FriendsService::SetOnline(const std::string& player, bool online) {
	if (player.empty()) return;
	if (online) online_.insert(player);
	else online_.erase(player);
}

// --- queries -----------------------------------------------------------------

bool FriendsService::AreFriends(const std::string& a, const std::string& b) const {
	const std::set<std::string>* s = Get(friends_, a);
	return s && s->count(b) != 0;
}

bool FriendsService::IsIgnoring(const std::string& player, const std::string& target) const {
	const std::set<std::string>* s = Get(ignores_, player);
	return s && s->count(target) != 0;
}

bool FriendsService::IsOnline(const std::string& player) const {
	return online_.count(player) != 0;
}

int32_t FriendsService::FriendCount(const std::string& player) const {
	const std::set<std::string>* s = Get(friends_, player);
	return s ? (int32_t)s->size() : 0;
}

std::vector<std::string> FriendsService::FriendsOf(const std::string& player) const {
	std::vector<std::string> out;
	const std::set<std::string>* s = Get(friends_, player);
	if (s) out.assign(s->begin(), s->end()); // std::set is already sorted
	return out;
}

std::vector<std::string> FriendsService::OnlineFriendsOf(const std::string& player) const {
	std::vector<std::string> out;
	const std::set<std::string>* s = Get(friends_, player);
	if (s) for (const auto& f : *s) if (online_.count(f)) out.push_back(f);
	return out;
}

std::vector<std::string> FriendsService::IgnoredBy(const std::string& player) const {
	std::vector<std::string> out;
	const std::set<std::string>* s = Get(ignores_, player);
	if (s) out.assign(s->begin(), s->end());
	return out;
}

std::vector<std::string> FriendsService::IncomingRequests(const std::string& invitee) const {
	std::vector<std::string> out;
	const std::set<std::string>* s = Get(requests_, invitee);
	if (s) out.assign(s->begin(), s->end());
	return out;
}

std::vector<std::string> FriendsService::OutgoingRequests(const std::string& inviter) const {
	std::vector<std::string> out;
	for (const auto& kv : requests_) if (kv.second.count(inviter)) out.push_back(kv.first);
	std::sort(out.begin(), out.end());
	return out;
}

bool FriendsService::HasIncoming(const std::string& invitee, const std::string& inviter) const {
	const std::set<std::string>* s = Get(requests_, invitee);
	return s && s->count(inviter) != 0;
}

// --- persistence -------------------------------------------------------------
// Schema (friends.json):
//   { "friends": [ { "player":"a", "list":["b","c"] } ],
//     "ignores": [ { "player":"a", "list":["d"] } ] }
// Friendships are mutual, so both sides appear; load rebuilds symmetrically.

static std::string EmitListSection(const std::string& section,
	const std::unordered_map<std::string, std::set<std::string>>& m) {
	std::vector<std::string> owners;
	owners.reserve(m.size());
	for (const auto& kv : m) if (!kv.second.empty()) owners.push_back(kv.first);
	std::sort(owners.begin(), owners.end());
	std::ostringstream ss;
	ss << "\"" << section << "\": [";
	bool first = true;
	for (const auto& owner : owners) {
		if (!first) ss << ",";
		first = false;
		ss << "\n    { \"player\": \"" << JsonEscapeF(owner) << "\", \"list\": [";
		bool fl = true;
		for (const auto& n : m.at(owner)) {
			if (!fl) ss << ", ";
			fl = false;
			ss << "\"" << JsonEscapeF(n) << "\"";
		}
		ss << "] }";
	}
	ss << "\n  ]";
	return ss.str();
}

std::string FriendsService::SaveJson() const {
	std::ostringstream ss;
	ss << "{\n  " << EmitListSection("friends", friends_)
	   << ",\n  " << EmitListSection("ignores", ignores_)
	   << "\n}\n";
	return ss.str();
}

bool FriendsService::LoadJson(const std::string& text) {
	if (text.empty()) return false;
	friends_.clear();
	ignores_.clear();
	requests_.clear(); // pending requests are transient; a load is authoritative
	online_.clear();   // presence is transient

	for (const auto& obj : SplitObjects(ExtractArrayBody(text, "friends"))) {
		std::string owner = JsonGetStr(obj, "player");
		if (owner.empty()) continue;
		for (const auto& n : SplitStringArray(ExtractArrayBody(obj, "list"))) {
			if (n.empty() || n == owner) continue;
			friends_[owner].insert(n);
			friends_[n].insert(owner); // enforce symmetry defensively
		}
	}
	for (const auto& obj : SplitObjects(ExtractArrayBody(text, "ignores"))) {
		std::string owner = JsonGetStr(obj, "player");
		if (owner.empty()) continue;
		for (const auto& n : SplitStringArray(ExtractArrayBody(obj, "list"))) {
			if (n.empty() || n == owner) continue;
			ignores_[owner].insert(n);
		}
	}
	return !friends_.empty() || !ignores_.empty();
}

} // namespace apb
