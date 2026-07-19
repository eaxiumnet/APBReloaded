# M4a — Stage 2011 menu art + UI sfx into the UE project.
# Runs INSIDE UnrealEditor (UE 5.8) via:
#   D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe D:\APBReloaded\APBReloaded.uproject ^
#     -run=pythonscript -script="D:\APBReloaded\tools\scripts\import_menu2011_assets.py" ^
#     -nullrhi -unattended -nop4
# Imports ONLY the subset that work\menu2011_spec.md references (<=200 textures),
# never a bulk dump. Report: tools\menu2011_import_report.json

import json
import os
import unreal

ROOT = r"D:\APBReloaded"
EXT = os.path.join(ROOT, "Content", "Extracted", "2011")

GFS = os.path.join(EXT, "MenuArt", "APBMenus_Art_GameFlowScenes", "Texture2D")
SKN = os.path.join(EXT, "MenuArt", "APBMenus_Skins", "Texture2D")
FNT = os.path.join(EXT, "MenuArt", "APBMenus_Font", "Texture2D")
POP = os.path.join(EXT, "MenuArt", "APBMenus_Art_PopUpMenu", "Texture2D")
PDL = os.path.join(EXT, "MenuArt", "APBMenus_Art_PopUpDialogs", "Texture2D")
ART = os.path.join(EXT, "MenuArt", "APBMenus_Art", "Texture2D")
DIS = os.path.join(EXT, "MenuArt", "APBMenus_Art_DistrictPictures", "Texture2D")
FIC = os.path.join(EXT, "MenuArt", "APBMenus_Art_FactionIcons", "Texture2D")
LFI = os.path.join(EXT, "MenuArt", "APBMenus_Art_LargeFactionIcons", "Texture2D")
# Full 2011 APBMenus_Art dump (222 PNGs) — the M3 MenuArt tree only has 8 for this package.
CHR = os.path.join(EXT, "LiveCurrentScene", "png", "packages",
                   "APBMenus_Art", "APBMenus_Art", "Texture2D")
HERO = os.path.join(EXT, "LiveCurrentScene", "hero")
SFX = os.path.join(EXT, "UISfx")

UI_DEST = "/Game/Imported/UI/Menu2011"
AU_DEST = "/Game/Audio/UI"

