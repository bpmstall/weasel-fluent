import winreg

def get_accent_info():
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\DWM") as k:
        try:
            val, _ = winreg.QueryValueEx(k, "AccentColor")
            print(f"DWM AccentColor: raw int={val}, hex={hex(val)}")
            # Interpretation 1: ABGR (Windows standard DWM storage)
            r1 = val & 0xff
            g1 = (val >> 8) & 0xff
            b1 = (val >> 16) & 0xff
            print(f"  If ABGR: R={r1}, G={g1}, B={b1} -> #{r1:02x}{g1:02x}{b1:02x}")
            # Interpretation 2: ARGB
            b2 = val & 0xff
            g2 = (val >> 8) & 0xff
            r2 = (val >> 16) & 0xff
            print(f"  If ARGB: R={r2}, G={g2}, B={b2} -> #{r2:02x}{g2:02x}{b2:02x}")
            # Interpretation 3: RGBA
            r3 = (val >> 24) & 0xff
            g3 = (val >> 16) & 0xff
            b3 = (val >> 8) & 0xff
            print(f"  If RGBA: R={r3}, G={g3}, B={b3} -> #{r3:02x}{g3:02x}{b3:02x}")
        except Exception as e:
            print("DWM AccentColor error:", e)

    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Explorer\Accent") as k:
        try:
            palette, _ = winreg.QueryValueEx(k, "AccentPalette")
            print("AccentPalette (32 bytes):")
            # AccentPalette is 8 colors (4 bytes each: RGBA)
            for i in range(8):
                chunk = palette[i*4 : (i+1)*4]
                r, g, b, a = chunk[0], chunk[1], chunk[2], chunk[3]
                print(f"  Color {i} (Index {i}): #{r:02x}{g:02x}{b:02x} (R={r}, G={g}, B={b})")
        except Exception as e:
            print("AccentPalette error:", e)

if __name__ == "__main__":
    get_accent_info()
