// APBClan.cpp — M14 (D10) clan domain implementation.
// Pure C++17, depends only on APBTypes.h (Faction). Mirrors APBGroup.cpp shape.
#include "APBClan.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace apb {

// --- self-contained JSON helpers (internal linkage; no ODR clash if co-linked
//     with the identically-named helpers in APBPersistence/APBCatalog) ---------
namespace {

std::string JsonEscapeC(const std::string& s) {
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
	return out;   // NOTE: empty string is a valid value (e.g. an empty MOTD)
}

int64_t JsonGetI64(const std::string& obj, const std::string& key, int64_t def) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	const char* start = obj.c_str() + p; char* end = nullptr;
	long long v = std::strtoll(start, &end, 10);
	return end == start ? def : (int64_t)v;
}

// Bracket-inclusive body of the array under "key", string-aware (ignores brackets
// inside string values). Returns the first matching array at/after the key.
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

// Depth-0 {...} spans, string-aware (robust against braces inside string values
// and against nested arrays/objects within each top-level object).
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

} // namespace

// --- Clan helpers ------------------------------------------------------------

const ClanMember* Clan::FindMember(const std::string& player) const {
	for (const auto& m : members) if (m.player == player) return &m;
	return nullptr;
}
ClanMember* Clan::FindMember(const std::string& player) {
	for (auto& m : members) if (m.player == player) return &m;
	return nullptr;
}

static std::string ToLower(const std::string& s) {
	std::string o; o.resize(s.size());
	for (size_t i = 0; i < s.size(); ++i) o[i] = (char)std::tolower((unsigned char)s[i]);
	return o;
}

// --- private helpers ---------------------------------------------------------

Clan* ClanService::FindMut(const std::string& clan_id) {
	auto it = clans_.find(clan_id);
	return it == clans_.end() ? nullptr : &it->second;
}

void ClanService::ClearInvite(const std::string& invitee, const std::string& clan_id) {
	auto it = invites_.find(invitee);
	if (it == invites_.end()) return;
	auto& v = it->second;
	v.erase(std::remove(v.begin(), v.end(), clan_id), v.end());
	if (v.empty()) invites_.erase(it);
}

bool ClanService::NameOrTagTaken(const std::string& name, const std::string& tag) const {
	const std::string ln = ToLower(name), lt = ToLower(tag);
	for (const auto& kv : clans_) {
		if (ToLower(kv.second.name) == ln) return true;
		if (!lt.empty() && ToLower(kv.second.tag) == lt) return true;
	}
	return false;
}

void ClanService::AddMemberAtLowestRank(Clan& c, const std::string& player) {
	ClanMember m;
	m.player = player;
	m.rank_index = c.ranks.empty() ? 0 : (int32_t)c.ranks.size() - 1; // lowest = last
	m.joined_seq = next_join_seq_++;
	c.members.push_back(m);
	player_clan_[player] = c.id;
}

void ClanService::DisbandInternal(Clan& c) {
	const std::string id = c.id;
	for (const auto& m : c.members) player_clan_.erase(m.player);
	for (auto it = invites_.begin(); it != invites_.end();) {
		auto& v = it->second;
		v.erase(std::remove(v.begin(), v.end(), id), v.end());
		if (v.empty()) it = invites_.erase(it); else ++it;
	}
	clans_.erase(id);
}

uint32_t ClanService::MemberPerms(const Clan& c, const std::string& player) {
	if (c.leader == player) return ClanPerm::All;
	const ClanMember* m = c.FindMember(player);
	if (!m) return ClanPerm::None;
	if (m->rank_index < 0 || m->rank_index >= (int32_t)c.ranks.size()) return ClanPerm::None;
	return c.ranks[m->rank_index].permissions;
}

// --- lifecycle & membership --------------------------------------------------

