# SNAKE HD v3 — GDI+ 真彩色 emoji 版

控制台版（`../snake_hd/`）无法使用真彩色 emoji：`CHAR_INFO` 每个字符格只能存一个 UTF-16 码元，🍎/🧊/🛑/💈 都是 BMP 之外的代理对。
这个版本换成 **Win32 + GDI+ 窗口**，把系统 Segoe UI Emoji 预渲染成 PNG，所以四个 emoji 都是**真彩色**、原样显示。

## 运行

双击 `snake_gui.exe`。需要和下面这些文件/目录放在一起：

```text
snake_gui/
├─ snake_gui.exe
├─ emoji/             apple.png / ice.png / stop.png / barber.png
├─ sounds/            eat.wav / bonus.wav / dead.wav
├─ snake_hd_best.txt  最高分（会继续更新）
├─ snake_gui.cpp
├─ game.c / game.h
├─ build_gui.bat
├─ snake_gui.cbp
├─ make_emoji.py
└─ gui_*.png          界面预览
```

窗口 1100×700，GDI+ 双缓冲，60 FPS。

## 操作

| 按键 | 功能 |
|---|---|
| 方向键 / WASD | 移动 |
| 空格 / P | 暂停 / 继续 |
| F3 / F4 | 加速 / 减速（1–5 档） |
| M | 背景音乐开 / 关 |
| Esc | 游戏中返回菜单；菜单中退出 |
| Enter / 空格 | 菜单开始；结算再来一局 |
| R | 结算重开 |
| Q | 菜单退出 |

默认 **3 档速度（155ms/步）**；按下方向键后蛇头会立即转向（眼睛/头部即时反馈），身体在下一个逻辑步移动。
窗口启动时会自动抢焦点；如果切到后台再点回来，按一下方向键即可继续。

## 图标

| 对象 | 图标 | 分值 |
|---|---|---|
| 苹果 | 🍎 | +10 |
| 蓝宝石 | 🧊 | +30 |
| 红宝石 | 🛑 | +25 |
| 障碍物 | 💈 | 撞到即结束 |

emoji PNG 由 `make_emoji.py` 用系统 `C:\Windows\Fonts\seguiemj.ttf` 以 `embedded_color=True` 渲染，再裁切到 256×256。
如果系统没有 Segoe UI Emoji，程序会退回彩色圆点；重新生成需要 Windows 8+ 的彩色 emoji 字体。

## 编译

### 方式 A：build_gui.bat
```bat
build_gui.bat
```
脚本会：
1. 用 gcc 编译 `game.c`
2. 用 g++ 编译 `snake_gui.cpp`
3. 静态链接 `snake_gui.exe`（`-mwindows`，不弹控制台、不依赖 MinGW DLL）

### 方式 B：Code::Blocks
打开 `snake_gui.cbp`，按 F9。工程已配置 GDI+、winmm、ole32、uuid、shell32、gdi32 和静态链接。

### 重新生成预览
```bat
build_dump.bat
snake_gui_dump.exe --dump gui_play.png play
snake_gui_dump.exe --dump gui_menu.png menu
snake_gui_dump.exe --dump gui_dead.png dead
snake_gui_dump.exe --dump gui_pause.png pause
```
`snake_gui_dump.exe` 是控制台入口的导出版，仅用于生成预览；玩家版是 `snake_gui.exe`。

## 音乐 / 最高分

- 背景音乐：`kldykxq.mp3` 放在 exe 同目录，或上 1–3 级目录（放在 `tcs` 根目录即可）。
- 音效：`sounds/` 随包提供，按 exe 目录向上找。
- 最高分：`snake_hd_best.txt`，从控制台版沿用；本目录已复制一份（335 分）。只在本局结束时写一次。

## 说明

- 这是**真彩色窗口版**，不是控制台版；原 `snake_hd` 仍保留，方便对比/答辩演示。
- 游戏规则完全复用 `game.c`：链表、占用表、食物/障碍公平生成、速度档位、最高分逻辑都没有重写。
- 渲染层是 GDI+：圆角蛇身、渐变、柔和阴影、透明面板、彩色 emoji、粒子、飘字。
- 预览图 `gui_play.png` 等是 GDI+ 离屏渲染的真实输出，不是手绘效果图。
