# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont

BG = (10, 14, 26)
PANEL = (17, 24, 39)
BORDER = (51, 65, 85)
WHITE = (248, 250, 252)
MUTED = (148, 163, 184)
GREEN = (134, 239, 172)
GOLD = (245, 158, 11)

CJK = r"C:\Windows\Fonts\msyh.ttc"
MONO = r"C:\Windows\Fonts\consola.ttf"

W, H = 1680, 780
img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

f_title = ImageFont.truetype(CJK, 38)
f_sub = ImageFont.truetype(CJK, 19)
f_label = ImageFont.truetype(CJK, 22)
f_small = ImageFont.truetype(CJK, 17)

d.text((40, 24), "界面改造前后对比", font=f_title, fill=WHITE)
d.text((40, 78), "同一门 C 语言控制台课设：左边是原版报告截图，右边是 v2 的真实帧缓冲预览（未用任何图形库）",
       font=f_sub, fill=MUTED)

before = Image.open("report_imgs/img11.png").convert("RGB")
before = before.crop((0, 40, before.width, before.height))
before.thumbnail((790, 500), Image.LANCZOS)

after = Image.open("preview_play.png").convert("RGB")
after.thumbnail((790, 500), Image.LANCZOS)

# 卡片
def card(x, y, w, h):
    d.rectangle([x - 2, y - 2, x + w + 1, y + h + 1], fill=BORDER)
    d.rectangle([x, y, x + w, y + h], fill=PANEL)

lx, rx = 40, 860
top = 150
card(lx, top, before.width, before.height)
img.paste(before, (lx, top))
card(rx, top, after.width, after.height)
img.paste(after, (rx, top))

d.text((lx, top - 34), "改造前 · 原版截图", font=f_label, fill=(239, 68, 68))
d.text((rx, top - 34), "改造后 · SNAKE HD v2", font=f_label, fill=GREEN)

d.text((lx, top + before.height + 18), "亮蓝底 + 星星墙 + 3 格蛇身，HUD 与提示文字挤在右侧", font=f_small, fill=MUTED)
d.text((rx, top + after.height + 18), "纯黑背景 + 高对比绿蛇 + 线框 HUD + 全屏结算", font=f_small, fill=GREEN)

d.text((40, 728), "改造后仍是纯 C + Win32 Console：100×30 字符、双缓冲、默认 16 色；游戏逻辑与渲染分离，可再替换为 GDI/SDL。",
       font=f_small, fill=MUTED)
d.text((1530, 728), "@wrl", font=ImageFont.truetype(MONO, 18), fill=GOLD)

img.save("before_after.png")
print("wrote before_after.png", img.size)
