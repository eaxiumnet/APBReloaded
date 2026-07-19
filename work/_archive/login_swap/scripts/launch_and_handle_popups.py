import subprocess
import time
import ctypes
from ctypes import wintypes
import os

# Win32 API setup
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

EnumWindows = user32.EnumWindows
EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
GetWindowText = user32.GetWindowTextW
GetWindowTextLength = user32.GetWindowTextLengthW
IsWindowVisible = user32.IsWindowVisible
GetWindowThreadProcessId = user32.GetWindowThreadProcessId
GetDlgItemText = user32.GetDlgItemTextW
GetClassName = user32.GetClassNameW

# Constants
BM_CLICK = 0x00F5
WM_CLOSE = 0x0010
WM_COMMAND = 0x0111


def get_window_text(hwnd):
    length = GetWindowTextLength(hwnd)
    if length == 0:
        return ""
    buffer = ctypes.create_unicode_buffer(length + 1)
    GetWindowText(hwnd, buffer, length + 1)
    return buffer.value


def get_class_name(hwnd):
    buffer = ctypes.create_unicode_buffer(256)
    GetClassName(hwnd, buffer, 256)
    return buffer.value


def enum_child_windows(hwnd):
    children = []
    def callback(h, _):
        children.append(h)
        return True
    cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(callback)
    user32.EnumChildWindows(hwnd, cb, 0)
    return children


def find_popup_windows(apb_pid):
    popups = []
    def callback(hwnd, _):
        if not IsWindowVisible(hwnd):
            return True
        pid = wintypes.DWORD()
        GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == apb_pid:
            title = get_window_text(hwnd)
            cls = get_class_name(hwnd)
            if title:
                popups.append((hwnd, title, cls))
        return True
    cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(callback)
    EnumWindows(cb, 0)
    return popups


def click_button(hwnd):
    user32.SendMessageW(hwnd, BM_CLICK, 0, 0)


def close_window(hwnd):
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)


def main():
    apb_dir = r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries"
    apb_exe = os.path.join(apb_dir, "APB.exe")

    print("Launching 2011 APB client...")
    proc = subprocess.Popen([apb_exe], cwd=apb_dir)

    timeout = 120
    start = time.time()
    main_window_seen = False

    while time.time() - start < timeout:
        time.sleep(2)

        # Check if process still alive
        if proc.poll() is not None:
            print(f"APB.exe exited with code {proc.returncode}")
            return

        # Check for popups
        popups = find_popup_windows(proc.pid)
        for hwnd, title, cls in popups:
            print(f"POPUP DETECTED: hwnd={hwnd} title='{title}' class='{cls}'")
            # Try to find child buttons
            children = enum_child_windows(hwnd)
            button_titles = []
            for child in children:
                child_title = get_window_text(child)
                child_class = get_class_name(child)
                if child_class == "Button" and child_title:
                    button_titles.append((child, child_title))
                    print(f"  Button: '{child_title}'")

            # Click OK/Yes/Continue if found
            clicked = False
            for btn, text in button_titles:
                if text in ("OK", "Ok", "ok", "Yes", "yes", "Continue", "continue", "Accept", "accept", "Agree", "agree"):
                    print(f"  Clicking button: {text}")
                    click_button(btn)
                    clicked = True
                    break

            if not clicked:
                # Try to close the popup
                print("  No known button found, closing window")
                close_window(hwnd)

        # Check for main window
        try:
            import psutil
            p = psutil.Process(proc.pid)
            if p.status() == psutil.STATUS_RUNNING:
                # Try to get main window title
                pass
        except Exception:
            pass

    print("Timeout reached. Killing APB.exe")
    proc.kill()


if __name__ == "__main__":
    main()
