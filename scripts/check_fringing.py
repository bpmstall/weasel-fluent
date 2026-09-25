from PIL import Image
import numpy as np

def check_fringing():
    top = Image.open('crop_top.png').convert('RGB')
    bot = Image.open('crop_bottom.png').convert('RGB')

    arr_top = np.array(top)
    arr_bot = np.array(bot)

    # Let's inspect difference between R, G, B channels on text edges
    diff_top = np.abs(arr_top[:, :, 0].astype(int) - arr_top[:, :, 2].astype(int))
    diff_bot = np.abs(arr_bot[:, :, 0].astype(int) - arr_bot[:, :, 2].astype(int))

    print("Max |R - B| in crop_top:", np.max(diff_top))
    print("Mean |R - B| in crop_top (where text exists):", np.mean(diff_top[arr_top[:, :, 1] > 100]))

    print("Max |R - B| in crop_bottom:", np.max(diff_bot))
    print("Mean |R - B| in crop_bottom (where text exists):", np.mean(diff_bot[arr_bot[:, :, 1] > 100]))

if __name__ == '__main__':
    check_fringing()
