from PIL import Image
import numpy as np

# ————————— CONFIG ——————————
width, height = 320, 240    # your panel’s logical resolution
bpp_bytes     = 2           # 16 bits/pixel → 2 bytes
double_size   = width * height * bpp_bytes * 2

# If you initialized your panel with .color_space = BGR:
is_bgr = False

# Match whatever LVGL used for `sw_rotate`
# (0, 90, 180 or 270 degrees)
sw_rotate = 0
# ————————————————————————————

# 1) load the two buffers, take the first one
with open("framebuffer.raw", "rb") as f:
    raw = f.read(double_size)
buf = raw[: width * height * bpp_bytes]

# 2) parse as big-endian 16-bit words
arr = np.frombuffer(buf, dtype=">u2").reshape((height, width))

# 3) undo LVGL’s software rotation
if   sw_rotate ==  90:
    arr = arr.T[:, ::-1]
elif sw_rotate == 180:
    arr = arr[::-1, ::-1]
elif sw_rotate == 270:
    arr = arr.T[::-1, :]

# now arr.shape = (img_h, img_w)
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
    # expand to 8-bits
    return (r5 << 3, g6 << 2, b5 << 3)

rgb = np.zeros((img_h, img_w, 3), dtype=np.uint8)
for y in range(img_h):
    for x in range(img_w):
        rgb[y, x] = decode_rgb565(int(arr[y, x]))

# 5) save
img = Image.fromarray(rgb, mode="RGB")
img.save("screenshot.png")
print(f"→ wrote screenshot.png ({img_w}×{img_h})")
