#include "APBPersistence.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace apb {
namespace {

std::string ReadFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary); if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
bool WriteFile(const std::string& path, const std::string& text) {
	const std::filesystem::path target(path);
	const std::filesystem::path temp(path + ".tmp");
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out << text;
		out.flush();
		if (!out) {
			out.close();
			std::error_code remove_error;
			std::filesystem::remove(temp, remove_error);
			return false;
		}
		out.close();
		if (!out) {
			std::error_code remove_error;
			std::filesystem::remove(temp, remove_error);
			return false;
		}
	}
	std::error_code rename_error;
	std::filesystem::rename(temp, target, rename_error);
	if (rename_error) {
		std::error_code remove_error;
		std::filesystem::remove(temp, remove_error);
		return false;
	}
	return true;
}
std::string FmtDouble(double v) {
	std::ostringstream ss; ss << std::setprecision(17) << v; return ss.str();
}
std::string Sanitize(const std::string& s) {
	std::string o;
	for (char c : s) {
		if (isalnum((unsigned char)c) || c == '-' || c == '_') o.push_back(c);
		else o.push_back('_');
	}
	return o;
}
bool JsonGetBool(const std::string& obj, const std::string& key, bool def) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	if (obj.compare(p, 4, "true") == 0) return true;
	if (obj.compare(p, 5, "false") == 0) return false;
	return def;
}
/** Body of the JSON array stored under "key" (bracket-inclusive), string-aware. */
std::string ExtractArrayBody(const std::string& text, const std::string& key) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = text.find(pat); if (p == std::string::npos) return {};
	p = text.find('[', p + pat.size()); if (p == std::string::npos) return {};
	int depth = 0; bool instr = false;
	for (size_t i = p; i < text.size(); ++i) {
		char c = text[i];
		if (instr) {
			if (c == char(92)) { ++i; continue; }
			if (c == char(34)) instr = false;
			continue;
		}
		if (c == char(34)) { instr = true; continue; }
		if (c == '[') { ++depth; }
		else if (c == ']') { --depth; if (depth == 0) return text.substr(p, i - p + 1); }
	}
	return {};
}
/** Depth-0 {...} spans, string-aware (robust against braces inside string values). */
std::vector<std::string> SplitObjects(const std::string& text) {
	std::vector<std::string> out;
	int depth = 0; bool instr = false; size_t start = std::string::npos;
	for (size_t i = 0; i < text.size(); ++i) {
		char c = text[i];
		if (instr) {
			if (c == char(92)) { ++i; continue; }
			if (c == char(34)) instr = false;
			continue;
		}
		if (c == char(34)) { instr = true; continue; }
		if (c == '{') { if (depth == 0) start = i; ++depth; }
		else if (c == '}') { --depth; if (depth == 0 && start != std::string::npos) { out.push_back(text.substr(start, i - start + 1)); start = std::string::npos; } }
	}
	return out;
}

} // namespace

std::string NowUtcIso() {
	std::time_t t = std::time(nullptr);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buf;
}

std::string JsonEscape(const std::string& s) {
	std::string o; o.reserve(s.size());
	for (char c : s) {
		if (c == char(34)) o += "\\\"";
		else if (c == char(92)) o += "\\\\";
		else if ((unsigned char)c < 0x20) o.push_back(' ');
		else o.push_back(c);
	}
	return o;
}

bool JsonDomainStore::Init(const std::string& dir) {
	active_ = false;
	if (dir.empty()) return false;
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(dir) / "characters", ec);
	if (ec) return false;
	dir_ = dir;
	active_ = true;
	return true;
}

// ---------------------------------------------------------------- accounts
// Schema (ARCHITECTURE.md §4): { "accounts": [ { "id", "name", "pass", "banned",
//   "created_utc", "last_login_utc" } ] } — plaintext pass, private offline port.

bool JsonDomainStore::SaveAccounts(const LoginService& login) const {
	if (!active_) return false;
	std::vector<std::string> names;
	for (const auto& kv : login.accounts) names.push_back(kv.first);
	std::sort(names.begin(), names.end());
	std::ostringstream ss;
	ss << "{\n  \"accounts\": [";
	bool first = true;
	for (const auto& name : names) {
		const AccountRecord& a = login.accounts.at(name);
		if (!first) ss << ",";
		first = false;
		ss << "\n    { \"id\": \"" << JsonEscape(a.account_id)
			<< "\", \"name\": \"" << JsonEscape(a.username)
			<< "\", \"pass\": \"" << JsonEscape(a.password_hash)
			<< "\", \"salt\": \"" << JsonEscape(a.password_salt)
			<< "\", \"banned\": " << (a.banned ? "true" : "false")
			<< ", \"created_utc\": \"" << JsonEscape(a.created_utc)
			<< "\", \"last_login_utc\": \"" << JsonEscape(a.last_login_utc) << "\" }";
	}
	ss << "\n  ]\n}\n";
	return WriteFile(dir_ + "/accounts.json", ss.str());
}

