import os
from PIL import Image

def analyze():
    img_path = r'C:\Users\zheng\.gemini\antigravity\brain\8ecf009c-b150-4eb7-8f24-1a6d2d011642\.user_uploaded\media_1790245168563.png'
    cand_path = r'cand_win.png'

    im = Image.open(img_path).convert('RGB')
    w, h = im.size

    # Let's crop two regions from user image:
    # Top bar: starts roughly y=40, x=10
    # Bottom bar: starts roughly y=140, x=90
    # Let's save small crops of both to examine text details
    crop_top = im.crop((10, 30, 480, 85))
    crop_bottom = im.crop((90, 140, 950, 195))

    crop_top.save('crop_top.png')
    crop_bottom.save('crop_bottom.png')
    print("Saved crop_top.png and crop_bottom.png")

if __name__ == '__main__':
    analyze()
