#pragma once
// APBMailClaimJournal.h — M14 S10: durable journal for the two-aggregate mail claim.
//
// A claim mutates two things the world owns separately: the character's cash and
// the mail message's claimed flag. A crash between them must never lose or
// duplicate a credit, so every transition is journalled to disk before the next
// one starts, and the CHARACTER RECEIPT is the idempotency fact on recovery.
//
// The key is {character, mail_id}, deliberately NOT an operation_id: an
// operation id neither survives a world restart nor dedups a retry re-issued
// after district travel with a freshly generated id.
//
// Engine-free C++17. The caller supplies both the directory and the clock so the
// Domain never constructs a UE path and stays deterministic under test.
#include <cstdint>
#include <string>
#include <vector>

namespace apb {

enum class MailClaimState {
	Prepared,           // journalled; neither aggregate has moved yet
	CharacterCommitted, // cash credited and durably receipted
	MailCommitted       // mail flag committed; terminal
};

struct MailClaimReceipt {
	std::string character;
	int64_t mail_id = 0;
	MailClaimState state = MailClaimState::Prepared;
	int64_t cash_delta = 0;
	int64_t claimed_utc = 0;
};

class MailClaimJournal {
public:
	// Creates <dir> if needed and activates the journal. Empty dir -> inactive/false.
	// Does not read the file; call Load() explicitly so a caller can distinguish a
	// fresh start from a populated one.
	bool Init(const std::string& dir);
	bool IsActive() const { return active_; }
	const std::string& Dir() const { return dir_; }
	std::string Path() const;

	// false when the file is missing or holds no receipts (fresh start).
	bool Load();
	bool Save() const;

	size_t Count() const { return receipts_.size(); }
	const MailClaimReceipt* Find(const std::string& character, int64_t mail_id) const;

	// Transitions. Each one persists immediately and refuses any out-of-order or
	// repeated call, so a replayed claim can never advance a receipt twice.
	bool Prepare(const std::string& character, int64_t mail_id, int64_t cash_delta, int64_t now_utc);
	bool CommitCharacter(const std::string& character, int64_t mail_id);
	bool CommitMail(const std::string& character, int64_t mail_id);

	// Recovery predicates. CashAlreadyCredited is the idempotency fact: true from
	// CharacterCommitted onward, so recovery knows never to credit a second time.
	bool CashAlreadyCredited(const std::string& character, int64_t mail_id) const;
	bool MailAlreadyCommitted(const std::string& character, int64_t mail_id) const;

	// Drives defect-6 handoff reconciliation: every receipt whose cash was already
	// credited, so a stale district snapshot cannot revert a committed credit.
	std::vector<MailClaimReceipt> CommittedReceiptsFor(const std::string& character) const;

private:
	MailClaimReceipt* Mutable(const std::string& character, int64_t mail_id);

	std::string dir_;
	bool active_ = false;
	std::vector<MailClaimReceipt> receipts_;
};

} // namespace apb
