// C2 RED test suite — auth API (APBCrypto + APBTicket; C3/C4/C5 implement these).
// Compile: cl /nologo /EHsc /std:c++17
//          /I"D:\APBReloaded\Source\APBReloaded\Domain" /c run_auth_tests.cpp
// Link errors for missing symbols are EXPECTED until C3-C5 land.
//
// Guard map:
//   APB_AUTH_V2          -- C5 adds AccountRecord::password_salt + salted-hash LoginService
//   APB_TICKET_AVAILABLE -- C4 creates APBTicket.h + TicketService

#include "../Source/APBReloaded/Domain/APBSocial.h"
#include "../Source/APBReloaded/Domain/APBCrypto.h"

// TODO: C3+C4 implements APBCrypto.h + APBTicket.h
// TODO: C5 adds password_salt to AccountRecord in APBSocial.h
#ifdef APB_TICKET_AVAILABLE
#include "../Source/APBReloaded/Domain/APBTicket.h"
#endif

#include <iostream>
#include <stdexcept>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

#ifdef APB_TICKET_AVAILABLE
static const std::string kTicketTestSecret(64, 'a');

std::string EncodeTicketPayload(const std::string& payload) {
    static constexpr char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string encoded;
    encoded.reserve((payload.size() * 4 + 2) / 3);
    unsigned buffer = 0;
    int bits = 0;
    for (unsigned char c : payload) {
        buffer = (buffer << 8) | c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            encoded += chars[(buffer >> bits) & 0x3f];
        }
    }
    if (bits > 0) encoded += chars[(buffer << (6 - bits)) & 0x3f];
    return encoded;
}

std::string SignTicketPayload(const std::string& payload_encoded) {
    const auto key = hex_decode(kTicketTestSecret);
    const std::string signature = hmac_sha256_hex(
        key.data(), key.size(),
        reinterpret_cast<const uint8_t*>(payload_encoded.data()), payload_encoded.size());
    return payload_encoded + "." + signature;
}
#endif

// --- Test 1: Salted hash stored (not plaintext, 16-byte hex salt) ----------
#ifdef APB_AUTH_V2
void TestSaltedHashStored() {
    // C5: Register() stores password_salt (32 hex chars = 16 bytes).
    // password_hash must NOT equal the plaintext password.
    LoginService svc;
    CHECK(svc.Register("alice", "hunter2"), "T1: register alice");
    const auto& rec = svc.accounts.at("alice");
    CHECK(rec.password_salt.size() == 32, "T1: salt is 32 hex chars (16 bytes)");
    CHECK(rec.password_hash != "hunter2", "T1: hash is not plaintext");
    CHECK(!rec.password_salt.empty(), "T1: salt non-empty");
    // Every account must get a unique salt even with the same plaintext password.
    CHECK(svc.Register("bob", "hunter2"), "T1: register bob (same pass)");
    const auto& recB = svc.accounts.at("bob");
    CHECK(rec.password_salt != recB.password_salt, "T1: unique salt per account");
}
#endif // APB_AUTH_V2

// --- Test 2: Login hashed OK / wrong-pass / banned -------------------------
#ifdef APB_AUTH_V2
void TestLoginHashedPaths() {
    // C5: Login() derives hash from stored salt -- never compares plaintext.
    LoginService svc;
    CHECK(svc.Register("carol", "correct"), "T2: register carol");
    CHECK(svc.Login("carol", "correct"), "T2: login_ok hashed");
    svc.Logout();
    CHECK(!svc.Login("carol", "wrong"), "T2: login_fail wrong password");
    CHECK(!svc.Login("nobody", "x"),    "T2: login_fail unknown user");
    svc.accounts["carol"].banned = true;
    CHECK(!svc.Login("carol", "correct"), "T2: login_fail banned");
}
#endif // APB_AUTH_V2

// --- Test 3: Legacy-plaintext migrate-on-login -----------------------------
#ifdef APB_AUTH_V2
void TestLegacyMigrate() {
    // C5: accounts with password_salt=="" are legacy (plaintext password_hash).
    // Login() detects the sentinel and uses the legacy path, then migrates in-place.
    LoginService svc;
    AccountRecord legacy;
    legacy.account_id    = "ACC-legacy";
    legacy.username      = "legacy_user";
    legacy.password_hash = "plaintextpw"; // old plaintext store
    legacy.password_salt = "";            // sentinel: no salt = legacy
    legacy.banned        = false;
    svc.accounts["legacy_user"] = legacy;

    // First login: legacy path succeeds, record is migrated in-place.
    CHECK(svc.Login("legacy_user", "plaintextpw"), "T3: legacy login succeeds");
    const auto& migrated = svc.accounts.at("legacy_user");
    CHECK(migrated.password_salt.size() == 32, "T3: post-migrate salt is 32 hex chars");
    CHECK(migrated.password_hash != "plaintextpw", "T3: post-migrate hash not plaintext");
    svc.Logout();

    // Second login must use the new salted hash -- plaintext path no longer taken.
    CHECK(svc.Login("legacy_user", "plaintextpw"), "T3: post-migrate login still works");
    svc.Logout();
    CHECK(!svc.Login("legacy_user", "wrongpw"), "T3: post-migrate wrong password fails");
}
#endif // APB_AUTH_V2

