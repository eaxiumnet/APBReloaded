// APBMailClaimJournal.cpp — M14 S10 durable mail-claim journal.
//
// Deliberately independent of JsonDomainStore: this file owns its own small
// schema (mail_claims.json) and does not touch the mail serializer, so there is
// still exactly one implementation of mail JSON in the codebase.
#include "APBMailClaimJournal.h"
#include <cstdio>
#include <filesystem>

namespace apb {
namespace {

std::string ReadFile(const std::string& path) {
	std::FILE* f = nullptr;
	if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return {};
	std::string out;
	char buf[4096];
	size_t n = 0;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
	std::fclose(f);
	return out;
}

// Temp-then-rename so a crash mid-write cannot leave a truncated journal: the
// old file stays intact until the replacement is fully on disk.
bool WriteFile(const std::string& path, const std::string& text) {
	const std::string tmp = path + ".tmp";
	std::FILE* f = nullptr;
	if (fopen_s(&f, tmp.c_str(), "wb") != 0 || !f) return false;
	const size_t written = std::fwrite(text.data(), 1, text.size(), f);
	std::fclose(f);
	if (written != text.size()) return false;
	std::error_code ec;
	std::filesystem::rename(std::filesystem::path(tmp), std::filesystem::path(path), ec);
	if (ec) {
		std::filesystem::remove(std::filesystem::path(path), ec);
		std::filesystem::rename(std::filesystem::path(tmp), std::filesystem::path(path), ec);
	}
	return !ec;
}

const char* StateToken(MailClaimState s) {
	switch (s) {
		case MailClaimState::Prepared:           return "Prepared";
		case MailClaimState::CharacterCommitted: return "CharacterCommitted";
		case MailClaimState::MailCommitted:      return "MailCommitted";
	}
	return "Prepared";
}

bool StateFromToken(const std::string& t, MailClaimState& out) {
	if (t == "Prepared")           { out = MailClaimState::Prepared;           return true; }
	if (t == "CharacterCommitted") { out = MailClaimState::CharacterCommitted; return true; }
	if (t == "MailCommitted")      { out = MailClaimState::MailCommitted;      return true; }
	return false;
}

std::string JsonEscape(const std::string& in) {
	std::string out;
	for (char c : in) {
		if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
		else if (c == '\n') out += "\\n";
		else if (c == '\r') out += "\\r";
		else if (c == '\t') out += "\\t";
		else out.push_back(c);
	}
	return out;
}

std::string JsonUnescape(const std::string& in) {
	std::string out;
	for (size_t i = 0; i < in.size(); ++i) {
		if (in[i] != '\\' || i + 1 >= in.size()) { out.push_back(in[i]); continue; }
		const char n = in[++i];
		if (n == 'n') out.push_back('\n');
		else if (n == 'r') out.push_back('\r');
		else if (n == 't') out.push_back('\t');
		else out.push_back(n);
	}
	return out;
}

// Field readers scoped to a single {...} record slice. Deliberately minimal: the
// journal owns this file exclusively, so the schema is fixed and flat.
bool FieldString(const std::string& rec, const std::string& key, std::string& out) {
	const std::string pat = "\"" + key + "\":\"";
	const size_t at = rec.find(pat);
	if (at == std::string::npos) return false;
	size_t i = at + pat.size();
	std::string raw;
	for (; i < rec.size(); ++i) {
		if (rec[i] == '\\' && i + 1 < rec.size()) { raw.push_back(rec[i]); raw.push_back(rec[i + 1]); ++i; continue; }
		if (rec[i] == '"') break;
		raw.push_back(rec[i]);
	}
	out = JsonUnescape(raw);
	return true;
}

bool FieldInt(const std::string& rec, const std::string& key, int64_t& out) {
	const std::string pat = "\"" + key + "\":";
	const size_t at = rec.find(pat);
	if (at == std::string::npos) return false;
	size_t i = at + pat.size();
	while (i < rec.size() && (rec[i] == ' ' || rec[i] == '\t')) ++i;
	bool neg = false;
	if (i < rec.size() && (rec[i] == '-' || rec[i] == '+')) { neg = rec[i] == '-'; ++i; }
	if (i >= rec.size() || rec[i] < '0' || rec[i] > '9') return false;
	int64_t v = 0;
	for (; i < rec.size() && rec[i] >= '0' && rec[i] <= '9'; ++i) v = v * 10 + (rec[i] - '0');
	out = neg ? -v : v;
	return true;
}

} // namespace

bool MailClaimJournal::Init(const std::string& dir) {
	if (dir.empty()) { active_ = false; return false; }
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(dir), ec);
	dir_ = dir;
	active_ = true;
	return true;
}

