from PIL import Image
from pathlib import Path

names = [
    "Constant_BG.tga",
    "LoadingScreen_APB.tga",
    "MaleAvatar.tga",
    "FemaleAvatar.tga",
    "NewBackgroundImage.tga",
    "CriminalFactionicon.tga",
    "EnforcerFactionicon.tga",
]

base_retail = Path(r"D:\APBReloaded\work\login_swap\textures\retail\APBMenus_Art_GameFlowScenes\Texture2D")
base_2011 = Path(r"D:\APBReloaded\work\login_swap\textures\2011\APBMenus_Art_GameFlowScenes\Texture2D")

for name in names:
    r = base_retail / name
    o = base_2011 / name
    if not r.exists():
        print(f"[retail missing] {name}")
        continue
    if not o.exists():
        print(f"[2011 missing] {name}")
        continue
    im_r = Image.open(r)
    im_o = Image.open(o)
    print(f"{name}: retail={im_r.size}/{im_r.mode} ({r.stat().st_size} bytes)  2011={im_o.size}/{im_o.mode} ({o.stat().st_size} bytes)")
