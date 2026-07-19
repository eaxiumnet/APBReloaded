import subprocess
import time
import ctypes
from ctypes import wintypes
import os

user32 = ctypes.windll.user32

GetWindowText = user32.GetWindowTextW
GetWindowTextLength = user32.GetWindowTextLengthW
IsWindowVisible = user32.IsWindowVisible
GetWindowThreadProcessId = user32.GetWindowThreadProcessId
GetClassName = user32.GetClassNameW
EnumChildWindows = user32.EnumChildWindows
SendMessage = user32.SendMessageW

BM_CLICK = 0x00F5
WM_CLOSE = 0x0010


def get_text(hwnd):
    length = GetWindowTextLength(hwnd)
    if length == 0:
        return ""
    buffer = ctypes.create_unicode_buffer(length + 1)
    GetWindowText(hwnd, buffer, length + 1)
    return buffer.value


def get_class(hwnd):
    buffer = ctypes.create_unicode_buffer(256)
    GetClassName(hwnd, buffer, 256)
    return buffer.value


def get_all_child_text(hwnd):
    texts = []
    def callback(h, _):
        if IsWindowVisible(h):
            t = get_text(h)
            if t:
                texts.append(t)
        return True
    cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(callback)
    EnumChildWindows(hwnd, cb, 0)
    return texts


def find_windows(pid):
    windows = []
    def callback(hwnd, _):
        if not IsWindowVisible(hwnd):
            return True
        p = wintypes.DWORD()
        GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid:
            title = get_text(hwnd)
            cls = get_class(hwnd)
            if title:
                windows.append((hwnd, title, cls))
        return True
    cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)(callback)
    user32.EnumWindows(cb, 0)
    return windows


def click_button(hwnd):
    SendMessage(hwnd, BM_CLICK, 0, 0)


def close_window(hwnd):
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)


def main():
    apb_dir = r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries"
    apb_exe = os.path.join(apb_dir, "APB.exe")

    print("Launching 2011 APB client...")
    proc = subprocess.Popen([apb_exe], cwd=apb_dir)

    timeout = 120
    start = time.time()

    while time.time() - start < timeout:
        time.sleep(2)

        if proc.poll() is not None:
            print(f"APB.exe exited with code {proc.returncode}")
            return

        windows = find_windows(proc.pid)
        for hwnd, title, cls in windows:
            print(f"Window: hwnd={hwnd} title='{title}' class='{cls}'")
            child_texts = get_all_child_text(hwnd)
            print(f"  Child texts: {child_texts}")

            # Only handle dialog-class windows with OK button
            if cls == "#32770":
                ok_btn = None
                for text in child_texts:
                    if text in ("OK", "Ok", "ok"):
                        # Find the button handle - we already have texts, need handles
                        pass
                # For now, just log; don't auto-click to avoid closing real popups we need to read
                print("  ^ This is a dialog. Review the child texts above.")

    print("Timeout reached. Killing APB.exe")
    proc.kill()


if __name__ == "__main__":
    main()
