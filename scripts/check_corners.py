from PIL import Image

im = Image.open(r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\cand_win.png")
print("Mode:", im.mode, "Size:", im.size)
# inspect pixels at (0,0), (1,0), (0,1), etc.
for y in range(5):
    row = [im.getpixel((x, y)) for x in range(5)]
    print(f"y={y}:", row)
