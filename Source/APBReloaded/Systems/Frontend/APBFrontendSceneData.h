#pragma once
// GENERATED from APBMenus_GameFlowScenes.upk UIScene docking data. Do not hand-edit.
// Regenerate: tools/gen_scene_data.py from Content/Extracted/2011/.../login_lobby_layout.json
namespace apb_scene {
struct FRectDef { const char* Name; float X, Y, W, H; };
struct FBgDef { const char* Name; float L, T, R, B; bool LPct, TPct, RPct, BPct; };
// ---- Login (design 1152x720) ----
static constexpr float LoginDesignW = 1152.f;
static constexpr float LoginDesignH = 720.f;
static const FRectDef LoginRects[] = {
    { "UIImage_APBlogo", 316.0f, 72.0f, 520.0f, 180.0f },
    { "UIPanel_EULA_MainPanel", 316.0f, 272.0f, 520.0f, 175.0f },
    { "UIPanel_footer", 316.0f, 487.0f, 520.0f, 40.0f },
    { "UIImage_credits_under", 316.0f, 487.0f, 258.0f, 18.0f },
    { "UIImage_credits_under_CREDITS", 578.0f, 509.0f, 258.0f, 18.0f },
    { "UIImage_credits_under_REPLAY", 578.0f, 487.0f, 258.0f, 18.0f },
    { "UIImage_credits_under_TOS", 316.0f, 509.0f, 258.0f, 18.0f },
    { "UILabelButton_AccMgmt", 316.0f, 487.0f, 258.0f, 18.0f },
    { "UILabelButton_Credits", 578.0f, 509.0f, 258.0f, 18.0f },
    { "UILabelButton_ReplayVideos", 578.0f, 487.0f, 258.0f, 18.0f },
    { "UILabelButton_TOS", 316.0f, 509.0f, 258.0f, 18.0f },
    { "UIImage_dropshadowTOS", 303.0f, 496.0f, 284.0f, 44.0f },
    { "UIImage_dropshadowREPLAY", 565.0f, 474.0f, 284.0f, 44.0f },
    { "UIImage_dropshadowCREDITS", 565.0f, 496.0f, 284.0f, 44.0f },
    { "UIImage_dropshadowACC", 303.0f, 474.0f, 284.0f, 44.0f },
    { "Check_RememberData", 438.0f, 356.0f, 16.0f, 16.0f },
    { "EditBox_Password", 439.0f, 384.0f, 352.0f, 19.0f },
    { "EditBox_UserID", 439.0f, 330.0f, 352.0f, 19.0f },
    { "UIImage_email_under", 316.0f, 327.0f, 520.0f, 25.0f },
    { "UIImage_header", 316.0f, 272.0f, 520.0f, 40.0f },
    { "UIImage_Key_Icon", 320.0f, 276.0f, 32.0f, 32.0f },
    { "UIImage_main_under", 316.0f, 317.0f, 520.0f, 100.0f },
    { "UIImage_password_under", 316.0f, 381.0f, 520.0f, 25.0f },
    { "Label_Instructions", 356.0f, 293.0f, 475.0f, 19.0f },
    { "Label_Password", 316.0f, 381.0f, 115.0f, 25.0f },
    { "Label_UserID", 316.0f, 327.0f, 115.0f, 25.0f },
    { "UILabel", 355.0f, 270.0f, 481.0f, 23.0f },
    { "UILabel_RememberMeLabel", 458.0f, 356.0f, 128.0f, 16.0f },
    { "Button_Login", 578.0f, 422.0f, 258.0f, 25.0f },
    { "UILabelButton_Exit", 316.0f, 422.0f, 258.0f, 25.0f },
    { "CapsLockWarningPanel", 616.0f, 356.0f, 175.0f, 16.0f },
    { "CapsLockWarningText", 616.0f, 356.0f, 175.0f, 16.0f },
    { "UIImage_dropshadowEXIT", 303.0f, 409.0f, 284.0f, 51.0f },
    { "UIImage_dropshadowLOGIN", 565.0f, 409.0f, 284.0f, 51.0f },
    { "UIImage_dropshadow", 300.0f, 256.0f, 552.0f, 207.0f },
    { "UIImage_userIDBG", 439.0f, 330.0f, 352.0f, 19.0f },
    { "UIImage_passwordBG", 439.0f, 384.0f, 352.0f, 19.0f },
};
static constexpr int LoginRectCount = sizeof(LoginRects)/sizeof(FRectDef);
static const FBgDef LoginBg[] = {
    { "UIImage", -0.45f, 0.0f, 0.75f, 720.0f, true, false, true, false },
    { "UIImage", 0.7428f, 0.0f, 0.75f, 720.0f, true, false, true, false },
    { "UIImage_Anchor", 0.5f, 0.5f, 0.0f, 0.0f, false, false, true, true },
    { "UIImage_AnimBG", -0.075f, -54.0f, 1.075f, 774.0f, true, false, true, false },
};
static constexpr int LoginBgCount = sizeof(LoginBg)/sizeof(FBgDef);
// ---- Lobby (design 800x600) ----
static constexpr float LobbyDesignW = 800.f;
static constexpr float LobbyDesignH = 600.f;
static const FRectDef LobbyRects[] = {
    { "UIImage", 689.0f, 506.0f, 16.0f, 16.0f },
    { "UIImage_Anchor", 0.5f, 0.5f, 0.0f, 0.0f },
    { "UIImage_CreateDeleteOverlay", 0.0f, 0.0f, 800.0f, 600.0f },
    { "UIPanel_Master", 0.0f, 0.0f, 800.0f, 600.0f },
    { "UIImage_Lobby_Icon", 4.0f, 4.0f, 32.0f, 32.0f },
    { "UILabel_ChooseACharacter", 41.0f, 23.0f, 759.0f, 0.0f },
    { "UILabel_Lobby_TITLE", 40.0f, 22.0f, 760.0f, 0.0f },
    { "UIPanel_BusinessModel", 10.0f, 50.0f, 319.0f, 30.0f },
    { "UIPanel_Character", 336.0f, 300.0f, 128.0f, 25.0f },
    { "UIPanel_CharacterList", 10.0f, 165.0f, 319.0f, 435.0f },
    { "UIImage_dropshadowCHARLIST", -6.0f, 165.0f, 351.0f, 398.0f },
    { "UIPanel_CL_Content", 10.0f, 165.0f, 319.0f, 382.0f },
    { "cUILabelButton_DeleteChar", 10.0f, 195.0f, 31.0f, 25.0f },
    { "UIImage_BaseBG", 10.0f, 165.0f, 319.0f, 382.0f },
    { "UIImage_Characterheader", 10.0f, 165.0f, 319.0f, 25.0f },
    { "UIImage_Characterheader2", 10.0f, 195.0f, 319.0f, 25.0f },
    { "UIImage_Characterheader_SHAD", 10.0f, 190.0f, 319.0f, 5.0f },
    { "UIImage_headerBG", 0.0f, 0.0f, 800.0f, 40.0f },
    { "UILabel_CharacterCount", 289.0f, 165.0f, 35.0f, 25.0f },
    { "UILabel_CharacterHeader", 15.0f, 165.0f, 26.0f, 25.0f },
    { "UILabelButton_CreateCharacter", 41.0f, 195.0f, 288.0f, 25.0f },
    { "UILabelButton_DeleteCharacter", 10.0f, 522.0f, 319.0f, 25.0f },
    { "UILabelButton_Options", 172.0f, 565.0f, 157.0f, 25.0f },
    { "UILabelButton_Quit", 10.0f, 565.0f, 157.0f, 25.0f },
    { "UIList_CharacterList", 10.0f, 226.0f, 319.0f, 321.0f },
    { "UIScrollbar", 309.0f, 226.0f, 20.0f, 321.0f },
    { "DecrementButton", 309.0f, 226.0f, 20.0f, 32.0f },
    { "IncrementButton", 309.0f, 515.0f, 20.0f, 32.0f },
    { "Marker", 0.0f, 32.0f, 0.0f, 225.0f },
    { "UIImage_dropshadowCLOSE", -3.0f, 552.0f, 183.0f, 51.0f },
    { "UIImage_dropshadowCLOSE", 159.0f, 552.0f, 183.0f, 51.0f },
    { "UIImage_HEADER_dropshadow", 0.0f, 40.0f, 800.0f, 5.0f },
    { "UIImage_FactionIcon", 1.06f, -10.24f, 254.94f, 266.24f },
    { "UILabel_CharactersWorldOffline", 336.0f, 302.0f, 128.0f, 18.0f },
    { "UILabel_LoadingCharacter", 336.0f, 302.0f, 128.0f, 18.0f },
    { "UIPanel_C_Content", 401.0f, 502.0f, 389.0f, 40.0f },
    { "UIPanel_Mesh", 0.0f, 0.0f, 800.0f, 600.0f },
    { "RotationSliders", 329.0f, 40.0f, 471.0f, 462.0f },
    { "UIImage", 401.0f, 502.0f, 389.0f, 40.0f },
    { "UIImage_dropshadowCHARINFO", 388.0f, 489.0f, 415.0f, 84.0f },
    { "UIImage_totalplaytime", 401.0f, 542.0f, 389.0f, 18.0f },
    { "UILabel_Cash", 446.0f, 521.0f, 244.0f, 21.0f },
    { "UILabel_CharacterName", 445.0f, 503.0f, 245.0f, 24.0f },
    { "UILabel_CharacterPlayedTime", 406.0f, 542.0f, 379.0f, 17.0f },
    { "UILabel_FactionName", 401.0f, 502.0f, 100.0f, 18.0f },
    { "UILabel_TeamName", 589.0f, 445.0f, 100.0f, 18.0f },
    { "UIPanel_AdditionalInfo", 401.0f, 502.0f, 389.0f, 40.0f },
    { "cUILabelButton_Play", 598.0f, 565.0f, 192.0f, 25.0f },
    { "UIImage_Threat", 405.0f, 506.0f, 32.0f, 32.0f },
    { "UIImage_threatbackground", 401.0f, 502.0f, 40.0f, 40.0f },
    { "UIImage_threatbackground2", 684.0f, 502.0f, 106.0f, 40.0f },
    { "UIImage_threatbackground3", 678.0f, 502.0f, 6.0f, 40.0f },
    { "UILabel_Rating", 708.0f, 504.0f, 82.0f, 33.0f },
    { "UILabel_RatingBG", 708.0f, 506.0f, 82.0f, 31.0f },
    { "UILabelButton_Logout", 401.0f, 565.0f, 192.0f, 25.0f },
    { "UIImage_dropshadowCLOSE", 388.0f, 552.0f, 218.0f, 51.0f },
    { "UIImage_dropshadowCLOSE", 585.0f, 552.0f, 218.0f, 51.0f },
    { "UIImage_RefreshIcon", 336.0f, 174.0f, 128.0f, 128.0f },
    { "UIImage_BusinessModel_header", 10.0f, 90.0f, 319.0f, 21.0f },
    { "UIImage_dropshadowBUSMODEL", -6.0f, 50.0f, 351.0f, 120.0f },
    { "UILabel_Email", 15.0f, 69.0f, 309.0f, 22.0f },
    { "UILabel_GametimeInfo", 15.0f, 111.0f, 314.0f, 19.0f },
    { "UILabel_RealTag", 15.0f, 51.0f, 314.0f, 18.0f },
    { "UILabel_RTWPoints", 15.0f, 51.0f, 309.0f, 18.0f },
    { "UILabelButton_AccMgmt", 10.0f, 130.0f, 319.0f, 25.0f },
    { "UILabel_Remaining_title", 15.0f, 90.0f, 314.0f, 21.0f },
};
static constexpr int LobbyRectCount = sizeof(LobbyRects)/sizeof(FRectDef);
static const FBgDef LobbyBg[] = {
    { "UIImage_AnimBG", -0.1667f, -60.0f, 1.1667f, 660.0f, true, false, true, false },
};
static constexpr int LobbyBgCount = sizeof(LobbyBg)/sizeof(FBgDef);
inline const FRectDef* FindRect(const FRectDef* Arr, int N, const char* Name) {
    for (int i=0;i<N;++i){ const char*a=Arr[i].Name;const char*b=Name;while(*a&&*a==*b){++a;++b;} if(*a==0&&*b==0) return &Arr[i]; }
    return nullptr;
}
}
