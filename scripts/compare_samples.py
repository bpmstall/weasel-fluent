from PIL import Image
import numpy as np

def compare():
    bot = Image.open('crop_bottom.png').convert('RGB')
    # Crop "1 你" from crop_bottom
    # In crop_bottom, '1' is around x=25..35, '你' is x=45..68, y=15..45
    crop_1ni = bot.crop((20, 15, 75, 45))
    crop_1ni.save('crop_native_1ni.png')

    for name in [
        "sample_Microsoft_YaHei_UI_10_nosub.png",
        "sample_Microsoft_YaHei_UI_11_nosub.png",
        "sample_Microsoft_YaHei_UI_11_subpixel.png",
        "sample_Microsoft_YaHei_UI_12_nosub.png",
        "sample_Microsoft_YaHei_UI_12_subpixel.png",
        "sample_Microsoft_YaHei_UI_13_nosub.png",
        "sample_Microsoft_YaHei_UI_14_nosub.png",
    ]:
        im = Image.open(name).convert('RGB')
        arr = np.array(im)
        # find bounding box of text (RGB > 120)
        mask = (arr[:, :, 0] > 120) | (arr[:, :, 1] > 120) | (arr[:, :, 2] > 120)
        ys, xs = np.where(mask)
        h = ys.max() - ys.min() + 1
        w = xs.max() - xs.min() + 1
        # also compute max |R - B|
        diff_rb = np.abs(arr[:, :, 0].astype(int) - arr[:, :, 2].astype(int))
        max_rb = np.max(diff_rb)
        mean_rb = np.mean(diff_rb[mask])
        print(f"{name:45s}: text h={h:2d}px, w={w:2d}px, max|R-B|={max_rb:3d}, mean|R-B|={mean_rb:.2f}")

    arr_nat = np.array(crop_1ni)
    mask_nat = (arr_nat[:, :, 0] > 120) | (arr_nat[:, :, 1] > 120) | (arr_nat[:, :, 2] > 120)
    ys_nat, xs_nat = np.where(mask_nat)
    h_nat = ys_nat.max() - ys_nat.min() + 1
    w_nat = xs_nat.max() - xs_nat.min() + 1
    diff_rb_nat = np.abs(arr_nat[:, :, 0].astype(int) - arr_nat[:, :, 2].astype(int))
    print(f"{'Native (crop_bottom)':45s}: text h={h_nat:2d}px, w={w_nat:2d}px, max|R-B|={np.max(diff_rb_nat):3d}, mean|R-B|={np.mean(diff_rb_nat[mask_nat]):.2f}")

if __name__ == '__main__':
    compare()
