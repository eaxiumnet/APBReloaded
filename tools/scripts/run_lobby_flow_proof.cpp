// Standalone proof: shipped WorldService login→char→district list/join.
// Built by tools/scripts/build_lobby_flow.ps1 (same Domain sources as game).
#include "../../Source/APBReloaded/Domain/APBWorldService.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace apb;

int main(int argc, char** argv) {
	const std::string data = (argc > 1) ? argv[1] : R"(D:\APBReloaded\Content\Data)";
	const std::string outPath = (argc > 2) ? argv[2]
		: R"(C:\Users\Support\AppData\Local\Temp\grok-goal-259c86d3b37e\implementer\lobby_flow.txt)";

	std::ofstream out(outPath, std::ios::trunc);
	auto line = [&](const std::string& s) {
		std::cout << s << "\n";
		out << s << "\n";
	};

	WorldService w;
	if (!w.InitFromDataDir(data)) {
		line("FAIL init");
		return 1;
	}
	line("OK init data=" + data);

	if (w.LoginAccount("nope", "x")) { line("FAIL expected login_fail"); return 2; }
	line("OK login_fail unknown");

	const std::string user = "lobby_proof_user";
	const std::string pass = "lobby_proof_pass";
	w.RegisterAccount(user, pass); // may fail if re-run; try login either way
	if (!w.LoginAccount(user, pass)) {
		// register fresh unique
		const std::string u2 = user + "_" + std::to_string(std::time(nullptr));
		if (!w.RegisterAccount(u2, pass) || !w.LoginAccount(u2, pass)) {
			line("FAIL register/login");
			return 3;
		}
		line("OK register+login user=" + u2);
	} else {
		line("OK login user=" + user);
	}

	if (!w.EnterWorld("W1")) { line("FAIL EnterWorld"); return 4; }
	line("OK EnterWorld W1 phase=" + std::to_string((int)w.phase));

	if (!w.CreateCharacter("ProofOp", Faction::Criminal)) { line("FAIL CreateCharacter"); return 5; }
	line("OK CreateCharacter ProofOp Criminal has_char=1");

	auto districts = w.ListDistricts();
	if (districts.empty()) { line("FAIL ListDistricts empty"); return 6; }
	line("OK ListDistricts count=" + std::to_string(districts.size()));
	for (size_t i = 0; i < districts.size() && i < 8; ++i) {
		line("  district id=" + districts[i].id + " name=" + districts[i].name);
	}

	const std::string did = districts[0].id;
	if (!w.JoinDistrict(did, "ProofOp")) { line("FAIL JoinDistrict " + did); return 7; }
	line("OK JoinDistrict id=" + did + " session=" + (w.district ? w.district->session_id : ""));
	line("OK phase_district=" + std::to_string(w.phase == SessionPhase::District ? 1 : 0));
	line("LOBBY_FLOW_PASS=1");
	return 0;
}
