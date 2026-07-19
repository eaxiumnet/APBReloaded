#!/usr/bin/env python3
"""Automate uMod to capture live texture hashes from 2011 APB client."""
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


def wait_for_window(title_substring, timeout=30):
    for _ in range(timeout):
        for w in gw.getAllWindows():
            if title_substring.lower() in w.title.lower():
                return w
        time.sleep(1)
    return None


def main():
    # Ensure capture dir exists
    Path(CAPTURE_DIR).mkdir(parents=True, exist_ok=True)

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

    # Click Main menu -> Add game
    print("Clicking Main -> Add game...")
    pyautogui.click(umod_win.left + 30, umod_win.top + 30)
    time.sleep(0.5)
    pyautogui.click(umod_win.left + 60, umod_win.top + 80)
    time.sleep(2)

    # File dialog: type APB.exe path and press Enter
    print("Selecting APB.exe...")
    pyautogui.typewrite(APB_EXE, interval=0.01)
    time.sleep(0.5)
    pyautogui.press("enter")
    time.sleep(2)

    # Launch APB
    print("Launching APB...")
    subprocess.Popen([APB_EXE], cwd=os.path.dirname(APB_EXE))

    # Wait for APB window
    apb_win = wait_for_window("Release_USER", timeout=60)
    if not apb_win:
        print("APB window not found")
        return
    print(f"APB window: {apb_win.title}")

    # Wait for menu to load
    print("Waiting for menu to load...")
    time.sleep(45)

    # Switch back to uMod
    umod_win.activate()
    time.sleep(1)

    # Click "Save all textures" button (approximate location in Capture tab)
    # This is heuristic; the exact coordinates may vary.
    print("Attempting to enable Save all textures...")
    pyautogui.click(umod_win.left + 100, umod_win.top + 200)
    time.sleep(1)

    # Wait for captures
    print("Waiting for textures to be saved...")
    time.sleep(30)

    # List captured files
    captured = list(Path(CAPTURE_DIR).glob("*.dds"))
    print(f"Captured {len(captured)} textures:")
    for c in captured[:20]:
        print(f"  {c.name}")


if __name__ == "__main__":
    main()
