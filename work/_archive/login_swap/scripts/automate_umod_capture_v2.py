#!/usr/bin/env python3
"""Automate uMod to capture live texture hashes from 2011 APB client using a template."""
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


def create_template():
    Path(CAPTURE_DIR).mkdir(parents=True, exist_ok=True)
    content = f"""ShowCollPane:1
SavePath:{CAPTURE_DIR}
SaveAllTextures:1
SaveSingleTexture:0
ShowSingleTextureString:0
ShowSingleTexture:0
SupportTPF:0
UseSizeFilter:0
Height:1,4096
Width:1,4096
Depth:1,1
UseFormatFilter:0
FileFormat:16
FontColour:255,0,0,255
TextureColour:0,255,0,255
"""
    Path(TEMPLATE_PATH).write_text(content, encoding="utf-8")
    print(f"Template created: {TEMPLATE_PATH}")


def main():
    create_template()

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

    # Load template via Main -> Load Template
    print("Loading template...")
    pyautogui.click(umod_win.left + 30, umod_win.top + 30)
    time.sleep(0.5)
    pyautogui.click(umod_win.left + 60, umod_win.top + 220)
    time.sleep(2)

    # File dialog: type template path and press Enter
    pyautogui.typewrite(TEMPLATE_PATH, interval=0.01)
    time.sleep(0.5)
    pyautogui.press("enter")
    time.sleep(2)

    # Add game
    print("Adding APB.exe...")
    pyautogui.click(umod_win.left + 30, umod_win.top + 30)
    time.sleep(0.5)
    pyautogui.click(umod_win.left + 60, umod_win.top + 80)
    time.sleep(2)
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