bool JsonDomainStore::LoadAccounts(LoginService& login) const {
	if (!active_) return false;
	std::string text = ReadFile(dir_ + "/accounts.json");
	if (text.empty()) return false;
	std::string body = ExtractArrayBody(text, "accounts");
	const std::vector<std::string> objects = SplitObjects(body);
	if (objects.empty()) return false;
	std::vector<AccountRecord> records;
	for (const auto& obj : objects) {
		AccountRecord a;
		a.username = JsonGetString(obj, "name");
		if (a.username.empty()) continue;
		a.account_id = JsonGetString(obj, "id", "ACC-" + a.username);
		a.password_hash = JsonGetString(obj, "pass");
		a.password_salt = JsonGetString(obj, "salt"); // empty = legacy plaintext record
		a.banned = JsonGetBool(obj, "banned", false);
		a.created_utc = JsonGetString(obj, "created_utc");
		a.last_login_utc = JsonGetString(obj, "last_login_utc");
		records.push_back(std::move(a));
	}
	if (records.empty()) return false;
	for (auto& record : records) login.accounts[record.username] = std::move(record);
	return true;
}

// ---------------------------------------------------------------- characters
// Schema (§4 adapted): { "name", "faction", "appearance_blob",
//   "wardrobe": [ { "slot", "item_id", "color_primary", "color_secondary", "decal" } ],
//   "inventory": [ { "item_id", "count" } ], "cash", "g1c", "threat_points" }
// Wardrobe is an array (not the §4 object map) because ClothingSlot carries
// colors/decal; roles/clan_id/tutorial_done omitted (not modeled by Domain yet).

std::string JsonDomainStore::CharacterPath(const std::string& account, int32_t slot) const {
	return dir_ + "/characters/" + Sanitize(account) + "_" + std::to_string(slot) + ".json";
}

bool JsonDomainStore::HasCharacter(const std::string& account, int32_t slot) const {
	if (!active_) return false;
	std::error_code ec;
	return std::filesystem::exists(CharacterPath(account, slot), ec);
}

bool JsonDomainStore::SaveCharacter(const std::string& account, int32_t slot,
	const CharacterProfile& profile, const CharacterAppearance& appearance,
	const Inventory& inventory, double threat_points) const {
	if (!active_ || account.empty() || profile.name.empty()) return false;
	std::ostringstream ss;
	ss << "{\n";
	ss << "  \"name\": \"" << JsonEscape(profile.name) << "\",\n";
	ss << "  \"faction\": \"" << FactionName(profile.faction) << "\",\n";
	ss << "  \"appearance_blob\": \"" << JsonEscape(appearance.Serialize()) << "\",\n";
	ss << "  \"wardrobe\": [";
	for (size_t i = 0; i < appearance.clothing.size(); ++i) {
		const ClothingSlot& c = appearance.clothing[i];
		if (i) ss << ",";
		ss << "\n    { \"slot\": \"" << JsonEscape(c.slot)
			<< "\", \"item_id\": \"" << JsonEscape(c.item_id)
			<< "\", \"color_primary\": " << c.color_primary
			<< ", \"color_secondary\": " << c.color_secondary
			<< ", \"decal\": \"" << JsonEscape(c.decal_key) << "\" }";
	}
	ss << "\n  ],\n";
	ss << "  \"inventory\": [";
	for (size_t i = 0; i < inventory.slots.size(); ++i) {
		const InventorySlot& s = inventory.slots[i];
		if (i) ss << ",";
		ss << "\n    { \"item_id\": \"" << JsonEscape(s.item_id) << "\", \"count\": " << s.quantity << " }";
	}
	ss << "\n  ],\n";
	ss << "  \"cash\": " << profile.cash << ",\n";
	ss << "  \"g1c\": " << profile.g1c << ",\n";
	ss << "  \"threat_points\": " << FmtDouble(threat_points) << "\n";
	ss << "}\n";
	if (!WriteFile(CharacterPath(account, slot), ss.str())) return false;
	++character_write_count_;
	return true;
}