# (source_dir, filename, dest_subfolder)
TEXTURES = [
    # ---- Login ----
    (GFS, "Constant_BG.png", "Login"),
    (GFS, "NewBackgroundImage.png", "Login"),
    (GFS, "frontendFooter.png", "Login"),
    (GFS, "JKICON_login_header_key.png", "Login"),
    (GFS, "splatter1.png", "Login"),
    (CHR, "APB_BG_TextEntry.png", "Login"),
    (CHR, "BG_TextEntry_01.png", "Login"),
    (CHR, "BG_TextEntry_Round_01.png", "Login"),
    (SKN, "Check_True.png", "Login"),
    (SKN, "Check_False.png", "Login"),
    (POP, "JKICON_close_default.png", "Login"),
    (POP, "JKICON_close_over.png", "Login"),
    (POP, "JKICON_close_pressed.png", "Login"),
    (POP, "JKICON_close_Highlight.png", "Login"),
    (HERO, "Login_Scene_Preview.png", "Reference"),
    # ---- Chrome (shared) ----
    (SKN, "MessageBox_BG.png", "Chrome"),
    (CHR, "APB_Window_BG.png", "Chrome"),
    (CHR, "APB_Window_BGC.png", "Chrome"),
    (CHR, "Window_GeneralBG.png", "Chrome"),
    (CHR, "Window_BG_04.png", "Chrome"),
    (CHR, "Window_BG_05.png", "Chrome"),
    (CHR, "Window_BG_05_a.png", "Chrome"),
    (CHR, "Window_BG_06.png", "Chrome"),
    (CHR, "Window_BGC_06.png", "Chrome"),
    (CHR, "Window_Devider_01.png", "Chrome"),
    (CHR, "Window_Devider_02.png", "Chrome"),
    (CHR, "Window_Title_Accent_01.png", "Chrome"),
    (CHR, "APB_Window_Title_Accent.png", "Chrome"),
    (CHR, "APB_DropShadow.png", "Chrome"),
    (CHR, "Menu_Button_On.png", "Chrome"),
    (CHR, "Menu_Button_Off.png", "Chrome"),
    (CHR, "Menu_Button_Light.png", "Chrome"),
    (CHR, "BG_Button_02.png", "Chrome"),
    (CHR, "BG_Button_Active_Ring.png", "Chrome"),
    (CHR, "APB_BG_GenericContent_01.png", "Chrome"),
    (CHR, "APB_List_Cell_NoBG_20.png", "Chrome"),
    (CHR, "APB_List_Cell_NoBG_20_Active.png", "Chrome"),
    (CHR, "APB_List_Cell_NoBG_20_Pressed.png", "Chrome"),
    (CHR, "APB_SmallListItem_Generic_01.png", "Chrome"),
    (CHR, "Slider_Arrow_01_Up.png", "Chrome"),
    (CHR, "Slider_Arrow_01_Up_Active.png", "Chrome"),
    (CHR, "Slider_Arrow_01_Down.png", "Chrome"),
    (CHR, "Slider_Arrow_01_Down_Active.png", "Chrome"),
    (CHR, "Slider_BG_04.png", "Chrome"),
    (CHR, "Slider_Marker_04.png", "Chrome"),
    (CHR, "Slider_Marker_04_horiz.png", "Chrome"),
    (CHR, "Tab_01_On.png", "Chrome"),
    (CHR, "Tab_01_Off.png", "Chrome"),
    (CHR, "Tab_02.png", "Chrome"),
    (CHR, "Tab_02_Enabled.png", "Chrome"),
    (CHR, "APB_Button_Generic_SM_Active.png", "Chrome"),
    (CHR, "APB_Button_Generic_SM_Active_2.png", "Chrome"),
    (CHR, "APB_Button_Generic_SM_On.png", "Chrome"),
    (CHR, "APB_Button_Global_SM_Off.png", "Chrome"),
    (CHR, "APB_Button_Global_SM_On.png", "Chrome"),
    (CHR, "APB_Button_Global_SM_Pressed.png", "Chrome"),
    (CHR, "Tick.png", "Chrome"),
    (CHR, "tickbox.png", "Chrome"),
    (CHR, "Shine_04.png", "Chrome"),
    (POP, "MenuItem_Active.png", "Chrome"),
    (POP, "MenuItem_BG.png", "Chrome"),
    (POP, "cUIbutton_MenuButton.png", "Chrome"),
    (POP, "hUIButton_MenuButton.png", "Chrome"),
    (PDL, "PopupUIHighlightTexture.png", "Chrome"),
    (PDL, "bulletpoint.png", "Chrome"),
    (ART, "GlossyButton_Highlight.png", "Chrome"),
    (ART, "Disabled.png", "Chrome"),
    (FNT, "Texture2D_37.png", "Reference"),  # 2011 font atlas (reference only)
    # ---- CharSelect ----
    (GFS, "MaleAvatar.png", "CharSelect"),
    (GFS, "FemaleAvatar.png", "CharSelect"),
    (GFS, "CharacterSelectIcon.png", "CharSelect"),
    (GFS, "CriminalFactionicon.png", "CharSelect"),
    (GFS, "CriminalFactionicon_Unselected.png", "CharSelect"),
    (GFS, "EnforcerFactionicon.png", "CharSelect"),
    (GFS, "EnforcerFactionicon_Unselected.png", "CharSelect"),
    (GFS, "newCriminalcon.png", "CharSelect"),
    (GFS, "newEnforcerIcon.png", "CharSelect"),
    (GFS, "FactionSelectbulletpoint.png", "CharSelect"),
    (GFS, "FactionSelectbulletpoint_Unselected.png", "CharSelect"),
    (GFS, "factionheadericon.png", "CharSelect"),
    (GFS, "factionselectbuttongrey.png", "CharSelect"),
    (FIC, "Faction_Icon_Criminal_32px.png", "CharSelect"),
    (FIC, "Faction_Icon_Criminal_64px.png", "CharSelect"),
    (FIC, "Faction_Icon_Enforcer_32px.png", "CharSelect"),
    (FIC, "Faction_Icon_Enforcer_64px.png", "CharSelect"),
    (FIC, "Spawnpoint_Select_Arrow.png", "CharSelect"),
    (LFI, "JKICON_Criminal_Large.png", "CharSelect"),
    (LFI, "JKICON_Enforcer_Large.png", "CharSelect"),
    (HERO, "Lobby_Scene_Preview.png", "Reference"),
    (HERO, "CreateCharacter_Scene_Preview.png", "Reference"),
    (HERO, "FactionSelect_Preview.png", "Reference"),
    # ---- DistrictSelect ----
    (DIS, "FinancialDistrict_MainPhoto256x195.png", "DistrictSelect"),
    (DIS, "SocialDistrict_MainPhoto256x195.png", "DistrictSelect"),
    (DIS, "WaterfrontDistrict_MainPhoto256x195.png", "DistrictSelect"),
    (DIS, "Waterfront_ImageA.png", "DistrictSelect"),
    (GFS, "worldselecticon.png", "DistrictSelect"),
    (CHR, "APB_Button_DistrictSelect_Background.png", "DistrictSelect"),
    (CHR, "APB_Button_Dropdown_DistrictSelect.png", "DistrictSelect"),
    (CHR, "APB_Button_Dropdown_DistrictSelect_Active.png", "DistrictSelect"),
    (CHR, "APB_Button_Dropdown_DistrictSelect_Pressed.png", "DistrictSelect"),
    (CHR, "APB_Button_Dropdown_DistrictSelect_Targeted.png", "DistrictSelect"),
    (CHR, "StatusBar_BG.png", "DistrictSelect"),
    (CHR, "StatusBar_Bar.png", "DistrictSelect"),
    # ---- Loading / queue ----
    (GFS, "LoadingScreen_APB.png", "Loading"),
    (GFS, "LoadingScreen_APBFlame_Alpha.png", "Loading"),
    (GFS, "LoadingScreen_Flames.png", "Loading"),
    (GFS, "LoadingScreen_Flames_Alpha.png", "Loading"),
    (GFS, "LoadingScreen_Flames_Mask.png", "Loading"),
    (GFS, "LoadingScreen_Flames_RimGlow.png", "Loading"),
    (GFS, "LoadingScreen_Flames_Emburs.png", "Loading"),
    (GFS, "LoadingScreen_Flames_Emburs_02.png", "Loading"),
    (GFS, "LoadingScreen_Overlay.png", "Loading"),
    (GFS, "LoadingIcon_MAIN.png", "Loading"),
    (GFS, "LoadingArrows_BG.png", "Loading"),
    (GFS, "LoadingArrows_Mask.png", "Loading"),
    (GFS, "LoadingArrows_Ring.png", "Loading"),
    (GFS, "LoadingScreen_FactionIcon_Crim.png", "Loading"),
    (GFS, "LoadingScreen_FactionIcon_Crim_Mask.png", "Loading"),
    (GFS, "LoadingScreen_FactionIcon_Enf.png", "Loading"),
    (GFS, "LoadingScreen_FactionIcon_Enf_Mask.png", "Loading"),
    (GFS, "LoadingScreen_FI_Crim_Alpha.png", "Loading"),
    (GFS, "LoadingScreen_FI_Enf_Alpha.png", "Loading"),
    (HERO, "WorldQueue_Scene_Preview.png", "Reference"),
]

