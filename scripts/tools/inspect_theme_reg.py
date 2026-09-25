import winreg

def check_keys(path):
    print(f"=== {path} ===")
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as k:
            i = 0
            while True:
                try:
                    name, val, typ = winreg.EnumValue(k, i)
                    print(f"  {name} = {val} ({hex(val) if isinstance(val, int) else val})")
                    i += 1
                except OSError:
                    break
    except Exception as e:
        print("  Error:", e)

check_keys(r"Software\Microsoft\Windows\CurrentVersion\Themes")
check_keys(r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize")
check_keys(r"Control Panel\Colors")
check_keys(r"Control Panel\Desktop")
