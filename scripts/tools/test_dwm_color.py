import ctypes
from ctypes import wintypes

dwmapi = ctypes.WinDLL("dwmapi")
color = wintypes.DWORD()
opaque = wintypes.BOOL()

hr = dwmapi.DwmGetColorizationColor(ctypes.byref(color), ctypes.byref(opaque))
val = color.value
print(f"DwmGetColorizationColor: hr={hr}, val={val} (0x{val:08x})")
# ARGB format:
a = (val >> 24) & 0xff
r = (val >> 16) & 0xff
g = (val >> 8) & 0xff
b = val & 0xff
print(f"  ARGB: A={a}, R={r}, G={g}, B={b} -> #{r:02x}{g:02x}{b:02x}")
