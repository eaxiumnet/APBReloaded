import pyautogui
import pygetwindow as gw
import time

pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.5

w = gw.getWindowsWithTitle("uMod V 2.0 alpha (r53)  by  ROTA")[0]
print(f"Window: {w.title} at ({w.left}, {w.top}, {w.width}, {w.height})")

w.activate()
time.sleep(1)

# Click Update button (bottom left area)
pyautogui.click(w.left + 100, w.top + w.height - 50)
time.sleep(1)

print("Update clicked")
