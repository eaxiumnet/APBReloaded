#pragma once
#include "APBTypes.h"
#include "APBCrypto.h"
#include <set>

namespace apb {

/** Minimal real mail service (M2, ARCHITECTURE.md §4): messages carry
 *  id/from/to/subject/body, item+cash attachments, and a read flag.
 *  A cash-only send is represented as one attachment with empty item_id. */
struct MailAttachment {
	std::string item_id;
	int32_t count = 0;
	int64_t cash = 0;
};

struct MailMessage {
	int64_t id = 0;
	std::string from, to, subject, body;
	std::vector<MailAttachment> attachments;
	bool read = false;
	bool claimed = false; // attachments transferred to the recipient (retail "Take All")
	int64_t created_utc = 0; // epoch seconds; 0 = no expiry tracking (legacy/system mail)
};

class MailService {
public:
	std::vector<MailMessage> messages;
	int64_t next_mail_id = 1;

	bool SendMail(const std::string& from, const std::string& to, const std::string& subject,
		const std::string& body, int64_t cash = 0, int64_t created_utc = 0) {
		std::vector<MailAttachment> att;
		if (cash > 0) att.push_back(MailAttachment{"", 0, cash});
		return SendMailWithAttachments(from, to, subject, body, att, created_utc);
	}

	bool SendMailWithAttachments(const std::string& from, const std::string& to, const std::string& subject,
		const std::string& body, const std::vector<MailAttachment>& attachments, int64_t created_utc = 0) {
		if (from.empty() || to.empty()) return false;
		MailMessage m;
		m.id = next_mail_id++;
		m.from = from; m.to = to; m.subject = subject; m.body = body;
		m.attachments = attachments;
		m.created_utc = created_utc;
		messages.push_back(m);
		return true;
	}

	std::vector<const MailMessage*> InboxFor(const std::string& character) const {
		std::vector<const MailMessage*> out;
		for (const auto& m : messages) if (m.to == character) out.push_back(&m);
		return out;
	}

	int32_t UnreadCount(const std::string& character) const {
		int32_t n = 0;
		for (const auto& m : messages) if (m.to == character && !m.read) ++n;
		return n;
	}

	MailMessage* Find(int64_t id) {
		for (auto& m : messages) if (m.id == id) return &m;
		return nullptr;
	}
	const MailMessage* Find(int64_t id) const {
		for (const auto& m : messages) if (m.id == id) return &m;
		return nullptr;
	}

	bool MarkRead(int64_t id) {
		if (MailMessage* m = Find(id)) { m->read = true; return true; }
		return false;
	}

	// Retail attachment claim ("Take All"): hands the recipient the cash+item
	// attachments exactly once, then marks the message read + claimed. The returned
	// attachments are what the caller applies to currency/inventory. A second claim
	// (or a message with no attachments) returns empty.
	//
	// FAIL-CLOSED on item payloads (M14 S10): inventory granting does not exist yet,
	// so an item-bearing message is refused outright rather than claimed-and-dropped.
	// Leaving claimed=false is load-bearing — Delete() refuses while attachments are
	// unclaimed, so the message stays undeletable and reclaimable until items land.
	std::vector<MailAttachment> ClaimAttachments(int64_t id) {
		MailMessage* m = Find(id);
		if (!m || m->claimed || m->attachments.empty()) return {};
		if (HasItemAttachments(id)) return {};
		m->claimed = true;
		m->read = true;
		return m->attachments;
	}

	bool HasItemAttachments(int64_t id) const {
		const MailMessage* m = Find(id);
		if (!m) return false;
		for (const MailAttachment& a : m->attachments) if (!a.item_id.empty()) return true;
		return false;
	}

	// Flag-only half of the journaled claim: the caller credits cash and persists a
	// durable receipt BEFORE the mail flag is committed, so this step must be
	// callable on its own. Refuses a second commit so replay cannot double-claim.
	bool CommitClaimed(int64_t id) {
		MailMessage* m = Find(id);
		if (!m || m->claimed) return false;
		m->claimed = true;
		m->read = true;
		return true;
	}

	bool HasUnclaimedAttachments(int64_t id) const {
		const MailMessage* m = Find(id);
		return m && !m->claimed && !m->attachments.empty();
	}

	// Retail discard: a message that still holds unclaimed attachments cannot be
	// deleted (the client forces "Take All" first). Returns false in that case.
	bool Delete(int64_t id) {
		for (auto it = messages.begin(); it != messages.end(); ++it) {
			if (it->id == id) {
				if (!it->claimed && !it->attachments.empty()) return false;
				messages.erase(it);
				return true;
			}
		}
		return false;
	}

