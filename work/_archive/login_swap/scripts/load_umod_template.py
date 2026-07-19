import pyautogui
import pygetwindow as gw
import time

pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.5

w = gw.getWindowsWithTitle("uMod V 2.0 alpha (r53)  by  ROTA")[0]
print(f"Window: {w.title} at ({w.left}, {w.top}, {w.width}, {w.height})")

w.activate()
time.sleep(1)

# Click Main menu
pyautogui.click(w.left + 30, w.top + 30)
time.sleep(0.5)

# Click "Load template" (5th item in Main menu)
pyautogui.click(w.left + 60, w.top + 130)
time.sleep(2)

# Type template path
pyautogui.typewrite(r"D:\APBReloaded\work\login_swap\mod\captures\umod_save_all\capture_template.uMod", interval=0.01)
time.sleep(0.5)
pyautogui.press("enter")
time.sleep(2)

print("Template loaded")
