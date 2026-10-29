# -*- coding: utf-8 -*-
"""把 snake_hd --dump 生成的帧缓冲渲染成 PNG 预览图。"""
import sys
from PIL import Image, ImageDraw, ImageFont

CW, CH = 18, 36          # 每个字符格 18x36 px（2x）
ASCII_FONT = r"C:\Windows\Fonts\consola.ttf"
CJK_FONT = r"C:\Windows\Fonts\msyh.ttc"


def wide(ch):
    o = ord(ch)
    return (0x2E80 <= o <= 0xA4CF) or (0xAC00 <= o <= 0xD7A3) or \
           (0xF900 <= o <= 0xFAFF) or (0xFE30 <= o <= 0xFE4F) or \
           (0xFF00 <= o <= 0xFF60) or (0xFFE0 <= o <= 0xFFE6)


def load(path):
    w = h = 0
    pal = {}
    cells = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "#" and p[1] == "SCREEN":
                w, h = int(p[2]), int(p[3])
            elif p[0] == "#" and p[1] == "PAL":
                pal[int(p[2])] = "#" + p[3]
            elif p[0] == "C":
                y, x, cp, fg, bg = (int(v) for v in p[1:6])
                cells[(x, y)] = (chr(cp), fg, bg)
    return w, h, pal, cells


def render(src, dst):
    w, h, pal, cells = load(src)
    img = Image.new("RGB", (w * CW, h * CH), pal.get(0, "#000000"))
    d = ImageDraw.Draw(img)

    for y in range(h):
        for x in range(w):
            ch, fg, bg = cells.get((x, y), (" ", 15, 0))
            d.rectangle([x * CW, y * CH, (x + 1) * CW - 1, (y + 1) * CH - 1],
                        fill=pal.get(bg, "#000000"))

    fa = ImageFont.truetype(ASCII_FONT, 30)
    fb = ImageFont.truetype(CJK_FONT, 29)

    for y in range(h):
        x = 0
        while x < w:
            ch, fg, bg = cells.get((x, y), (" ", 15, 0))
            if ch == " ":
                x += 1
                continue
            color = pal.get(fg, "#ffffff")
            if wide(ch):
                d.text((x * CW + CW, y * CH + CH // 2), ch, font=fb,
                       fill=color, anchor="mm")
                x += 2
            else:
                d.text((x * CW + CW // 2, y * CH + CH // 2 - 1), ch, font=fa,
                       fill=color, anchor="mm")
                x += 1

    img.save(dst)
    print("wrote", dst, img.size)


if __name__ == "__main__":
    render(sys.argv[1], sys.argv[2])
