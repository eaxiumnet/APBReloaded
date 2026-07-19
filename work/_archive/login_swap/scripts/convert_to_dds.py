from PIL import Image
from pathlib import Path


def main():
    src = Path(r"D:\APBReloaded\work\login_swap\textures\2011\APBMenus_Art_GameFlowScenes\Texture2D")
    dst = Path(r"D:\APBReloaded\work\login_swap\mod\textures\2011_dds")
    dst.mkdir(parents=True, exist_ok=True)

    for tga in src.glob("*.tga"):
        try:
            im = Image.open(tga)
            if im.mode in ('RGBA', 'LA', 'P'):
                im = im.convert('RGBA')
            else:
                im = im.convert('RGB')
            out = dst / (tga.stem + ".dds")
            im.save(out, "DDS")
            print(f"Converted {tga.name} -> {out.name} ({im.size[0]}x{im.size[1]} {im.mode})")
        except Exception as e:
            print(f"Failed {tga.name}: {e}")


if __name__ == '__main__':
    main()