	// Retail mail expiry: messages older than ttl_seconds are purged (retail default is
	// ~30 days = 2592000s). Only messages with created_utc > 0 are considered — legacy /
	// system mail with no timestamp never expires. `now` is supplied by the caller (the
	// server clock) so the Domain stays clock-free and deterministic under test. Returns
	// the number of messages removed. Callers that want retail "return unclaimed
	// attachments to sender" semantics should inspect HasUnclaimedAttachments first.
	int32_t PurgeExpired(int64_t now, int64_t ttl_seconds) {
		int32_t removed = 0;
		for (auto it = messages.begin(); it != messages.end(); ) {
			if (it->created_utc > 0 && now - it->created_utc >= ttl_seconds) {
				it = messages.erase(it);
				++removed;
			} else {
				++it;
			}
		}
		return removed;
	}
};

/** Non-blocking social stubs: friends, clan — do not block core loop. */
struct FriendEntry {
	std::string name;
	bool online = false;
};

struct ClanInfo {
	std::string id, name, tag;
	std::vector<std::string> members;
};

class SocialService {
public:
	std::vector<FriendEntry> friends;
	std::optional<ClanInfo> clan;

	bool AddFriend(const std::string& name) {
		if (name.empty()) return false;
		for (const auto& f : friends) if (f.name == name) return true;
		friends.push_back(FriendEntry{name, false});
		return true;
	}

	bool CreateClan(const std::string& id, const std::string& name, const std::string& tag, const std::string& leader) {
		if (id.empty() || name.empty() || leader.empty()) return false;
		ClanInfo c; c.id = id; c.name = name; c.tag = tag; c.members.push_back(leader);
		clan = c;
		return true;
	}

	bool ClanInvite(const std::string& member) {
		if (!clan || member.empty()) return false;
		for (const auto& m : clan->members) if (m == member) return true;
		clan->members.push_back(member);
		return true;
	}
};

/** Account / login surface (protocol-shaped, offline private). */
struct AccountRecord {
	std::string account_id;
	std::string username;
	std::string password_hash;
	std::string password_salt; // empty = legacy plaintext record (migrate on next login)
	bool banned = false;
	std::string created_utc;
	std::string last_login_utc;
};

class LoginService {
public:
	std::unordered_map<std::string, AccountRecord> accounts; // key=username
	std::optional<AccountRecord> session;

	bool Register(const std::string& user, const std::string& pass) {
		if (user.empty() || pass.empty() || accounts.count(user)) return false;
		AccountRecord a;
		a.account_id = "ACC-" + user;
		a.username   = user;
		a.password_salt = random_hex(16); // 16 bytes = 32 hex chars
		auto salt_bytes = hex_decode(a.password_salt);
		auto pass_bytes = reinterpret_cast<const uint8_t*>(pass.data());
		auto dk = pbkdf2_hmac_sha256(pass_bytes, pass.size(),
		                              salt_bytes.data(), salt_bytes.size());
		a.password_hash = hex_encode(dk.data(), dk.size());
		accounts[user] = a;
		return true;
	}

	bool Login(const std::string& user, const std::string& pass) {
		auto it = accounts.find(user);
		if (it == accounts.end() || it->second.banned) return false;
		AccountRecord& rec = it->second;
		if (rec.password_salt.empty()) {
			// Legacy plaintext path: compare directly, then migrate in-place.
			if (rec.password_hash != pass) return false;
			rec.password_salt = random_hex(16);
			auto salt_bytes = hex_decode(rec.password_salt);
			auto pass_bytes = reinterpret_cast<const uint8_t*>(pass.data());
			auto dk = pbkdf2_hmac_sha256(pass_bytes, pass.size(),
			                              salt_bytes.data(), salt_bytes.size());
			rec.password_hash = hex_encode(dk.data(), dk.size());
		} else {
			// Salted PBKDF2 path: constant-time compare.
			auto salt_bytes = hex_decode(rec.password_salt);
			auto pass_bytes = reinterpret_cast<const uint8_t*>(pass.data());
			auto dk = pbkdf2_hmac_sha256(pass_bytes, pass.size(),
			                              salt_bytes.data(), salt_bytes.size());
			std::string candidate = hex_encode(dk.data(), dk.size());
			if (candidate.size() != rec.password_hash.size()) return false;
			unsigned char diff = 0;
			for (size_t i = 0; i < candidate.size(); ++i)
				diff |= (unsigned char)(candidate[i] ^ rec.password_hash[i]);
			if (diff != 0) return false;
		}
		session = rec;
		return true;
	}

	void Logout() { session.reset(); }
	bool IsLoggedIn() const { return session.has_value(); }
};

/** World list / status (before district enter). */
struct WorldInstance {
	std::string id;
	std::string name;
	std::string status; // online, full, maintenance
	int32_t population = 0;
	int32_t capacity = 5000;
};

class WorldDirectory {
public:
	std::vector<WorldInstance> worlds;

