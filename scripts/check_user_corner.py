from PIL import Image

im = Image.open(r"C:\Users\zheng\.gemini\antigravity\brain\8ecf009c-b150-4eb7-8f24-1a6d2d011642\.user_uploaded\media_1790245168563.png")
print("User screenshot size:", im.size)
# The user screenshot has top (our window) and bottom (native window)
# Top window is around y=30 to y=80, x=0 to x=500
crop_top_left = im.crop((0, 0, 50, 50))
crop_top_left.save(r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\scratch_user_corner.png")

# Also print RGB of top left corner of the top window
for y in range(20):
    row = [im.getpixel((x, y)) for x in range(20)]
    # print non-black pixels
    print(f"y={y}:", [c for c in row if c != (0,0,0)])