bool JsonDomainStore::LoadCharacter(const std::string& account, int32_t slot,
	CharacterProfile& profile, CharacterAppearance& appearance,
	Inventory& inventory, double& threat_points) const {
	if (!active_) return false;
	std::string text = ReadFile(CharacterPath(account, slot));
	if (text.empty()) return false;
	if (SplitObjects(text).empty()) return false;
	CharacterProfile p;
	p.name = JsonGetString(text, "name");
	if (p.name.empty()) return false;
	p.faction = FactionFromString(JsonGetString(text, "faction", "Criminal"));
	p.cash = (int64_t)JsonGetNumber(text, "cash", 10000);
	p.g1c = (int64_t)JsonGetNumber(text, "g1c", 500);
	double loaded_threat_points = JsonGetNumber(text, "threat_points", 0);
	CharacterAppearance loaded_appearance;
	std::string blob = JsonGetString(text, "appearance_blob");
	if (!blob.empty()) CharacterAppearance::Deserialize(blob, loaded_appearance);
	Inventory loaded_inventory;
	std::string inv = ExtractArrayBody(text, "inventory");
	for (const auto& obj : SplitObjects(inv)) {
		std::string item_id = JsonGetString(obj, "item_id");
		int32_t count = (int32_t)JsonGetNumber(obj, "count", 0);
		if (!item_id.empty() && count > 0) loaded_inventory.Grant(item_id, count);
	}
	profile = p;
	appearance = std::move(loaded_appearance);
	inventory = std::move(loaded_inventory);
	threat_points = loaded_threat_points;
	return true;
}

// ---------------------------------------------------------------- progression
// M15 per-character progression sidecar: characters/<account>_<slot>_progress.json
//   { "contact_standing": [ { "contact_id", "standing" } ],
//     "role_xp":          [ { "role_id", "xp" } ] }
// Kept as a sidecar (not folded into the character file) so the M5 character record and
// the M15 progression maps evolve independently and this addition never has to touch
// SaveCharacter's signature. Load tolerates a missing file (fresh char = empty progress).
// Both maps are emitted sorted by key for deterministic, diff-stable output.

std::string JsonDomainStore::ProgressPath(const std::string& account, int32_t slot) const {
	return dir_ + "/characters/" + Sanitize(account) + "_" + std::to_string(slot) + "_progress.json";
}

bool JsonDomainStore::HasProgress(const std::string& account, int32_t slot) const {
	if (!active_) return false;
	std::error_code ec;
	return std::filesystem::exists(ProgressPath(account, slot), ec);
}

bool JsonDomainStore::SaveProgress(const std::string& account, int32_t slot,
	const CharacterProgress& progress) const {
	if (!active_ || account.empty()) return false;
	std::vector<std::string> contacts;
	for (const auto& kv : progress.contact_standing) contacts.push_back(kv.first);
	std::sort(contacts.begin(), contacts.end());
	std::vector<std::string> roles;
	for (const auto& kv : progress.role_xp) roles.push_back(kv.first);
	std::sort(roles.begin(), roles.end());

	std::ostringstream ss;
	ss << "{\n  \"contact_standing\": [";
	for (size_t i = 0; i < contacts.size(); ++i) {
		if (i) ss << ",";
		ss << "\n    { \"contact_id\": \"" << JsonEscape(contacts[i])
			<< "\", \"standing\": " << progress.contact_standing.at(contacts[i]) << " }";
	}
	ss << "\n  ],\n  \"role_xp\": [";
	for (size_t i = 0; i < roles.size(); ++i) {
		if (i) ss << ",";
		ss << "\n    { \"role_id\": \"" << JsonEscape(roles[i])
			<< "\", \"xp\": " << progress.role_xp.at(roles[i]) << " }";
	}
	ss << "\n  ]\n}\n";
	return WriteFile(ProgressPath(account, slot), ss.str());
}

bool JsonDomainStore::LoadProgress(const std::string& account, int32_t slot,
	CharacterProgress& progress) const {
	if (!active_) return false;
	std::string text = ReadFile(ProgressPath(account, slot));
	if (text.empty()) return false; // tolerate-missing: caller keeps its (empty) progress
	std::string cs = ExtractArrayBody(text, "contact_standing");
	std::string rx = ExtractArrayBody(text, "role_xp");
	const std::vector<std::string> contact_objects = SplitObjects(cs);
	const std::vector<std::string> role_objects = SplitObjects(rx);
	if (contact_objects.empty() && role_objects.empty()) return false;
	CharacterProgress loaded;
	for (const auto& obj : contact_objects) {
		std::string id = JsonGetString(obj, "contact_id");
		if (id.empty()) continue;
		loaded.contact_standing[id] = (int64_t)JsonGetNumber(obj, "standing", 0);
	}
	for (const auto& obj : role_objects) {
		std::string id = JsonGetString(obj, "role_id");
		if (id.empty()) continue;
		loaded.role_xp[id] = (int64_t)JsonGetNumber(obj, "xp", 0);
	}
	if (loaded.contact_standing.empty() && loaded.role_xp.empty()) return false;
	progress = std::move(loaded);
	return true;
}