	void EnsureDefault() {
		if (!worlds.empty()) return;
		worlds.push_back(WorldInstance{"W1", "San Paro - Primary", "online", 0, 5000});
		worlds.push_back(WorldInstance{"W2", "San Paro - Overflow", "online", 0, 5000});
	}

	std::vector<WorldInstance> ListOnline() const {
		std::vector<WorldInstance> o;
		for (const auto& w : worlds) if (w.status == "online") o.push_back(w);
		return o;
	}

	bool EnterWorld(const std::string& world_id, int32_t& out_pop) {
		for (auto& w : worlds) {
			if (w.id == world_id && w.status == "online" && w.population < w.capacity) {
				w.population++;
				out_pop = w.population;
				return true;
			}
		}
		return false;
	}
};

/** District reserve / queue / enter / exit with population. */
enum class DistrictQueueState { None, Queued, Reserved, InDistrict };

struct DistrictReservation {
	std::string district_id;
	std::string session_id;
	std::string player;
	DistrictQueueState state = DistrictQueueState::None;
	int32_t queue_position = 0;
	int32_t population = 0;
	int32_t max_players = 64;
};

class DistrictRouter {
public:
	std::unordered_map<std::string, int32_t> population_by_district;
	std::unordered_map<std::string, std::vector<std::string>> queues; // district -> players
	std::unordered_map<std::string, DistrictReservation> by_player;

	DistrictReservation ReserveOrQueue(const std::string& district_id, const std::string& player, int32_t max_players = 64) {
		DistrictReservation r;
		r.district_id = district_id;
		r.player = player;
		r.max_players = max_players;
		r.population = population_by_district[district_id];
		if (r.population < max_players) {
			r.state = DistrictQueueState::Reserved;
			r.session_id = "DS-" + district_id + "-1";
			r.queue_position = 0;
		} else {
			r.state = DistrictQueueState::Queued;
			queues[district_id].push_back(player);
			r.queue_position = (int32_t)queues[district_id].size();
			r.session_id.clear();
		}
		by_player[player] = r;
		return r;
	}

	bool Enter(const std::string& player) {
		auto it = by_player.find(player);
		if (it == by_player.end()) return false;
		if (it->second.state != DistrictQueueState::Reserved && it->second.state != DistrictQueueState::InDistrict)
			return false;
		if (it->second.state == DistrictQueueState::Reserved) {
			population_by_district[it->second.district_id]++;
			it->second.population = population_by_district[it->second.district_id];
			it->second.state = DistrictQueueState::InDistrict;
		}
		return true;
	}

	bool Exit(const std::string& player) {
		auto it = by_player.find(player);
		if (it == by_player.end()) return false;
		if (it->second.state == DistrictQueueState::InDistrict) {
			auto& pop = population_by_district[it->second.district_id];
			if (pop > 0) pop--;
		}
		// promote queue head
		auto& q = queues[it->second.district_id];
		if (!q.empty()) {
			std::string next = q.front();
			q.erase(q.begin());
			if (by_player.count(next)) {
				by_player[next].state = DistrictQueueState::Reserved;
				by_player[next].session_id = "DS-" + it->second.district_id + "-1";
				by_player[next].queue_position = 0;
			}
		}
		by_player.erase(it);
		return true;
	}
};

/** Character config blob store (file/config service shape). */
class ConfigBlobStore {
public:
	std::unordered_map<std::string, std::string> blobs; // character -> blob

	bool Save(const std::string& character, const std::string& blob) {
		if (character.empty()) return false;
		blobs[character] = blob;
		return true;
	}
	bool Load(const std::string& character, std::string& out) const {
		auto it = blobs.find(character);
		if (it == blobs.end()) return false;
		out = it->second;
		return true;
	}
};

/** Chunked district stream planner (World Partition equivalent data). */
struct StreamChunk {
	std::string id;
	double origin_x = 0, origin_y = 0, size = 64;
	double building_density = 0.5;
	std::vector<std::string> landmarks;
};

struct DistrictStreamPlan {
	std::string active_district_id;
	double chunk_size = 64;
	int32_t stream_radius_chunks = 2;
	std::vector<StreamChunk> chunks;

	std::vector<std::string> ChunksNear(double x, double y) const {
		std::vector<std::string> out;
		const double r = stream_radius_chunks * chunk_size;
		for (const auto& c : chunks) {
			double cx = c.origin_x + c.size * 0.5;
			double cy = c.origin_y + c.size * 0.5;
			double dx = cx - x, dy = cy - y;
			if (dx * dx + dy * dy <= r * r) out.push_back(c.id);
		}
		return out;
	}
};

} // namespace apb
