import ctypes
from ctypes import wintypes

# Let's check if we can query Windows.UI.ViewManagement.UISettings
# or inspect HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\History
import winreg

with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize") as k:
    i = 0
    while True:
        try:
            name, val, typ = winreg.EnumValue(k, i)
            print(f"Personalize: {name} = {val}")
            i += 1
        except OSError:
            break

try:
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Themes\History") as k:
        i = 0
        while True:
            try:
                name, val, typ = winreg.EnumValue(k, i)
                print(f"History: {name} = {val}")
                i += 1
            except OSError:
                break
except Exception as e:
    print("History error:", e)
