# -*- coding: utf-8 -*-
"""Generate the four true-color emoji PNGs from the Windows color emoji font."""
from PIL import Image, ImageDraw, ImageFont
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "emoji")
os.makedirs(OUT, exist_ok=True)

FONT = r"C:\Windows\Fonts\seguiemj.ttf"
if not os.path.exists(FONT):
    raise SystemExit("Segoe UI Emoji not found: " + FONT)

CHARS = {
    "apple": "\U0001F34E",
    "ice": "\U0001F9CA",
    "stop": "\U0001F6D1",
    "barber": "\U0001F488",
}

font = ImageFont.truetype(FONT, 220)
for name, ch in CHARS.items():
    img = Image.new("RGBA", (320, 320), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.text((160, 160), ch, font=font, anchor="mm", embedded_color=True)
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    side = max(img.size)
    canvas = Image.new("RGBA", (side + 24, side + 24), (0, 0, 0, 0))
    canvas.paste(img, ((canvas.width - img.width) // 2,
                       (canvas.height - img.height) // 2), img)
    canvas = canvas.resize((256, 256), Image.LANCZOS)
    path = os.path.join(OUT, name + ".png")
    canvas.save(path)
    print("wrote", path, canvas.size)