ClanResult ClanService::CreateClan(const std::string& id, const std::string& name, const std::string& tag,
	Faction faction, const std::string& leader) {
	if (id.empty() || name.empty() || tag.empty() || leader.empty()) return ClanResult::InvalidArg;
	if (clans_.count(id)) return ClanResult::NameTaken;
	if (player_clan_.count(leader)) return ClanResult::PlayerInClan;
	if (NameOrTagTaken(name, tag)) return ClanResult::NameTaken;

	Clan c;
	c.id = id; c.name = name; c.tag = tag; c.faction = faction; c.leader = leader;
	// Default rank ladder (mechanism is 1:1; names refine to apbdb when confirmed).
	c.ranks.push_back(ClanRank{"Officer", ClanPerm::Invite | ClanPerm::Kick | ClanPerm::EditMotd});
	c.ranks.push_back(ClanRank{"Member",  ClanPerm::None});
	clans_[id] = c;
	// Leader joins at rank 0 (perms are All via MemberPerms special-case regardless).
	ClanMember lm; lm.player = leader; lm.rank_index = 0; lm.joined_seq = next_join_seq_++;
	clans_[id].members.push_back(lm);
	player_clan_[leader] = id;
	return ClanResult::Ok;
}

ClanResult ClanService::Invite(const std::string& inviter, const std::string& invitee, Faction invitee_faction) {
	if (inviter.empty() || invitee.empty()) return ClanResult::InvalidArg;
	if (inviter == invitee) return ClanResult::SelfTarget;
	std::string cid = ClanOf(inviter);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if ((MemberPerms(*c, inviter) & ClanPerm::Invite) == 0) return ClanResult::NoPermission;
	if (invitee_faction != c->faction) return ClanResult::WrongFaction;
	if (player_clan_.count(invitee)) return ClanResult::TargetInClan;
	if (c->IsFull(max_members)) return ClanResult::ClanFull;

	auto& pend = invites_[invitee];
	if (std::find(pend.begin(), pend.end(), cid) != pend.end()) return ClanResult::AlreadyInvited;
	pend.push_back(cid);
	return ClanResult::Ok;
}

ClanResult ClanService::AcceptInvite(const std::string& invitee) {
	if (invitee.empty()) return ClanResult::InvalidArg;
	if (player_clan_.count(invitee)) return ClanResult::PlayerInClan;
	auto it = invites_.find(invitee);
	if (it == invites_.end() || it->second.empty()) return ClanResult::NoSuchInvite;

	std::string cid;
	Clan* c = nullptr;
	while (!it->second.empty()) {
		cid = it->second.back();
		c = FindMut(cid);
		if (c) break;
		it->second.pop_back();
	}
	if (!c) { invites_.erase(it); return ClanResult::NoSuchClan; }
	if (c->IsFull(max_members)) return ClanResult::ClanFull;

	AddMemberAtLowestRank(*c, invitee);
	invites_.erase(invitee); // joining clears all other pending invites
	return ClanResult::Ok;
}

ClanResult ClanService::DeclineInvite(const std::string& invitee) {
	if (invitee.empty()) return ClanResult::InvalidArg;
	auto it = invites_.find(invitee);
	if (it == invites_.end() || it->second.empty()) return ClanResult::NoSuchInvite;
	it->second.pop_back();
	if (it->second.empty()) invites_.erase(it);
	return ClanResult::Ok;
}

ClanResult ClanService::Leave(const std::string& player) {
	if (player.empty()) return ClanResult::InvalidArg;
	std::string cid = ClanOf(player);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	// Retail rule (ClanLeaveLeaderFail): the leader cannot leave outright.
	if (c->leader == player) return ClanResult::LeaderMustTransferOrDisband;

	auto& mv = c->members;
	mv.erase(std::remove_if(mv.begin(), mv.end(),
		[&](const ClanMember& m) { return m.player == player; }), mv.end());
	player_clan_.erase(player);
	return ClanResult::Ok;
}

ClanResult ClanService::Kick(const std::string& actor, const std::string& target) {
	if (actor.empty() || target.empty()) return ClanResult::InvalidArg;
	if (actor == target) return ClanResult::SelfTarget;
	std::string cid = ClanOf(actor);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if ((MemberPerms(*c, actor) & ClanPerm::Kick) == 0) return ClanResult::NoPermission;
	if (target == c->leader) return ClanResult::NoPermission; // leader is untouchable
	const ClanMember* tm = c->FindMember(target);
	if (!tm) return ClanResult::NoSuchMember;
	// A non-leader may only kick members strictly lower in rank (higher index).
	if (c->leader != actor) {
		const ClanMember* am = c->FindMember(actor);
		if (!am || tm->rank_index <= am->rank_index) return ClanResult::NoPermission;
	}
	auto& mv = c->members;
	mv.erase(std::remove_if(mv.begin(), mv.end(),
		[&](const ClanMember& m) { return m.player == target; }), mv.end());
	player_clan_.erase(target);
	return ClanResult::Ok;
}

