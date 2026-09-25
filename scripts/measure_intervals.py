from PIL import Image
import numpy as np

def measure_cands():
    bot = Image.open('crop_bottom.png').convert('RGB')
    arr = np.array(bot)
    h, w, _ = arr.shape

    # Find numbers and text along x axis
    # Text is bright (RGB > 140)
    line = np.mean(arr[15:45, :, :], axis=(0, 2))
    # Let's inspect where peaks and valleys are
    in_item = False
    start = 0
    intervals = []
    for x in range(w):
        val = line[x]
        if val > 80:
            if not in_item:
                in_item = True
                start = x
        else:
            if in_item:
                in_item = False
                intervals.append((start, x - 1, x - 1 - start + 1))

    print("Detected text intervals (start, end, width):")
    for iv in intervals:
        print(f"  x={iv[0]}..{iv[1]} (w={iv[2]})")

if __name__ == '__main__':
    measure_cands()