std::string JsonDomainStore::HandoffSnapshotPath(const std::string& account, int32_t slot) const {
	return dir_ + "/characters/" + Sanitize(account) + "_" + std::to_string(slot) + "_handoff.json";
}

bool JsonDomainStore::LoadHandoffSnapshot(const std::string& account, int32_t slot, std::string& snapshot_json) const {
	if (!active_) return false;
	snapshot_json = ReadFile(HandoffSnapshotPath(account, slot));
	return !snapshot_json.empty();
}

bool JsonDomainStore::SaveHandoffSnapshot(const std::string& account, int32_t slot, const std::string& snapshot_json) const {
	return active_ && !account.empty() && !snapshot_json.empty() && WriteFile(HandoffSnapshotPath(account, slot), snapshot_json);
}

// ---------------------------------------------------------------- auction
// Schema (§4): { "next_id", "listings": [ { "id", "seller_char", "item_id",
//   "count", "start_price", "buyout_price", "current_bid", "bidder_char",
//   "fee_paid", "created_utc", "expires_utc",
//   "state": "active|sold|expired|cancelled" } ] } — M12 bid/expiry/fee fields.

static const char* AuctionStateName(AuctionState s) {
	switch (s) {
		case AuctionState::Sold:      return "sold";
		case AuctionState::Expired:   return "expired";
		case AuctionState::Cancelled: return "cancelled";
		case AuctionState::Active:    default: return "active";
	}
}
static AuctionState AuctionStateFrom(const std::string& s, bool& active_out) {
	if (s == "sold")      { active_out = false; return AuctionState::Sold; }
	if (s == "expired")   { active_out = false; return AuctionState::Expired; }
	if (s == "cancelled") { active_out = false; return AuctionState::Cancelled; }
	if (s == "inactive")  { active_out = false; return AuctionState::Sold; } // legacy files
	active_out = true; return AuctionState::Active;
}

bool JsonDomainStore::SaveAuction(const AuctionHouse& auction) const {
	if (!active_) return false;
	std::ostringstream ss;
	ss << "{\n  \"next_id\": " << auction.next_id << ",\n  \"listings\": [";
	for (size_t i = 0; i < auction.listings.size(); ++i) {
		const AuctionListing& L = auction.listings[i];
		if (i) ss << ",";
		ss << "\n    { \"id\": " << L.listing_id
			<< ", \"seller_char\": \"" << JsonEscape(L.seller)
			<< "\", \"item_id\": \"" << JsonEscape(L.item_id)
			<< "\", \"count\": " << L.quantity
			<< ", \"start_price\": " << L.start_price
			<< ", \"buyout_price\": " << L.buyout_price
			<< ", \"current_bid\": " << L.current_bid
			<< ", \"bidder_char\": \"" << JsonEscape(L.high_bidder)
			<< "\", \"fee_paid\": " << L.fee_paid
			<< ", \"created_utc\": " << L.created_utc
			<< ", \"expires_utc\": " << L.expires_utc
			<< ", \"state\": \"" << AuctionStateName(L.state) << "\" }";
	}
	ss << "\n  ]\n}\n";
	return WriteFile(dir_ + "/auction.json", ss.str());
}