ClanResult ClanService::Disband(const std::string& leader) {
	if (leader.empty()) return ClanResult::InvalidArg;
	std::string cid = ClanOf(leader);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if (c->leader != leader) return ClanResult::NotLeader;
	DisbandInternal(*c);
	return ClanResult::Ok;
}

ClanResult ClanService::TransferLeader(const std::string& leader, const std::string& target) {
	if (leader.empty() || target.empty()) return ClanResult::InvalidArg;
	if (leader == target) return ClanResult::SelfTarget;
	std::string cid = ClanOf(leader);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if (c->leader != leader) return ClanResult::NotLeader;
	ClanMember* tm = c->FindMember(target);
	if (!tm) return ClanResult::NoSuchMember;
	// New leader takes the top; old leader stays as a top-rank member (Officer = rank 0).
	tm->rank_index = 0;
	if (ClanMember* om = c->FindMember(leader)) om->rank_index = 0;
	c->leader = target;
	return ClanResult::Ok;
}

// --- ranks & MOTD ------------------------------------------------------------

ClanResult ClanService::SetMotd(const std::string& actor, const std::string& text) {
	std::string cid = ClanOf(actor);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if ((MemberPerms(*c, actor) & ClanPerm::EditMotd) == 0) return ClanResult::NoPermission;
	c->motd = text;
	return ClanResult::Ok;
}

ClanResult ClanService::AddRank(const std::string& actor, const std::string& name, uint32_t permissions) {
	if (name.empty()) return ClanResult::InvalidArg;
	std::string cid = ClanOf(actor);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if ((MemberPerms(*c, actor) & ClanPerm::ManageRanks) == 0) return ClanResult::NoPermission;
	c->ranks.push_back(ClanRank{name, permissions});
	return ClanResult::Ok;
}

ClanResult ClanService::SetMemberRank(const std::string& actor, const std::string& target, int32_t rank_index) {
	if (target.empty()) return ClanResult::InvalidArg;
	std::string cid = ClanOf(actor);
	if (cid.empty()) return ClanResult::NotInClan;
	Clan* c = FindMut(cid);
	if (!c) return ClanResult::NoSuchClan;
	if ((MemberPerms(*c, actor) & ClanPerm::ManageRanks) == 0) return ClanResult::NoPermission;
	if (target == c->leader) return ClanResult::NoPermission; // leader rank is fixed
	if (rank_index < 0 || rank_index >= (int32_t)c->ranks.size()) return ClanResult::NoSuchRank;
	ClanMember* tm = c->FindMember(target);
	if (!tm) return ClanResult::NoSuchMember;
	tm->rank_index = rank_index;
	return ClanResult::Ok;
}

// --- persistence -------------------------------------------------------------

std::string ClanService::SaveJson() const {
	// Deterministic clan order (sorted ids) so saves are reproducible/diffable.
	std::vector<std::string> ids;
	ids.reserve(clans_.size());
	for (const auto& kv : clans_) ids.push_back(kv.first);
	std::sort(ids.begin(), ids.end());

	std::ostringstream ss;
	ss << "{\n  \"next_join_seq\": " << next_join_seq_ << ",\n  \"clans\": [";
	bool first_clan = true;
	for (const auto& id : ids) {
		const Clan& c = clans_.at(id);
		if (!first_clan) ss << ",";
		first_clan = false;
		ss << "\n    { \"id\": \"" << JsonEscapeC(c.id)
			<< "\", \"name\": \"" << JsonEscapeC(c.name)
			<< "\", \"tag\": \"" << JsonEscapeC(c.tag)
			<< "\", \"faction\": \"" << FactionName(c.faction)
			<< "\", \"leader\": \"" << JsonEscapeC(c.leader)
			<< "\", \"motd\": \"" << JsonEscapeC(c.motd)
			<< "\", \"ranks\": [";
		bool first_rank = true;
		for (const auto& r : c.ranks) {
			if (!first_rank) ss << ",";
			first_rank = false;
			ss << " { \"name\": \"" << JsonEscapeC(r.name)
				<< "\", \"perms\": " << (uint64_t)r.permissions << " }";
		}
		ss << " ], \"members\": [";
		bool first_mem = true;
		for (const auto& m : c.members) {
			if (!first_mem) ss << ",";
			first_mem = false;
			ss << " { \"player\": \"" << JsonEscapeC(m.player)
				<< "\", \"rank\": " << m.rank_index
				<< ", \"seq\": " << m.joined_seq << " }";
		}
		ss << " ] }";
	}
	ss << "\n  ]\n}\n";
	return ss.str();
}