std::string MailClaimJournal::Path() const { return dir_ + "/mail_claims.json"; }

const MailClaimReceipt* MailClaimJournal::Find(const std::string& character, int64_t mail_id) const {
	for (const MailClaimReceipt& r : receipts_)
		if (r.mail_id == mail_id && r.character == character) return &r;
	return nullptr;
}

MailClaimReceipt* MailClaimJournal::Mutable(const std::string& character, int64_t mail_id) {
	for (MailClaimReceipt& r : receipts_)
		if (r.mail_id == mail_id && r.character == character) return &r;
	return nullptr;
}

bool MailClaimJournal::Load() {
	receipts_.clear();
	if (!active_) return false;
	const std::string text = ReadFile(Path());
	if (text.empty()) return false;
	size_t at = 0;
	while ((at = text.find('{', at)) != std::string::npos) {
		const size_t end = text.find('}', at);
		if (end == std::string::npos) break;
		const std::string rec = text.substr(at, end - at + 1);
		at = end + 1;
		MailClaimReceipt r;
		std::string state_token;
		if (!FieldString(rec, "character", r.character)) continue;
		if (!FieldInt(rec, "mail_id", r.mail_id)) continue;
		if (!FieldString(rec, "state", state_token)) continue;
		if (!StateFromToken(state_token, r.state)) continue;
		FieldInt(rec, "cash_delta", r.cash_delta);
		FieldInt(rec, "claimed_utc", r.claimed_utc);
		if (r.character.empty() || Find(r.character, r.mail_id)) continue;
		receipts_.push_back(r);
	}
	return !receipts_.empty();
}

bool MailClaimJournal::Save() const {
	if (!active_) return false;
	std::string out = "{\"claims\":[";
	for (size_t i = 0; i < receipts_.size(); ++i) {
		const MailClaimReceipt& r = receipts_[i];
		if (i) out += ",";
		out += "{\"character\":\"" + JsonEscape(r.character) + "\"";
		out += ",\"mail_id\":" + std::to_string(r.mail_id);
		out += ",\"state\":\"" + std::string(StateToken(r.state)) + "\"";
		out += ",\"cash_delta\":" + std::to_string(r.cash_delta);
		out += ",\"claimed_utc\":" + std::to_string(r.claimed_utc);
		out += "}";
	}
	out += "]}";
	return WriteFile(Path(), out);
}

bool MailClaimJournal::Prepare(const std::string& character, int64_t mail_id,
	int64_t cash_delta, int64_t now_utc) {
	if (!active_ || character.empty() || mail_id <= 0) return false;
	if (Find(character, mail_id)) return false;
	MailClaimReceipt r;
	r.character = character;
	r.mail_id = mail_id;
	r.state = MailClaimState::Prepared;
	r.cash_delta = cash_delta;
	r.claimed_utc = now_utc;
	receipts_.push_back(r);
	if (!Save()) { receipts_.pop_back(); return false; }
	return true;
}

bool MailClaimJournal::CommitCharacter(const std::string& character, int64_t mail_id) {
	MailClaimReceipt* r = Mutable(character, mail_id);
	if (!active_ || !r || r->state != MailClaimState::Prepared) return false;
	r->state = MailClaimState::CharacterCommitted;
	if (!Save()) { r->state = MailClaimState::Prepared; return false; }
	return true;
}

bool MailClaimJournal::CommitMail(const std::string& character, int64_t mail_id) {
	MailClaimReceipt* r = Mutable(character, mail_id);
	if (!active_ || !r || r->state != MailClaimState::CharacterCommitted) return false;
	r->state = MailClaimState::MailCommitted;
	if (!Save()) { r->state = MailClaimState::CharacterCommitted; return false; }
	return true;
}

bool MailClaimJournal::CashAlreadyCredited(const std::string& character, int64_t mail_id) const {
	const MailClaimReceipt* r = Find(character, mail_id);
	return r && r->state != MailClaimState::Prepared;
}

bool MailClaimJournal::MailAlreadyCommitted(const std::string& character, int64_t mail_id) const {
	const MailClaimReceipt* r = Find(character, mail_id);
	return r && r->state == MailClaimState::MailCommitted;
}

std::vector<MailClaimReceipt> MailClaimJournal::CommittedReceiptsFor(const std::string& character) const {
	std::vector<MailClaimReceipt> out;
	for (const MailClaimReceipt& r : receipts_)
		if (r.character == character && r.state != MailClaimState::Prepared) out.push_back(r);
	return out;
}

} // namespace apb