bool JsonDomainStore::LoadAuction(AuctionHouse& auction) const {
	if (!active_) return false;
	std::string text = ReadFile(dir_ + "/auction.json");
	if (text.empty()) return false;
	std::string body = ExtractArrayBody(text, "listings");
	const std::vector<std::string> objects = SplitObjects(body);
	if (objects.empty()) return false;
	AuctionHouse loaded = auction;
	loaded.listings.clear();
	for (const auto& obj : objects) {
		AuctionListing L;
		L.listing_id = (int64_t)JsonGetNumber(obj, "id", 0);
		L.seller = JsonGetString(obj, "seller_char");
		L.item_id = JsonGetString(obj, "item_id");
		L.quantity = (int32_t)JsonGetNumber(obj, "count", 1);
		L.start_price = (int64_t)JsonGetNumber(obj, "start_price", 0);
		L.buyout_price = (int64_t)JsonGetNumber(obj, "buyout_price", 0);
		L.current_bid = (int64_t)JsonGetNumber(obj, "current_bid", 0);
		L.high_bidder = JsonGetString(obj, "bidder_char");
		L.fee_paid = (int64_t)JsonGetNumber(obj, "fee_paid", 0);
		L.created_utc = (int64_t)JsonGetNumber(obj, "created_utc", 0);
		L.expires_utc = (int64_t)JsonGetNumber(obj, "expires_utc", 0);
		bool active = true;
		L.state = AuctionStateFrom(JsonGetString(obj, "state", "active"), active);
		L.active = active;
		if (L.listing_id <= 0 || L.item_id.empty()) continue;
		loaded.listings.push_back(L);
	}
	if (loaded.listings.empty()) return false;
	int64_t next = (int64_t)JsonGetNumber(text, "next_id", 1);
	for (const auto& L : loaded.listings) next = std::max(next, L.listing_id + 1);
	loaded.next_id = next;
	auction = std::move(loaded);
	return true;
}

// ---------------------------------------------------------------- mail
// Schema (§4 adapted): { "next_id", "messages": [ { "id", "to_char", "from",
//   "subject", "body", "attachments": [ { "item_id", "count", "cash" } ],
//   "read" } ] } — expires_utc omitted (no expiry logic in Domain yet).

bool JsonDomainStore::SaveMail(const MailService& mail) const {
	if (!active_) return false;
	std::ostringstream ss;
	ss << "{\n  \"next_id\": " << mail.next_mail_id << ",\n  \"messages\": [";
	for (size_t i = 0; i < mail.messages.size(); ++i) {
		const MailMessage& m = mail.messages[i];
		if (i) ss << ",";
		ss << "\n    { \"id\": " << m.id
			<< ", \"to_char\": \"" << JsonEscape(m.to)
			<< "\", \"from\": \"" << JsonEscape(m.from)
			<< "\", \"subject\": \"" << JsonEscape(m.subject)
			<< "\", \"body\": \"" << JsonEscape(m.body)
			<< "\", \"attachments\": [";
		for (size_t j = 0; j < m.attachments.size(); ++j) {
			const MailAttachment& a = m.attachments[j];
			if (j) ss << ",";
			ss << " { \"item_id\": \"" << JsonEscape(a.item_id)
				<< "\", \"count\": " << a.count << ", \"cash\": " << a.cash << " }";
		}
		ss << " ], \"read\": " << (m.read ? "true" : "false")
			<< ", \"claimed\": " << (m.claimed ? "true" : "false")
			<< ", \"created_utc\": " << m.created_utc << " }";
	}
	ss << "\n  ]\n}\n";
	return WriteFile(dir_ + "/mail.json", ss.str());
}

bool JsonDomainStore::LoadMail(MailService& mail) const {
	if (!active_) return false;
	std::string text = ReadFile(dir_ + "/mail.json");
	if (text.empty()) return false;
	std::string body = ExtractArrayBody(text, "messages");
	const std::vector<std::string> objects = SplitObjects(body);
	if (objects.empty()) return false;
	MailService loaded = mail;
	loaded.messages.clear();
	for (const auto& obj : objects) {
		MailMessage m;
		m.id = (int64_t)JsonGetNumber(obj, "id", 0);
		m.to = JsonGetString(obj, "to_char");
		m.from = JsonGetString(obj, "from");
		m.subject = JsonGetString(obj, "subject");
		m.body = JsonGetString(obj, "body");
		m.read = JsonGetBool(obj, "read", false);
		m.claimed = JsonGetBool(obj, "claimed", false);
		m.created_utc = (int64_t)JsonGetNumber(obj, "created_utc", 0);
		std::string att = ExtractArrayBody(obj, "attachments");
		for (const auto& aobj : SplitObjects(att)) {
			MailAttachment a;
			a.item_id = JsonGetString(aobj, "item_id");
			a.count = (int32_t)JsonGetNumber(aobj, "count", 0);
			a.cash = (int64_t)JsonGetNumber(aobj, "cash", 0);
			m.attachments.push_back(a);
		}
		if (m.id <= 0 || m.to.empty()) continue;
		loaded.messages.push_back(m);
	}
	if (loaded.messages.empty()) return false;
	int64_t next = (int64_t)JsonGetNumber(text, "next_id", 1);
	for (const auto& m : loaded.messages) next = std::max(next, m.id + 1);
	loaded.next_mail_id = next;
	mail = std::move(loaded);
	return true;
}

} // namespace apb
