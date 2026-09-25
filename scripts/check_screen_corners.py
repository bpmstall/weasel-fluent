from PIL import Image

im = Image.open(r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\cand_win_screen.png")
print("Mode:", im.mode, "Size:", im.size)
# inspect pixels around top-left
for y in range(15):
    row = [im.getpixel((x, y)) for x in range(15)]
    print(f"y={y}:", row[:8])
