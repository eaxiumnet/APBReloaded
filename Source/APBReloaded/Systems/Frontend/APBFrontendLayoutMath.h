#pragma once
/**
 * Pure layout math for 2011-style GameFlow lobby UI.
 * Shared by UAPBFrontendWidget and standalone tests (no UObject).
 * Design space: 1920×1080. Login is fixed (non-scroll).
 */
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace apb_layout
{
static constexpr float kDesignW = 1920.f;
static constexpr float kDesignH = 1080.f;
static constexpr float kLogoAspect = 128.f / 256.f; // LoadingScreen_APB (2011 RTW logo, 256×128)

enum class ScaleMode : int { Fit = 0, Fill = 1, Stretch = 2 };

/** Design-space dialog size (before viewport scale). Login must fit without scroll.
 *  Geometry per work\menu2011_spec.md §3–5 (measured from 2011 UIScene previews). */
inline void DesignPanelSize(const char* StageToken, float& OutW, float& OutH)
{
	if (!StageToken) { OutW = 440.f; OutH = 400.f; return; }
	// 2011 Login_Scene window: preview-measured ~52% screen width. Compact = returning
	// user (credentials only); LoginTOS = first-run with scrollable TOS body.
	if (std::strcmp(StageToken, "Login") == 0) { OutW = 1006.f; OutH = 480.f; return; }
	if (std::strcmp(StageToken, "LoginTOS") == 0) { OutW = 1006.f; OutH = 898.f; return; }
	// Lobby_Scene: left char list + name plate + play bar
	if (std::strcmp(StageToken, "CharacterSelect") == 0) { OutW = 640.f; OutH = 560.f; return; }
	if (std::strcmp(StageToken, "CharacterCreate") == 0) { OutW = 560.f; OutH = 640.f; return; }
	// District rows with 256×195 photos
	if (std::strcmp(StageToken, "DistrictSelect") == 0) { OutW = 900.f; OutH = 560.f; return; }
	if (std::strcmp(StageToken, "Settings") == 0) { OutW = 500.f; OutH = 520.f; return; }
	if (std::strcmp(StageToken, "Loading") == 0) { OutW = 460.f; OutH = 260.f; return; }
	OutW = 440.f;
	OutH = 400.f;
}

inline void ClampScale(float& S)
{
	if (S < 0.55f) S = 0.55f;
	if (S > 1.75f) S = 1.75f;
}

/**
 * Compute UI scale from viewport vs design space.
 * Fit = min (safe 4:3/16:9/16:10), Fill = max, Stretch = independent axes.
 */
inline void ComputeUiScale(float ViewportX, float ViewportY, ScaleMode Mode,
	float& OutScaleX, float& OutScaleY, float& OutUniform)
{
	const float Sx = (ViewportX > 1.f) ? (ViewportX / kDesignW) : 1.f;
	const float Sy = (ViewportY > 1.f) ? (ViewportY / kDesignH) : 1.f;
	float Uni = 1.f;
	float X = 1.f, Y = 1.f;
	switch (Mode)
	{
	case ScaleMode::Fill:
		Uni = (Sx > Sy) ? Sx : Sy;
		X = Y = Uni;
		break;
	case ScaleMode::Stretch:
		X = Sx; Y = Sy;
		Uni = (Sx < Sy) ? Sx : Sy;
		break;
	case ScaleMode::Fit:
	default:
		Uni = (Sx < Sy) ? Sx : Sy;
		X = Y = Uni;
		break;
	}
	ClampScale(Uni);
	ClampScale(X);
	ClampScale(Y);
	OutScaleX = X;
	OutScaleY = Y;
	OutUniform = Uni;
}

/** Final panel pixel size for a stage + viewport + mode. */
inline void ScaledPanelSize(const char* StageToken, float ViewportX, float ViewportY, ScaleMode Mode,
	float& OutW, float& OutH)
{
	float Dw = 0.f, Dh = 0.f;
	DesignPanelSize(StageToken, Dw, Dh);
	float Sx = 1.f, Sy = 1.f, Uni = 1.f;
	ComputeUiScale(ViewportX, ViewportY, Mode, Sx, Sy, Uni);
	if (Mode == ScaleMode::Stretch)
	{
		OutW = Dw * Sx;
		OutH = Dh * Sy;
	}
	else
	{
		OutW = Dw * Uni;
		OutH = Dh * Uni;
	}
}

/** Logo size tracks panel width at LoadingScreen_APB 256×128 (2:1) aspect. */
inline void LogoSizeFromPanelWidth(float PanelW, float& OutLogoW, float& OutLogoH)
{
	float W = PanelW * 0.88f;
	if (W < 220.f) W = 220.f;
	if (W > 520.f) W = 520.f;
	float H = W * kLogoAspect; // 110..260 — always on-aspect
	OutLogoW = W;
	OutLogoH = H;
}

/** Relative movie filenames preferred first (compat H.264). */
inline const char* StageBedRelative(const char* StageToken, bool bEnforcerFaction)
{
	if (!StageToken) return "Movies/Login/Login_BG_AI_compat.mp4";
	if (std::strcmp(StageToken, "CharacterSelect") == 0)
		return "Movies/Login/Character_Select_BG_AI_compat.mp4";
	if (std::strcmp(StageToken, "CharacterCreate") == 0)
		return bEnforcerFaction
			? "Movies/Login/Faction_Enforcer_BG_AI_compat.mp4"
			: "Movies/Login/Faction_Criminal_BG_AI_compat.mp4";
	if (std::strcmp(StageToken, "DistrictSelect") == 0
		|| std::strcmp(StageToken, "Settings") == 0
		|| std::strcmp(StageToken, "Loading") == 0)
		return "Movies/Login/Generic_BG_AI_compat.mp4";
	// Login / Splash
	return "Movies/Login/Login_BG_AI_compat.mp4";
}

inline bool LoginAllowsScroll() { return false; }
}
