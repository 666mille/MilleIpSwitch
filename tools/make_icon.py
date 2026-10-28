"""Generates src/milleipswitch.ico and docs/icon-preview.png (requires Pillow)."""
import os
from PIL import Image, ImageDraw, ImageFilter
import math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def grad(size, top, bot):
    g = Image.new("RGBA", (size, size))
    d = ImageDraw.Draw(g)
    for y in range(size):
        t = y / (size - 1)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)) + (255,)
        d.line([(0, y), (size, y)], fill=c)
    return g

def render(small=False, N=1024):
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    cx = cy = N / 2
    R = N * (0.445 if not small else 0.44)
    W = N * (0.048 if not small else 0.075)
    mask = Image.new("L", (N, N), 0)
    md = ImageDraw.Draw(mask)
    box = [cx - R - W / 2, cy - R - W / 2, cx + R + W / 2, cy + R + W / 2]
    arcs = [(196, 324), (16, 144)] if not small else [(198, 318), (18, 138)]
    for a0, a1 in arcs:
        md.arc(box, a0, a1, fill=255, width=int(W))
        th = math.radians(a0)
        px, py = cx + R * math.cos(th), cy + R * math.sin(th)
        md.ellipse([px - W / 2, py - W / 2, px + W / 2, py + W / 2], fill=255)
        th1 = math.radians(a1)
        tip = math.radians(a1 + (16 if not small else 22))
        ro, ri = R + W * 1.6, R - W * 1.6
        pts = [(cx + ro * math.cos(th1), cy + ro * math.sin(th1)),
               (cx + R * math.cos(tip), cy + R * math.sin(tip)),
               (cx + ri * math.cos(th1), cy + ri * math.sin(th1))]
        md.polygon(pts, fill=255)
    ring = grad(N, (66, 165, 255), (8, 82, 196))
    img.paste(ring, (0, 0), mask)

    d = ImageDraw.Draw(img)
    bw = N * (0.62 if not small else 0.62)
    bh = bw * 0.88
    x0, y0 = cx - bw / 2, cy - bh / 2 + N * 0.005
    x1, y1 = cx + bw / 2, cy + bh / 2
    rad = bw * 0.16
    out = N * (0.022 if not small else 0.03)
    d.rounded_rectangle([x0, y0, x1, y1], radius=rad, fill=(52, 58, 66, 255))
    ox0, ox1 = x0 + bw * 0.07, x1 - bw * 0.07
    oy0, oy1 = y0 + bh * 0.06, y1 - bh * 0.07
    step = (ox1 - ox0) * 0.15
    stepy = oy0 + (oy1 - oy0) * 0.50
    tab = (ox1 - ox0) * 0.40
    taby = oy0 + (oy1 - oy0) * 0.75
    port = [(ox0, oy0), (ox1, oy0), (ox1, stepy), (ox1 - step, stepy), (ox1 - step, taby),
            (cx + tab / 2 + step * 0.0, taby), (cx + tab / 2, oy1), (cx - tab / 2, oy1),
            (cx - tab / 2, taby), (ox0 + step, taby), (ox0 + step, stepy), (ox0, stepy)]
    if small:
        port = [(ox0, oy0), (ox1, oy0), (ox1, oy1), (ox0, oy1)]
    d.polygon(port, fill=(14, 16, 20, 255))
    sh = bw * (0.045 if not small else 0.06)
    d.line(port + [port[0]], fill=(240, 243, 247, 255), width=int(sh), joint="curve")
    npins = 6 if not small else 1
    pw_total = (ox1 - ox0) * 0.62
    py0, py1 = oy0 + (oy1 - oy0) * 0.13, oy0 + (oy1 - oy0) * 0.40
    if small:
        d.rectangle([cx - pw_total / 2, py0, cx + pw_total / 2, py1], fill=(255, 196, 30, 255))
    else:
        gap = pw_total / npins
        for i in range(npins):
            px = cx - pw_total / 2 + gap * i + gap * 0.2
            d.rectangle([px, py0, px + gap * 0.6, py1], fill=(255, 196, 30, 255))
    return img

big = render(False)
small = render(True)
sizes = [256, 128, 64, 48, 40, 32, 24, 20, 16]
frames = []
for s in sizes:
    src = small if s <= 32 else big
    frames.append(src.resize((s, s), Image.LANCZOS))
frames[0].save(os.path.join(ROOT, "src", "milleipswitch.ico"), sizes=[(s, s) for s in sizes], append_images=frames[1:])
detail = [big.resize((s, s), Image.LANCZOS) for s in sizes]
detail[0].save(os.path.join(ROOT, "src", "milleipswitch_detail.ico"), sizes=[(s, s) for s in sizes], append_images=detail[1:])
prev = Image.new("RGBA", (256 + 20 + 64 + 20 + 32 + 20 + 16 + 20, 256), (255, 255, 255, 255))
x = 0
for s in [256, 64, 32, 16]:
    im = frames[sizes.index(s)]
    prev.paste(im, (x, (256 - s) // 2), im)
    x += s + 20
os.makedirs(os.path.join(ROOT, "docs"), exist_ok=True)
prev.save(os.path.join(ROOT, "docs", "icon-preview.png"))
