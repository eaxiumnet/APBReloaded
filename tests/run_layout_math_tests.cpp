// Drives shipped APBFrontendLayoutMath.h (same header used by UAPBFrontendWidget).
#include "../Source/APBReloaded/Systems/Frontend/APBFrontendLayoutMath.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace apb_layout;

static int fails = 0;
#define CHECK(c, msg) do { if(!(c)){ std::fprintf(stderr,"FAIL: %s\n", msg); ++fails; } else { std::printf("PASS: %s\n", msg); } } while(0)

static bool Near(float A, float B, float Eps = 0.5f) { return std::fabs(A - B) <= Eps; }

int main(int argc, char** argv)
{
	const std::string Content = (argc > 1) ? argv[1] : R"(D:\APBReloaded\Content)";
	const std::string OutPath = (argc > 2) ? argv[2]
		: R"(C:\Users\Support\AppData\Local\Temp\grok-goal-4381756b8529\implementer\scale_path.txt)";
	std::ofstream out(OutPath, std::ios::trunc);

	// Login fixed size (menu2011_spec §3: 2011 Login_Scene window, compact returning-user state)
	float Lw = 0, Lh = 0;
	DesignPanelSize("Login", Lw, Lh);
	CHECK(Lw > 0 && Lh > 0, "login design size positive");
	CHECK(Near(Lw, 1006.f) && Near(Lh, 480.f), "login design 1006x480");
	CHECK(!LoginAllowsScroll(), "login never allows scroll");

	// First-run TOS state is the same width, taller window (spec §3.2 State A)
	float Tw = 0, Th = 0;
	DesignPanelSize("LoginTOS", Tw, Th);
	CHECK(Near(Tw, 1006.f) && Near(Th, 898.f), "login TOS design 1006x898");

	// Scale Fit 16:9 1920x1080 -> identity
	float Sx, Sy, Uni, Pw, Ph;
	ScaledPanelSize("Login", 1920.f, 1080.f, ScaleMode::Fit, Pw, Ph);
	CHECK(Near(Pw, 1006.f) && Near(Ph, 480.f), "login 1080p fit = design size");

	// 4:3 1280x960 — uniform fit scale = min(1280/1920, 960/1080) = min(0.666, 0.888) = 0.666
	ScaledPanelSize("Login", 1280.f, 960.f, ScaleMode::Fit, Pw, Ph);
	ComputeUiScale(1280.f, 960.f, ScaleMode::Fit, Sx, Sy, Uni);
	CHECK(Uni > 0.5f && Uni < 1.f, "4:3 fit scale in range");
	CHECK(Near(Pw, 1006.f * Uni) && Near(Ph, 480.f * Uni), "login scales uniformly on 4:3");
	out << "4:3 fit uni=" << Uni << " panel=" << Pw << "x" << Ph << "\n";

	// 16:10 1920x1200
	ScaledPanelSize("Login", 1920.f, 1200.f, ScaleMode::Fit, Pw, Ph);
	ComputeUiScale(1920.f, 1200.f, ScaleMode::Fit, Sx, Sy, Uni);
	CHECK(Near(Uni, 1.f), "16:10 1920x1200 fit uni ~1 (height larger)");
	out << "16:10 fit uni=" << Uni << " panel=" << Pw << "x" << Ph << "\n";

	// Stretch vs Fit differ on ultrawide
	float PwF, PhF, PwS, PhS;
	ScaledPanelSize("Login", 2560.f, 1080.f, ScaleMode::Fit, PwF, PhF);
	ScaledPanelSize("Login", 2560.f, 1080.f, ScaleMode::Stretch, PwS, PhS);
	CHECK(PwS > PwF || Near(PwS, PwF), "stretch width >= fit width on ultrawide");
	out << "ultrawide fit=" << PwF << "x" << PhF << " stretch=" << PwS << "x" << PhS << "\n";

	// Logo tracks panel (2011 LoadingScreen_APB 256×128)
	float LogoW, LogoH;
	LogoSizeFromPanelWidth(400.f, LogoW, LogoH);
	CHECK(LogoW > 0 && LogoH > 0, "logo size positive");
	CHECK(Near(LogoH / LogoW, kLogoAspect, 0.05f), "logo keeps 256:128 aspect");
	out << "logo_from_400w=" << LogoW << "x" << LogoH << "\n";

	// Stage beds exist on disk (same relative paths as widget resolver prefers)
	const char* Stages[][2] = {
		{ "Login", "0" },
		{ "CharacterSelect", "0" },
		{ "CharacterCreate", "0" },
		{ "CharacterCreate", "1" },
		{ "DistrictSelect", "0" },
	};
	std::ofstream beds(
		(argc > 3) ? argv[3]
		: R"(C:\Users\Support\AppData\Local\Temp\grok-goal-4381756b8529\implementer\stage_beds.txt)",
		std::ios::trunc);
	for (auto& Row : Stages)
	{
		const bool bEnf = Row[1][0] == '1';
		const char* Rel = StageBedRelative(Row[0], bEnf);
		fs::path P = fs::path(Content) / Rel;
		const bool Ok = fs::exists(P);
		CHECK(Ok, Rel);
		beds << Row[0] << " enf=" << (bEnf ? 1 : 0) << " path=" << P.string() << " exists=" << (Ok ? 1 : 0) << "\n";
	}

	// Staged 2011 assets the widget now references (M4a import)
	fs::path Logo = fs::path(Content) / "Imported/UI/Menu2011/Loading/LoadingScreen_APB.uasset";
	CHECK(fs::exists(Logo), "LoadingScreen_APB.uasset staged");
	fs::path Chrome = fs::path(Content) / "Imported/UI/Menu2011/Chrome/MessageBox_BG.uasset";
	CHECK(fs::exists(Chrome), "MessageBox_BG.uasset staged");
	fs::path Sfx = fs::path(Content) / "Audio/UI/ButtonPos.uasset";
	CHECK(fs::exists(Sfx), "ButtonPos.uasset staged");
	out << "logo_asset=" << Logo.string() << "\n";
	out << "FAILS=" << fails << "\n";
	beds << "FAILS=" << fails << "\n";

	std::printf("LAYOUT_MATH FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
