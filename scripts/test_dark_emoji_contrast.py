import os
from PIL import Image, ImageDraw, ImageFont

def main():
    bg_color = (32, 32, 32, 255)
    w, h = 640, 280
    img = Image.new("RGBA", (w, h), bg_color)
    draw = ImageDraw.Draw(img)

    emoji_names = ["1f600.png", "1f602.png", "1f970.png", "1f44b.png"]
    asset_dir = r"C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\assets\emojis"

    emojis = []
    for name in emoji_names:
        p = os.path.join(asset_dir, name)
        if os.path.exists(p):
            em = Image.open(p).convert("RGBA").resize((36, 36), Image.Resampling.LANCZOS)
            emojis.append(em)

    opacities = [1.0, 0.90, 0.85, 0.78]
    col_w = w // len(opacities)

    for i, op in enumerate(opacities):
        cx = i * col_w + col_w // 2
        label = f"{int(op*100)}% (原版)" if op == 1.0 else f"{int(op*100)}% 舒适暗色"
        draw.text((cx - 35, 15), label, fill=(180, 180, 180, 255))

        for j, em in enumerate(emojis):
            y = 50 + j * 50
            em_op = em.copy()
            r, g, b, a = em_op.split()
            a = a.point(lambda p: int(p * op))
            em_op.putalpha(a)
            img.alpha_composite(em_op, (cx - 18, y))

    out_path = r"C:\Users\zheng\.gemini\antigravity\brain\8ecf009c-b150-4eb7-8f24-1a6d2d011642\dark_emoji_comparison.png"
    img.save(out_path)
    print("Saved to", out_path)

if __name__ == "__main__":
    main()