SOUNDS = [
    "TabSound_10.wav",          # UI_Hover
    "ButtonPos.wav",            # UI_Click
    "Button2.wav",              # UI_Back
    "Error.wav",                # UI_Error
    "PopUp.wav",                # UI_Popup
    "Positive3.wav",            # UI_SceneOpen
    "Spark.wav",                # UI_ListSelect
    "Positive.wav",             # UI_CharConfirm
    "Button4_616844292.wav",    # UI_SliderTick
    "CSABeep2.wav",             # UI_Loading ping
    "Button1.wav",              # spare
    "Nice.wav",                 # spare
]


def make_task(filename, dest_path):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    return task


def run():
    report = {"textures_ok": [], "textures_failed": [],
              "sounds_ok": [], "sounds_failed": []}
    tasks = []
    for src_dir, name, sub in TEXTURES:
        path = os.path.join(src_dir, name)
        if not os.path.isfile(path):
            report["textures_failed"].append({"file": path, "error": "source missing"})
            continue
        tasks.append(make_task(path, UI_DEST + "/" + sub))
    for name in SOUNDS:
        path = os.path.join(SFX, name)
        if not os.path.isfile(path):
            report["sounds_failed"].append({"file": path, "error": "source missing"})
            continue
        tasks.append(make_task(path, AU_DEST))

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for task in tasks:
        base = os.path.basename(task.filename)
        is_sound = base.lower().endswith(".wav")
        ok_list = report["sounds_ok"] if is_sound else report["textures_ok"]
        fail_list = report["sounds_failed"] if is_sound else report["textures_failed"]
        if task.imported_object_paths:
            ok_list.append({"file": task.filename,
                            "assets": list(task.imported_object_paths)})
        else:
            fail_list.append({"file": task.filename, "error": "no asset imported"})

    out = os.path.join(ROOT, "tools", "menu2011_import_report.json")
    with open(out, "w") as f:
        json.dump(report, f, indent=1)
    unreal.log("M4A_IMPORT textures ok={} fail={} | sounds ok={} fail={}".format(
        len(report["textures_ok"]), len(report["textures_failed"]),
        len(report["sounds_ok"]), len(report["sounds_failed"])))
    unreal.log("M4A_IMPORT report -> " + out)


run()
