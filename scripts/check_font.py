import os

font_path = "C:/Windows/Fonts/seguiemj.ttf"
if os.path.exists(font_path):
    size = os.path.getsize(font_path)
    print(f"seguiemj.ttf size: {size} bytes ({size / 1024 / 1024:.2f} MB)")
else:
    print("seguiemj.ttf not found!")