bool ClanService::LoadJson(const std::string& text) {
	if (text.empty()) return false;
	// Replace all state (a load is authoritative). Invites are session-transient.
	clans_.clear();
	player_clan_.clear();
	invites_.clear();
	next_join_seq_ = 1;

	const int64_t nseq = JsonGetI64(text, "next_join_seq", 1);

	const std::string clans_body = ExtractArrayBody(text, "clans");
	for (const auto& cobj : SplitObjects(clans_body)) {
		Clan c;
		c.id = JsonGetStr(cobj, "id");
		if (c.id.empty()) continue;
		c.name    = JsonGetStr(cobj, "name");
		c.tag     = JsonGetStr(cobj, "tag");
		c.faction = FactionFromString(JsonGetStr(cobj, "faction", "Criminal"));
		c.leader  = JsonGetStr(cobj, "leader");
		c.motd    = JsonGetStr(cobj, "motd");

		for (const auto& robj : SplitObjects(ExtractArrayBody(cobj, "ranks"))) {
			ClanRank r;
			r.name = JsonGetStr(robj, "name");
			r.permissions = (uint32_t)JsonGetI64(robj, "perms", 0);
			c.ranks.push_back(r);
		}
		for (const auto& mobj : SplitObjects(ExtractArrayBody(cobj, "members"))) {
			ClanMember m;
			m.player = JsonGetStr(mobj, "player");
			if (m.player.empty()) continue;
			m.rank_index = (int32_t)JsonGetI64(mobj, "rank", 0);
			m.joined_seq = JsonGetI64(mobj, "seq", 0);
			c.members.push_back(m);
			player_clan_[m.player] = c.id;
		}
		clans_[c.id] = c;
	}
	next_join_seq_ = nseq < 1 ? 1 : nseq;
	return !clans_.empty();
}

// --- queries -----------------------------------------------------------------

bool ClanService::InClan(const std::string& player) const {
	return player_clan_.count(player) != 0;
}

std::string ClanService::ClanOf(const std::string& player) const {
	auto it = player_clan_.find(player);
	return it == player_clan_.end() ? std::string() : it->second;
}

std::string ClanService::LeaderOf(const std::string& clan_id) const {
	const Clan* c = Find(clan_id);
	return c ? c->leader : std::string();
}

bool ClanService::IsLeader(const std::string& player) const {
	std::string cid = ClanOf(player);
	if (cid.empty()) return false;
	const Clan* c = Find(cid);
	return c && c->leader == player;
}

const Clan* ClanService::Find(const std::string& clan_id) const {
	auto it = clans_.find(clan_id);
	return it == clans_.end() ? nullptr : &it->second;
}

int32_t ClanService::Size(const std::string& clan_id) const {
	const Clan* c = Find(clan_id);
	return c ? (int32_t)c->members.size() : 0;
}

std::string ClanService::RankOf(const std::string& player) const {
	std::string cid = ClanOf(player);
	if (cid.empty()) return std::string();
	const Clan* c = Find(cid);
	if (!c) return std::string();
	const ClanMember* m = c->FindMember(player);
	if (!m || m->rank_index < 0 || m->rank_index >= (int32_t)c->ranks.size()) return std::string();
	return c->ranks[m->rank_index].name;
}

bool ClanService::HasPermission(const std::string& player, uint32_t perm) const {
	std::string cid = ClanOf(player);
	if (cid.empty()) return false;
	const Clan* c = Find(cid);
	if (!c) return false;
	return (MemberPerms(*c, player) & perm) == perm;
}

std::string ClanService::MotdOf(const std::string& clan_id) const {
	const Clan* c = Find(clan_id);
	return c ? c->motd : std::string();
}

std::vector<std::string> ClanService::MembersOf(const std::string& clan_id) const {
	std::vector<std::string> out;
	const Clan* c = Find(clan_id);
	if (c) for (const auto& m : c->members) out.push_back(m.player);
	return out;
}

std::vector<std::string> ClanService::PendingInvitesFor(const std::string& invitee) const {
	auto it = invites_.find(invitee);
	return it == invites_.end() ? std::vector<std::string>() : it->second;
}

} // namespace apb
