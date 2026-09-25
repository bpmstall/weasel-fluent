import shutil
import os

src_dir = r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt"
dst_dir = r"C:\Users\zheng\.gemini\antigravity\brain\8ecf009c-b150-4eb7-8f24-1a6d2d011642"

files = [
    "cand_win.png",
    "cand_win_vertical.png",
    "settings_win.png",
    "emoji_picker.png",
    "emoji_picker_kaomoji.png",
    "emoji_picker_symbols.png"
]

for f in files:
    src_path = os.path.join(src_dir, f)
    dst_path = os.path.join(dst_dir, f)
    if os.path.exists(src_path):
        shutil.copy2(src_path, dst_path)
        print(f"Copied {f} -> {dst_dir}")
print("Done copying artifacts.")
