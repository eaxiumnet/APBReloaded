#!/usr/bin/env python3
"""Automate uMod to capture live texture hashes from 2011 APB client.

This version uses the standard uMod d3d9.dll and toggles the
"Save all textures" checkbox after the game connects.
"""
import subprocess
import time
import os
from pathlib import Path
import pyautogui
import pygetwindow as gw

pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.5

UMOD_DIR = r"D:\APBReloaded\work\login_swap\mod\uMod\uMod"
UMOD_EXE = os.path.join(UMOD_DIR, "uMod.exe")
APB_EXE = r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe"
CAPTURE_DIR = r"D:\APBReloaded\work\login_swap\mod\captures\umod_save_all"


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

    # Copy standard uMod d3d9.dll to 2011 Binaries
    src_dll = os.path.join(UMOD_DIR, "d3d9.dll")
    dst_dll = r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\d3d9.dll"
    import shutil
    shutil.copy(src_dll, dst_dll)
    print("Copied standard uMod d3d9.dll")

    # Start uMod
    print("Starting uMod...")
    subprocess.Popen([UMOD_EXE], cwd=UMOD_DIR)
    time.sleep(3)

    umod_win = wait_for_window("uMod", timeout=10)
    if not umod_win:
        print("uMod window not found")
        return
    print(f"uMod window: {umod_win.title}")
    umod_win.activate()
    time.sleep(1)

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
    apb_win = wait_for_window("Release_User", timeout=60)
    if not apb_win:
        print("APB window not found")
        return
    print(f"APB window: {apb_win.title}")

    # Wait for game to connect to uMod (tab appears)
    print("Waiting for game to connect to uMod...")
    time.sleep(10)

    # Activate uMod and click the APB tab (second tab)
    umod_win.activate()
    time.sleep(1)
    print("Clicking APB tab...")
    click_at_window(umod_win, 100, 60)  # Approximate tab location

    # Set save path
    print("Setting save path...")
    click_at_window(umod_win, 100, 200)  # Save path text field
    pyautogui.typewrite(CAPTURE_DIR, interval=0.01)
    time.sleep(0.5)

    # Click "Save all textures" checkbox
    print("Enabling Save all textures...")
    click_at_window(umod_win, 30, 250)  # Approximate checkbox location

    # Click Update
    print("Clicking Update...")
    click_at_window(umod_win, 100, umod_win.height - 50)

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
