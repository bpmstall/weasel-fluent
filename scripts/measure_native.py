from PIL import Image
import numpy as np

def measure():
    bot = Image.open('crop_bottom.png').convert('RGB')
    arr = np.array(bot)
    h, w, _ = arr.shape
    print(f"crop_bottom shape: {w}x{h}")

    # Let's find candidate bar top and bottom in crop_bottom
    # Dark bar background is around RGB(32, 32, 32)
    # The background outside the bar is around (24, 24, 24) or whatever
    # Let's inspect column slices or vertical profile
    col = arr[:, 200, :]
    print("Vertical profile at x=200:")
    for y in range(h):
        if np.mean(col[y]) > 20: # non-black
            # print first few and last few
            pass

    # Let's find the accent bar (cyan color)
    # Cyan is high in G and B, lower in R
    cyan_mask = (arr[:, :, 2] > 120) & (arr[:, :, 1] > 120) & (arr[:, :, 0] < 100)
    ys, xs = np.where(cyan_mask)
    if len(ys) > 0:
        print(f"Accent bar: x range [{xs.min()}, {xs.max()}], y range [{ys.min()}, {ys.max()}]")
        print(f"Accent bar width: {xs.max() - xs.min() + 1}, height: {ys.max() - ys.min() + 1}")

    # Let's find pill background around the first candidate
    # Pill is lighter than background (e.g. RGB around 50-60)
    pill_mask = (arr[:, :, 0] > 45) & (arr[:, :, 0] < 80) & (arr[:, :, 1] > 45) & (arr[:, :, 1] < 80)
    p_ys, p_xs = np.where(pill_mask)
    if len(p_ys) > 0:
        print(f"Pill: x range [{p_xs.min()}, {p_xs.max()}], y range [{p_ys.min()}, {p_ys.max()}]")
        print(f"Pill width: {p_xs.max() - p_xs.min() + 1}, height: {p_ys.max() - p_ys.min() + 1}")

    # Let's find character '你' bounding box
    # White text: RGB > 180
    ni_mask = (arr[:, :, 0] > 180) & (arr[:, :, 1] > 180) & (arr[:, :, 2] > 180)
    # restrict to x range near first candidate (xs between 40 and 120)
    ni_mask_1 = ni_mask & (np.arange(w)[None, :] >= 45) & (np.arange(w)[None, :] <= 100)
    n_ys, n_xs = np.where(ni_mask_1)
    if len(n_ys) > 0:
        print(f"Char '你': x range [{n_xs.min()}, {n_xs.max()}], y range [{n_ys.min()}, {n_ys.max()}]")
        print(f"Char '你' width: {n_xs.max() - n_xs.min() + 1}, height: {n_ys.max() - n_ys.min() + 1}")

    # Let's find number '1' bounding box
    num_mask = (arr[:, :, 0] > 120) & (arr[:, :, 1] > 120) & (arr[:, :, 2] > 120)
    num1_mask = num_mask & (np.arange(w)[None, :] >= 20) & (np.arange(w)[None, :] <= 44)
    num_ys, num_xs = np.where(num1_mask)
    if len(num_ys) > 0:
        print(f"Number '1': x range [{num_xs.min()}, {num_xs.max()}], y range [{num_ys.min()}, {num_ys.max()}]")
        print(f"Number '1' width: {num_xs.max() - num_xs.min() + 1}, height: {num_ys.max() - num_ys.min() + 1}")

if __name__ == '__main__':
    measure()
