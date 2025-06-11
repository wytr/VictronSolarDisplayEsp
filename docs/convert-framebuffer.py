from PIL import Image
import numpy as np

# ————————— CONFIG ——————————
width, height = 480, 320    # your panel’s logical resolution
bpp_bytes     = 2           # 16 bits/pixel → 2 bytes
double_size   = width * height * bpp_bytes * 2

# If you initialized your panel with .color_space = BGR:
is_bgr = False

# Match whatever LVGL used for `sw_rotate`
# (0, 90, 180 or 270 degrees)
sw_rotate = 0
# ————————————————————————————

# 1) load the two buffers
with open("framebuffer.raw", "rb") as f:
    raw = f.read(double_size)
buf1 = raw[: width * height * bpp_bytes]
buf2 = raw[width * height * bpp_bytes : 2 * width * height * bpp_bytes]

def process_buffer(buf, suffix):
    arr = np.frombuffer(buf, dtype=">u2").reshape((height, width))

    # 3) undo LVGL’s software rotation
    if   sw_rotate ==  90:
        arr = arr.T[:, ::-1]
    elif sw_rotate == 180:
        arr = arr[::-1, ::-1]
    elif sw_rotate == 270:
        arr = arr.T[::-1, :]

    img_h, img_w = arr.shape

    # 4) convert each RGB565 → RGB888 (with optional BGR swap)
    def decode_rgb565(px):
        if is_bgr:
            b5 = (px >> 11) & 0x1F
            g6 = (px >>  5) & 0x3F
            r5 = (px      ) & 0x1F
        else:
            r5 = (px >> 11) & 0x1F
            g6 = (px >>  5) & 0x3F
            b5 = (px      ) & 0x1F
        return (r5 << 3, g6 << 2, b5 << 3)

    rgb = np.zeros((img_h, img_w, 3), dtype=np.uint8)
    for y in range(img_h):
        for x in range(img_w):
            rgb[y, x] = decode_rgb565(int(arr[y, x]))

    img = Image.fromarray(rgb, mode="RGB")
    img.save(f"screenshot_{suffix}.png")
    print(f"→ wrote screenshot_{suffix}.png ({img_w}×{img_h})")

# Process both buffers
process_buffer(buf1, "buf1")