// --- Test 4: Ticket issue->verify field parity + tamper-fail ---------------
#ifdef APB_TICKET_AVAILABLE
// TODO: C4 creates APBTicket.h (TicketService, TicketClaims, IssueTicket,
//       VerifyTicket, ConsumeJti). Format: compact "payload.signature" HMAC token.
void TestTicketIssueVerify() {
    TicketService& ts = TicketService::Global();
    TicketClaims req;
    req.account     = "ACC-alice";
    req.character   = "Alice";
    req.faction     = "Criminal";
    req.district    = "Financial";
    req.expiry_secs = 300;

    std::string token = ts.IssueTicket(req);
    CHECK(!token.empty(), "T4: ticket issued non-empty");
    CHECK(token.find('.') != std::string::npos, "T4: payload.signature separator present");

    TicketClaims out;
    bool ok = ts.VerifyTicket(token, out);
    CHECK(ok, "T4: ticket verifies");
    CHECK(out.account   == req.account,   "T4: account parity");
    CHECK(out.character == req.character, "T4: character parity");
    CHECK(out.faction   == req.faction,   "T4: faction parity");
    CHECK(out.district  == req.district,  "T4: district parity");
    CHECK(!out.jti.empty(), "T4: jti non-empty");

    std::string tampered = token;
    tampered[0] = (tampered[0] == 'A') ? 'B' : 'A';
    TicketClaims tOut;
    CHECK(!ts.VerifyTicket(tampered, tOut), "T4: tampered ticket rejected");
}
#endif // APB_TICKET_AVAILABLE

// --- Test 5: Ticket replay blocked after ConsumeJti ------------------------
#ifdef APB_TICKET_AVAILABLE
void TestTicketReplayBlocked() {
    TicketService& ts = TicketService::Global();
    TicketClaims req;
    req.account     = "ACC-bob";
    req.character   = "Bob";
    req.faction     = "Enforcer";
    req.district    = "Waterfront";
    req.expiry_secs = 300;

    std::string token = ts.IssueTicket(req);
    CHECK(!token.empty(), "T5: replay token issued");

    TicketClaims first;
    CHECK(ts.VerifyTicket(token, first), "T5: first verify ok");
    CHECK(ts.ConsumeJti(first.jti), "T5: ConsumeJti accepted");

    TicketClaims second;
    CHECK(!ts.VerifyTicket(token, second), "T5: second verify blocked (replay)");
}
#endif // APB_TICKET_AVAILABLE

#ifdef APB_TICKET_AVAILABLE
void TestJsonIntOverflowContained() {
    // Given: a correctly signed ticket whose network-supplied issued field exceeds int64_t.
    TicketService& ts = TicketService::Global();
    ts.SetSecret(kTicketTestSecret);
    const std::string payload =
        "{\"account\":\"ACC-hostile\",\"character\":\"Hostile\","
        "\"faction\":\"Criminal\",\"district\":\"Financial\","
        "\"jti\":\"overflow-jti\",\"issued\":999999999999999999999999,\"expiry\":300}";
    const std::string token = SignTicketPayload(EncodeTicketPayload(payload));

    // When: the hostile ticket crosses the verification boundary.
    TicketClaims out;
    bool refused = false;
    try {
        refused = !ts.VerifyTicket(token, out);
    } catch (const std::invalid_argument&) {
        refused = false;
    } catch (const std::out_of_range&) {
        refused = false;
    }

    // Then: decoding produces a typed refusal rather than an exception or valid defaults.
    CHECK(refused, "TestJsonIntOverflowContained: overflowing integer ticket refused");
}
#endif // APB_TICKET_AVAILABLE

#ifdef APB_TICKET_AVAILABLE
void TestTicketPayloadEscaped() {
    TicketService& ts = TicketService::Global();
    ts.SetSecret(kTicketTestSecret);
    TicketClaims req;
    req.account = "ACC-\"hostile\\account\nline";
    req.character = "Char\t\"quoted\"";
    req.faction = "Criminal\\Enforcer";
    req.district = "Financial\rDistrict";
    req.expiry_secs = 300;

    const std::string token = ts.IssueTicket(req);
    TicketClaims out;
    const bool verified = ts.VerifyTicket(token, out);

    CHECK(verified &&
          out.account == req.account &&
          out.character == req.character &&
          out.faction == req.faction &&
          out.district == req.district,
          "TestTicketPayloadEscaped: hostile strings round-trip exactly");
}
#endif // APB_TICKET_AVAILABLE

void TestSecretMaterialRejected() {
    CHECK(!is_valid_secret_material(""), "T6: empty secret material rejected");
    CHECK(!is_valid_secret_material("0123456789abcdef"), "T6: too-short secret material rejected");
    CHECK(!is_valid_secret_material(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"),
        "T6: non-hex secret material rejected");
    CHECK(is_valid_secret_material(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
        "T6: 32-byte hex secret material accepted");
}

int main() {
    std::cout << "APB Auth tests (C2 RED suite)\n";

#ifdef APB_AUTH_V2
    TestSaltedHashStored();
    TestLoginHashedPaths();
    TestLegacyMigrate();
#else
    std::cout << "NOTE: APB_AUTH_V2 not defined -- tests 1-3 skipped (C5 implements)\n";
#endif

#ifdef APB_TICKET_AVAILABLE
    TestTicketIssueVerify();
    TestTicketReplayBlocked();
    TestJsonIntOverflowContained();
    TestTicketPayloadEscaped();
#else
    std::cout << "NOTE: APB_TICKET_AVAILABLE not defined -- tests 4-5 skipped (C4 implements)\n";
#endif

    TestSecretMaterialRejected();

    std::cout << "FAILS=" << fails << "\n";
    return fails ? 1 : 0;
}
