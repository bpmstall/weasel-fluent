from PIL import Image

im = Image.open('C:/Users/zheng/.gemini/antigravity/brain/8ecf009c-b150-4eb7-8f24-1a6d2d011642/.user_uploaded/media_1790233126711.png')
w, h = im.size
# Crop around the right icons: width is 776, height is 79
# Candidate 7 is around x=580, then ◀ ▶, then the sticker button, then ∨
crop = im.crop((w - 180, 0, w, h))
crop.save('C:/Users/zheng/.gemini/antigravity/brain/8ecf009c-b150-4eb7-8f24-1a6d2d011642/icon_crop.png')
print('Cropped successfully, size:', crop.size)
