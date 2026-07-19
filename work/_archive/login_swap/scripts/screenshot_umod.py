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

# Start uMod
subprocess.Popen([UMOD_EXE], cwd=os.path.dirname(UMOD_EXE))
time.sleep(3)

umod_win = gw.getWindowsWithTitle("uMod V 2.0 alpha (r53)  by  ROTA")[0]
umod_win.activate()
time.sleep(1)

# Add game
pyautogui.click(umod_win.left + 30, umod_win.top + 30)
time.sleep(0.5)
pyautogui.click(umod_win.left + 60, umod_win.top + 80)
time.sleep(2)
pyautogui.typewrite(APB_EXE, interval=0.01)
time.sleep(0.5)
pyautogui.press("enter")
time.sleep(2)

# Launch APB
subprocess.Popen([APB_EXE], cwd=os.path.dirname(APB_EXE))
time.sleep(30)

# Activate uMod and screenshot
umod_win = gw.getWindowsWithTitle("uMod V 2.0 alpha (r53)  by  ROTA")[0]
umod_win.activate()
time.sleep(1)
screenshot = pyautogui.screenshot(region=(umod_win.left, umod_win.top, umod_win.width, umod_win.height))
screenshot.save(r"D:\APBReloaded\work\login_swap\mod\captures\umod_screenshot.png")
print(f"Screenshot saved: {umod_win.left}, {umod_win.top}, {umod_win.width}, {umod_win.height}")
