from PIL import Image
import numpy as np

def measure_cand():
    im = Image.open('cand_win.png').convert('RGB')
    arr = np.array(im)
    h, w, _ = arr.shape
    print(f"cand_win shape: {w}x{h}")

    # Cyan accent bar
    cyan_mask = (arr[:, :, 2] > 100) & (arr[:, :, 1] > 100) & (arr[:, :, 0] < 100)
    ys, xs = np.where(cyan_mask)
    if len(ys) > 0:
        print(f"Accent bar: x range [{xs.min()}, {xs.max()}], y range [{ys.min()}, {ys.max()}]")
        print(f"Accent bar width: {xs.max() - xs.min() + 1}, height: {ys.max() - ys.min() + 1}")

    # Number 1
    num_mask = (arr[:, :, 0] > 100) & (arr[:, :, 1] > 100) & (arr[:, :, 2] > 100)
    num1_mask = num_mask & (np.arange(w)[None, :] >= 15) & (np.arange(w)[None, :] <= 35)
    num_ys, num_xs = np.where(num1_mask)
    if len(num_ys) > 0:
        print(f"Number '1': x range [{num_xs.min()}, {num_xs.max()}], y range [{num_ys.min()}, {num_ys.max()}]")
        print(f"Number '1' width: {num_xs.max() - num_xs.min() + 1}, height: {num_ys.max() - num_ys.min() + 1}")

    # Character 你
    ni_mask = (arr[:, :, 0] > 150) & (arr[:, :, 1] > 150) & (arr[:, :, 2] > 150)
    ni_mask_1 = ni_mask & (np.arange(w)[None, :] >= 35) & (np.arange(w)[None, :] <= 70)
    n_ys, n_xs = np.where(ni_mask_1)
    if len(n_ys) > 0:
        print(f"Char '你': x range [{n_xs.min()}, {n_xs.max()}], y range [{n_ys.min()}, {n_ys.max()}]")
        print(f"Char '你' width: {n_xs.max() - n_xs.min() + 1}, height: {n_ys.max() - n_ys.min() + 1}")

if __name__ == '__main__':
    measure_cand()
