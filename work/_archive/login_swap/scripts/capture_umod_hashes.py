import subprocess
import time
import os
from pathlib import Path
import pyautogui
import pygetwindow as gw

pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.5

UMOD_EXE = r"D:\APBReloaded\work\login_swap\mod\uMod\uMod\uMod.exe"
APB_EXE = r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe"
CAPTURE_DIR = r"D:\APBReloaded\work\login_swap\mod\captures\umod_save_all"
TEMPLATE_PATH = r"D:\APBReloaded\work\login_swap\mod\captures\umod_save_all\capture_template.uMod"


def wait_for_window(title_substring, timeout=30):
    for _ in range(timeout):
        for w in gw.getAllWindows():
            if title_substring.lower() in w.title.lower():
                return w
        time.sleep(1)
    return None


def click_at_window(w, x, y):
    pyautogui.click(w.left + x, w.top + y)
    time.sleep(0.5)


def main():
    # Ensure capture dir exists and is empty
    Path(CAPTURE_DIR).mkdir(parents=True, exist_ok=True)
    for f in Path(CAPTURE_DIR).glob("*.dds"):
        f.unlink()

    # Start uMod
    print("Starting uMod...")
    subprocess.Popen([UMOD_EXE], cwd=os.path.dirname(UMOD_EXE))
    time.sleep(3)

    umod_win = wait_for_window("uMod", timeout=10)
    if not umod_win:
        print("uMod window not found")
        return
    print(f"uMod window: {umod_win.title}")
    umod_win.activate()
    time.sleep(1)

    # Load template first
    print("Loading template...")
    click_at_window(umod_win, 30, 30)  # Main menu
    click_at_window(umod_win, 60, 130)  # Load template
    time.sleep(2)
    pyautogui.typewrite(TEMPLATE_PATH, interval=0.01)
    time.sleep(0.5)
    pyautogui.press("enter")
    time.sleep(2)
    print("Template loaded")

    # Add game
    print("Adding APB.exe...")
    click_at_window(umod_win, 30, 30)  # Main menu
    click_at_window(umod_win, 60, 80)  # Add game
    time.sleep(2)
    pyautogui.typewrite(APB_EXE, interval=0.01)
    time.sleep(0.5)
    pyautogui.press("enter")
    time.sleep(2)
    print("Game added")

    # Launch APB
    print("Launching APB...")
    subprocess.Popen([APB_EXE], cwd=os.path.dirname(APB_EXE))

    # Wait for APB window
    apb_win = wait_for_window("Release_USER", timeout=60)
    if not apb_win:
        print("APB window not found")
        return
    print(f"APB window: {apb_win.title}")

    # Wait for menu to load and textures to be saved
    print("Waiting for menu to load and textures to be saved...")
    time.sleep(60)

    # List captured files
    captured = list(Path(CAPTURE_DIR).glob("*.dds"))
    print(f"Captured {len(captured)} textures:")
    for c in captured[:30]:
        print(f"  {c.name}")


if __name__ == "__main__":
    main()
