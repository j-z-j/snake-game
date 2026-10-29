#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

/* ============================================================
 * 颜色：只使用终端默认 16 色，保证 cmd / Windows Terminal 一致。
 * 不再调用 SetConsoleScreenBufferInfoEx 自定义调色板——部分终端会忽略它，
 * 反而退回默认的蓝/绿/洋红，画面会变得很花。
 * ============================================================ */
enum {
    C_BG = 0, C_PANEL = 0, C_FIELD_A = 0, C_FIELD_B = 0,
    C_FRAME_D = 8, C_FRAME_L = 8,
    C_ACCENT = 9, C_MUTED = 8, C_TEXT = 7,
    C_SNAKE_D = 2, C_SNAKE_M = 10, C_SNAKE_B = 10,
    C_RED = 12, C_GOLD = 14, C_CYAN = 11, C_WHITE = 15
};

/* 预览图使用 Windows 控制台默认 16 色，和实际运行一致 */
static const COLORREF PAL[16] = {
    RGB(0, 0, 0),
    RGB(0, 0, 128),
    RGB(0, 128, 0),
    RGB(0, 128, 128),
    RGB(128, 0, 0),
    RGB(128, 0, 128),
    RGB(128, 128, 0),
    RGB(192, 192, 192),
    RGB(128, 128, 128),
    RGB(0, 0, 255),
    RGB(0, 255, 0),
    RGB(0, 255, 255),
    RGB(255, 0, 0),
    RGB(255, 0, 255),
    RGB(255, 255, 0),
    RGB(255, 255, 255)
};

#define ATTR(fg, bg) ((WORD)(((bg) << 4) | (fg)))

/* 场地左上角（屏幕字符坐标） */
#define FIELD_X 3
#define FIELD_Y 3

static HANDLE g_out = INVALID_HANDLE_VALUE;
static CHAR_INFO fb[SCREEN_W * SCREEN_H];
static bool g_music_available = false;
static bool g_music_on = false;

/* 动画状态 */
static double last_now = 0.0;
static double death_at = -1.0;
static GameState prev_state = ST_MENU;
static int last_score_seen = 0;

typedef struct {
    double x, y, vx, vy;
    double life, max_life;
    wchar_t ch;
    int color;
} Particle;

typedef struct {
    double x, y;
    double life;
    wchar_t text[8];
    int color;
} FloatText;

#define MAX_PARTICLES 96
#define MAX_FLOATS    12

static Particle particles[MAX_PARTICLES];
static int particle_count = 0;
static FloatText floats[MAX_FLOATS];
static int float_count = 0;

/* ============================================================
 * 低层绘制
 * ============================================================ */

static void put(int x, int y, wchar_t ch, WORD attr)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    int i = y * SCREEN_W + x;
    fb[i].Char.UnicodeChar = ch;
    fb[i].Attributes = attr;
}

static void fill_bg(int x, int y, int w, int h, int bg)
{
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            put(xx, yy, L' ', ATTR(0, bg));
        }
    }
}

static void hline(int x, int y, int w, int color)
{
    for (int i = 0; i < w; i++) put(x + i, y, L'-', ATTR(color, C_BG));
}

static void vline(int x, int y, int h, int color)
{
    for (int i = 0; i < h; i++) put(x, y + i, L'|', ATTR(color, C_BG));
}

static void corner(int x, int y, int color)
{
    put(x, y, L'+', ATTR(color, C_BG));
}

static int char_w(wchar_t c)
{
    if (c < 0x1100) return 1;
    if ((c >= 0x2E80 && c <= 0xA4CF) ||
        (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE30 && c <= 0xFE4F) ||
        (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6))
        return 2;
    return 1;
}

static int text_width(const wchar_t *s)
{
    int w = 0;
    for (; *s; s++) w += char_w(*s);
    return w;
}

static int text(int x, int y, const wchar_t *s, WORD attr)
{
    for (; *s; s++) {
        int w = char_w(*s);
        if (x + w > SCREEN_W) break;
        put(x, y, *s, attr);
        if (w == 2) put(x + 1, y, L' ', attr);
        x += w;
    }
    return x;
}

static void text_center(int y, const wchar_t *s, WORD attr)
{
    int w = text_width(s);
    text((SCREEN_W - w) / 2, y, s, attr);
}

static void text_center_in(int x, int w, int y, const wchar_t *s, WORD attr)
{
    int tw = text_width(s);
    text(x + (w - tw) / 2, y, s, attr);
}

/* 在已有背景上写字（用于粒子、飘字） */
static void text_keep_bg(int x, int y, const wchar_t *s, int fg)
{
    for (; *s; s++) {
        int w = char_w(*s);
        if (x < 0 || x + w > SCREEN_W || y < 0 || y >= SCREEN_H) break;
        int i = y * SCREEN_W + x;
        WORD bg = (WORD)((fb[i].Attributes >> 4) & 0x0F);
        put(x, y, *s, ATTR(fg, bg));
        if (w == 2) put(x + 1, y, L' ', ATTR(fg, bg));
        x += w;
    }
}

static void fmt(wchar_t *buf, size_t n, const wchar_t *f, ...)
{
    va_list ap;
    va_start(ap, f);
    _vsnwprintf(buf, n, f, ap);
    va_end(ap);
    buf[n - 1] = L'\0';
}

/* ============================================================
 * 像素字体
 * ============================================================ */

static const char *F5[26][5] = {
    { " ### ", "#   #", "#####", "#   #", "#   #" }, /* A */
    { "#### ", "#   #", "#### ", "#   #", "#### " }, /* B */
    { " ####", "#    ", "#    ", "#    ", " ####" }, /* C */
    { "#### ", "#   #", "#   #", "#   #", "#### " }, /* D */
    { "#####", "#    ", "#### ", "#    ", "#####" }, /* E */
    { "#####", "#    ", "#### ", "#    ", "#    " }, /* F */
    { " ####", "#    ", "#  ##", "#   #", " ####" }, /* G */
    { "#   #", "#   #", "#####", "#   #", "#   #" }, /* H */
    { "#####", "  #  ", "  #  ", "  #  ", "#####" }, /* I */
    { "#####", "   # ", "   # ", "#  # ", " ##  " }, /* J */
    { "#   #", "#  # ", "###  ", "#  # ", "#   #" }, /* K */
    { "#    ", "#    ", "#    ", "#    ", "#####" }, /* L */
    { "#   #", "## ##", "# # #", "#   #", "#   #" }, /* M */
    { "#   #", "##  #", "# # #", "#  ##", "#   #" }, /* N */
    { " ### ", "#   #", "#   #", "#   #", " ### " }, /* O */
    { "#### ", "#   #", "#### ", "#    ", "#    " }, /* P */
    { " ### ", "#   #", "# # #", "#  # ", " ## #" }, /* Q */
    { "#### ", "#   #", "#### ", "#  # ", "#   #" }, /* R */
    { " ####", "#    ", " ### ", "    #", "#### " }, /* S */
    { "#####", "  #  ", "  #  ", "  #  ", "  #  " }, /* T */
    { "#   #", "#   #", "#   #", "#   #", " ### " }, /* U */
    { "#   #", "#   #", "#   #", " # # ", "  #  " }, /* V */
    { "#   #", "#   #", "# # #", "## ##", "#   #" }, /* W */
    { "#   #", " # # ", "  #  ", " # # ", "#   #" }, /* X */
    { "#   #", " # # ", "  #  ", "  #  ", "  #  " }, /* Y */
    { "#####", "   # ", "  #  ", " #   ", "#####" }  /* Z */
};

static const char *D3[10][5] = {
    { "###", "# #", "# #", "# #", "###" },
    { "  #", "  #", "  #", "  #", "  #" },
    { "###", "  #", "###", "#  ", "###" },
    { "###", "  #", "###", "  #", "###" },
    { "# #", "# #", "###", "  #", "  #" },
    { "###", "#  ", "###", "  #", "###" },
    { "###", "#  ", "###", "# #", "###" },
    { "###", "  #", "  #", "  #", "  #" },
    { "###", "# #", "###", "# #", "###" },
    { "###", "# #", "###", "  #", "###" }
};

static int big5_width(int scale, const wchar_t *s)
{
    int n = (int)wcslen(s);
    return n ? (6 * scale * n - scale) : 0;
}

static void big5(int x, int y, int scale, const wchar_t *s, int color)
{
    for (; *s; s++) {
        wchar_t c = *s;
        if (c == L' ') {
            x += 6 * scale;
            continue;
        }
        if (c >= L'a' && c <= L'z') c -= 32;
        int idx = (int)c - 'A';
        if (idx >= 0 && idx < 26) {
            for (int r = 0; r < 5; r++) {
                for (int col = 0; col < 5; col++) {
                    if (F5[idx][r][col] == '#')
                        fill_bg(x + col * scale, y + r * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

static void digits3(int x, int y, int scale, const wchar_t *s, int color)
{
    for (; *s; s++) {
        if (*s >= L'0' && *s <= L'9') {
            int idx = (int)(*s - L'0');
            for (int r = 0; r < 5; r++) {
                for (int col = 0; col < 3; col++) {
                    if (D3[idx][r][col] == '#')
                        fill_bg(x + col * scale, y + r * scale, scale, scale, color);
                }
            }
        }
        x += 4 * scale;
    }
}

/* ============================================================
 * 特效
 * ============================================================ */

static void clear_effects(void)
{
    particle_count = 0;
    float_count = 0;
}

static void spawn_burst(int cx, int cy, FoodKind kind)
{
    int base_color = (kind == FOOD_BLUE) ? C_CYAN : C_RED;
    double sx = FIELD_X + 2 * cx + 1;
    double sy = FIELD_Y + cy;

    for (int i = 0; i < 14 && particle_count < MAX_PARTICLES; i++) {
        Particle *p = &particles[particle_count++];
        p->x = sx;
        p->y = sy;
        p->vx = ((rand() % 200) - 100) / 9.0;
        p->vy = ((rand() % 160) - 110) / 9.0;
        p->max_life = 0.35 + (rand() % 40) / 100.0;
        p->life = p->max_life;
        p->ch = (i % 3 == 0) ? L'+' : ((i % 3 == 1) ? L'*' : L'.');
        p->color = (i % 4 == 0) ? C_WHITE : base_color;
    }
}

static void spawn_float(int cx, int cy, FoodKind kind, int speed)
{
    if (float_count >= MAX_FLOATS) return;
    FloatText *f = &floats[float_count++];
    int gain = game_value_of(kind) + (speed - 1) * 5;
    f->x = FIELD_X + 2 * cx;
    f->y = FIELD_Y + cy - 1;
    f->life = 0.9;
    f->color = (kind == FOOD_APPLE) ? C_WHITE : ((kind == FOOD_BLUE) ? C_CYAN : C_RED);
    fmt(f->text, 8, L"+%d", gain);
}

static void update_effects(double now)
{
    double dt = (now - last_now) / 1000.0;
    if (dt < 0.0) dt = 0.0;
    if (dt > 0.1) dt = 0.016;

    for (int i = 0; i < particle_count; ) {
        Particle *p = &particles[i];
        p->life -= dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 6.0 * dt;
        if (p->life <= 0.0) {
            particles[i] = particles[particle_count - 1];
            particle_count--;
        } else {
            i++;
        }
    }

    for (int i = 0; i < float_count; ) {
        FloatText *f = &floats[i];
        f->life -= dt;
        f->y -= 1.6 * dt;
        if (f->life <= 0.0) {
            floats[i] = floats[float_count - 1];
            float_count--;
        } else {
            i++;
        }
    }
}

static void draw_effects(void)
{
    for (int i = 0; i < particle_count; i++) {
        Particle *p = &particles[i];
        int x = (int)p->x;
        int y = (int)p->y;
        if (x < FIELD_X || x >= FIELD_X + FIELD_W * 2) continue;
        if (y < FIELD_Y || y >= FIELD_Y + FIELD_H) continue;
        int color = p->life < p->max_life * 0.35 ? C_FRAME_L : p->color;
        text_keep_bg(x, y, (wchar_t[]){ p->ch, 0 }, color);
    }
    for (int i = 0; i < float_count; i++) {
        FloatText *f = &floats[i];
        if (f->y < FIELD_Y) continue;
        text_keep_bg((int)f->x, (int)f->y, f->text, f->color);
    }
}

/* ============================================================
 * 场景：游戏
 * ============================================================ */

static void draw_topbar(const Game *g)
{
    int x = 2;
    x = text(x, 0, L"贪吃蛇", ATTR(C_SNAKE_M, C_BG)) + 2;
    text(x, 0, L"SNAKE HD", ATTR(C_MUTED, C_BG));

    const wchar_t *st = L"主菜单";
    int col = C_MUTED;
    switch (g->state) {
    case ST_PLAYING: st = L"游玩中"; col = C_SNAKE_M; break;
    case ST_PAUSED:  st = L"已暂停"; col = C_GOLD;    break;
    case ST_DEAD:    st = L"已结束"; col = C_RED;     break;
    default: break;
    }
    text_center(0, st, ATTR(col, C_BG));

    wchar_t buf[32];
    fmt(buf, 32, L"BEST %06d", g->best);
    text(SCREEN_W - text_width(buf) - 2, 0, buf, ATTR(C_GOLD, C_BG));

    hline(0, 1, SCREEN_W, C_FRAME_L);
}

static void draw_field_frame(void)
{
    hline(2, 2, 58, C_FRAME_L);
    hline(2, 25, 58, C_FRAME_L);
    vline(2, 2, 24, C_FRAME_L);
    vline(59, 2, 24, C_FRAME_L);
    corner(2, 2, C_FRAME_L);
    corner(59, 2, C_FRAME_L);
    corner(2, 25, C_FRAME_L);
    corner(59, 25, C_FRAME_L);

    text(4, 2, L" ARENA 竞技场 ", ATTR(C_TEXT, C_BG));
    text(45, 25, L" 28 x 22 ", ATTR(C_MUTED, C_BG));
}

static void draw_tile(int cx, int cy, int bg_l, int bg_r, wchar_t ch_l, wchar_t ch_r, int fg)
{
    int x = FIELD_X + 2 * cx;
    int y = FIELD_Y + cy;
    put(x, y, ch_l, ATTR(fg, bg_l));
    put(x + 1, y, ch_r, ATTR(fg, bg_r));
}

static void draw_food(const Food *f, double now)
{
    if (!f->active) return;

    int x = FIELD_X + 2 * f->x;
    int y = FIELD_Y + f->y;
    bool blink = ((int)(now / 320.0) & 1) != 0;

    switch (f->kind) {
    case FOOD_BLUE:
        /* 蓝宝石 / 冰块：[] */
        put(x, y, L'[', ATTR(blink ? C_WHITE : C_CYAN, C_BG));
        put(x + 1, y, L']', ATTR(blink ? C_WHITE : C_ACCENT, C_BG));
        break;
    case FOOD_RED:
        /* 红宝石：<> */
        put(x, y, L'<', ATTR(blink ? C_WHITE : C_RED, C_BG));
        put(x + 1, y, L'>', ATTR(blink ? C_WHITE : C_RED, C_BG));
        break;
    default:
        /* 苹果：@ + 绿色叶子 ^ */
        put(x, y, L'@', ATTR(C_RED, C_BG));
        put(x + 1, y, L'^', ATTR(C_SNAKE_M, C_BG));
        break;
    }
}

static void draw_field(const Game *g, double now)
{
    /* 单一黑色背景，不再铺棋盘格 */
    fill_bg(FIELD_X, FIELD_Y, FIELD_W * 2, FIELD_H, C_BG);

    draw_food(&g->apple, now);
    draw_food(&g->bonus, now);

    bool blink = ((int)(now / 110.0) & 1) != 0;
    static const int stripes[3] = { C_RED, C_WHITE, C_ACCENT };
    int phase = (int)(now / 160.0) % 3;

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        const Obstacle *o = &g->obstacles[i];
        int x = FIELD_X + 2 * o->x;
        int y = FIELD_Y + o->y;
        if (o->warning && blink) {
            put(x, y, L'!', ATTR(C_WHITE, C_BG));
            put(x + 1, y, L'!', ATTR(C_WHITE, C_BG));
        } else {
            /* 理发店转灯：两根竖条，红/白/蓝循环 */
            put(x, y, L'|', ATTR(stripes[phase], C_BG));
            put(x + 1, y, L'|', ATTR(stripes[(phase + 1) % 3], C_BG));
        }
    }

    bool flash = (g->state == ST_DEAD) && death_at >= 0.0 && (now - death_at) < 700.0 &&
                 ((int)((now - death_at) / 90.0) & 1);

    int i = 0;
    for (const SnakeNode *n = g->head; n; n = n->next, i++) {
        int bg = C_SNAKE_M;
        if (i == 0) {
            bg = C_SNAKE_B;
        } else if (i >= g->len - 2) {
            bg = C_SNAKE_D;      /* 尾巴最后两节用深绿收尾 */
        }
        if (flash) bg = C_RED;

        if (i == 0) {
            draw_tile(n->x, n->y, bg, bg, L'o', L'o', flash ? C_WHITE : C_BG);
        } else {
            draw_tile(n->x, n->y, bg, bg, L' ', L' ', C_BG);
        }
    }

    draw_effects();
}

static void draw_hud(const Game *g, double now)
{
    /* 只用一条竖线分隔，不铺背景块 */
    vline(60, 2, 24, C_FRAME_L);
    text(61, 2, L"STATUS", ATTR(C_MUTED, C_BG));

    text(61, 3, L"SCORE", ATTR(C_MUTED, C_BG));
    text(94, 3, L"得分", ATTR(C_MUTED, C_BG));

    int score = g->score;
    if (score > 99999) score = 99999;
    wchar_t num[8];
    fmt(num, 8, L"%05d", score);

    bool hot = (g->last_eat_ms >= 0 && g->game_ms - g->last_eat_ms < 220 && g->state == ST_PLAYING);
    digits3(61, 5, 2, num, hot ? C_GOLD : C_SNAKE_M);

    wchar_t buf[64];

    text(61, 16, L"BEST", ATTR(C_MUTED, C_BG));
    fmt(buf, 64, L"%06d", g->best);
    text(98 - text_width(buf), 16, buf, ATTR(C_GOLD, C_BG));

    text(61, 17, L"LEN", ATTR(C_MUTED, C_BG));
    fmt(buf, 64, L"%d", g->len);
    text(98 - text_width(buf), 17, buf, ATTR(C_TEXT, C_BG));

    text(61, 18, L"SPEED", ATTR(C_MUTED, C_BG));
    for (int i = 0; i < 5; i++) {
        wchar_t ch = (i < g->speed) ? L'=' : L'-';
        int col = (i < g->speed) ? C_SNAKE_M : C_FRAME_L;
        put(69 + i, 18, ch, ATTR(col, C_BG));
    }
    fmt(buf, 64, L"%d/5", g->speed);
    text(75, 18, buf, ATTR(C_TEXT, C_BG));

    hline(61, 19, 38, C_FRAME_L);

    text(61, 20, L"食物 FOOD", ATTR(C_MUTED, C_BG));

    put(61, 21, L'@', ATTR(C_RED, C_BG));
    put(62, 21, L'^', ATTR(C_SNAKE_M, C_BG));
    text(65, 21, L"苹果  +10", ATTR(C_TEXT, C_BG));

    bool blink = ((int)(now / 320.0) & 1) != 0;
    put(61, 22, L'[', ATTR(blink ? C_WHITE : C_CYAN, C_BG));
    put(62, 22, L']', ATTR(blink ? C_WHITE : C_ACCENT, C_BG));
    text(65, 22, L"蓝宝石 +30", ATTR(C_TEXT, C_BG));

    put(61, 23, L'<', ATTR(C_RED, C_BG));
    put(62, 23, L'>', ATTR(C_RED, C_BG));
    text(65, 23, L"红宝石 +25", ATTR(C_TEXT, C_BG));

    text(61, 24, L"MUSIC", ATTR(C_MUTED, C_BG));
    if (!g_music_available) {
        text(75, 24, L"NO MP3", ATTR(C_MUTED, C_BG));
    } else {
        text(75, 24, g_music_on ? L"ON" : L"OFF",
             ATTR(g_music_on ? C_CYAN : C_MUTED, C_BG));
    }
}

static void draw_footer(const Game *g, double now)
{
    if (g->toast[0] && g->game_ms < g->toast_until) {
        text_center(26, g->toast, ATTR(C_GOLD, C_BG));
    } else {
        hline(0, 26, SCREEN_W, C_FRAME_L);
    }

    text_center(27, L"方向键 / WASD 移动   空格 暂停   F3 / F4 调速   M 音乐   Esc 返回菜单",
                ATTR(C_TEXT, C_BG));
    text_center(28, L"规则：吃食物变长 · 撞墙/自身/障碍物结束 · 障碍物每 9 秒换位 · 速度每档 +5 分",
                ATTR(C_MUTED, C_BG));

    (void)now;
}

static void panel(int x, int y, int w, int h, int accent)
{
    fill_bg(x, y, w, h, C_BG);
    hline(x, y, w, C_FRAME_L);
    hline(x, y + h - 1, w, C_FRAME_L);
    vline(x, y, h, C_FRAME_L);
    vline(x + w - 1, y, h, C_FRAME_L);
    corner(x, y, C_FRAME_L);
    corner(x + w - 1, y, C_FRAME_L);
    corner(x, y + h - 1, C_FRAME_L);
    corner(x + w - 1, y + h - 1, C_FRAME_L);
    if (accent >= 0) hline(x + 1, y + 1, w - 2, accent);
}

static void draw_pause(const Game *g)
{
    fill_bg(0, 0, SCREEN_W, SCREEN_H, C_BG);
    panel(19, 7, 62, 16, C_GOLD);
    big5(32, 10, 1, L"PAUSED", C_GOLD);

    wchar_t buf[64];
    fmt(buf, 64, L"本局 %05d      长度 %d", g->score, g->len);
    text_center_in(19, 62, 16, buf, ATTR(C_TEXT, C_BG));

    text_center_in(19, 62, 18, L"按 空格 继续", ATTR(C_TEXT, C_BG));
    text_center_in(19, 62, 19, L"Esc 返回主菜单", ATTR(C_MUTED, C_BG));
}

static void draw_gameover(const Game *g, double now)
{
    fill_bg(0, 0, SCREEN_W, SCREEN_H, C_BG);
    panel(8, 1, 84, 28, C_RED);
    big5(23, 4, 1, L"GAME OVER", C_RED);
    text_center(10, game_death_text(g->death), ATTR(C_TEXT, C_BG));

    int score = g->score;
    if (score > 99999) score = 99999;
    wchar_t num[8];
    fmt(num, 8, L"%05d", score);
    digits3((SCREEN_W - 38) / 2, 13, 2, num, C_SNAKE_M);

    wchar_t buf[64];
    fmt(buf, 64, L"最高 %05d", g->best);
    text_center(24, buf, ATTR(C_TEXT, C_BG));

    if (g->new_record) {
        bool blink = ((int)(now / 380.0) & 1) != 0;
        text_center(25, L"**  新 纪 录  **", ATTR(blink ? C_GOLD : C_WHITE, C_BG));
    } else {
        text_center(25, L"再接再厉", ATTR(C_MUTED, C_BG));
    }

    text_center(26, L"R 再来一局      Esc 返回主菜单", ATTR(C_TEXT, C_BG));
}

static void draw_game(const Game *g, double now)
{
    draw_topbar(g);
    draw_field_frame();
    draw_field(g, now);
    draw_hud(g, now);
    draw_footer(g, now);

    if (g->state == ST_PAUSED) draw_pause(g);
    if (g->state == ST_DEAD && death_at >= 0.0 && (now - death_at) >= 650.0)
        draw_gameover(g, now);
}

/* ============================================================
 * 场景：主菜单
 * ============================================================ */

static void draw_menu(const Game *g, double now)
{
    fill_bg(0, 0, SCREEN_W, SCREEN_H, C_BG);

    const wchar_t *author = L"2023141231  王瑞龄";
    text(SCREEN_W - text_width(author) - 2, 1, author, ATTR(C_MUTED, C_BG));

    int scale = 3;
    int title_w = big5_width(scale, L"SNAKE");
    int tx = (SCREEN_W - title_w) / 2;
    big5(tx + 1, 4, scale, L"SNAKE", C_SNAKE_D);   /* 阴影 */
    big5(tx, 3, scale, L"SNAKE", C_SNAKE_M);       /* 主体 */

    text_center(20, L"贪 吃 蛇", ATTR(C_GOLD, C_BG));

    wchar_t buf[64];
    fmt(buf, 64, L"历史最高   %06d", g->best);
    text_center(22, buf, ATTR(C_TEXT, C_BG));

    bool blink = ((int)(now / 520.0) & 1) != 0;
    text_center(24, L"[ ENTER ]   开始游戏        [ Q ]   退出",
                ATTR(blink ? C_WHITE : C_TEXT, C_BG));

    text_center(26, L"方向键 / WASD 移动   ·   空格 暂停   ·   F3 / F4 调速   ·   M 音乐",
                ATTR(C_TEXT, C_BG));
    text_center(28, L"28 x 22 竞技场   ·   3 个游走障碍   ·   苹果 / 蓝宝石 / 红宝石",
                ATTR(C_MUTED, C_BG));
}

/* ============================================================
 * 对外接口
 * ============================================================ */

void render_set_console(HANDLE hOut)
{
    g_out = hOut;
}

void render_apply_palette(HANDLE hOut)
{
    /* 写回“默认 VGA 16 色”，而不是自定义深色调色板：
       - 如果旧版在同一个控制台里改过 ColorTable，这里会把它恢复；
       - 如果终端忽略 ColorTable（Windows Terminal 的常见行为），也不会影响默认配色。 */
    CONSOLE_SCREEN_BUFFER_INFOEX ex;
    ZeroMemory(&ex, sizeof(ex));
    ex.cbSize = sizeof(ex);
    if (GetConsoleScreenBufferInfoEx(hOut, &ex)) {
        for (int i = 0; i < 16; i++) ex.ColorTable[i] = PAL[i];
        SetConsoleScreenBufferInfoEx(hOut, &ex);
    }
}

void render_set_music(bool available, bool on)
{
    g_music_available = available;
    g_music_on = on;
}

void render_draw(const Game *g, double now_ms)
{
    if (last_now <= 0.0) last_now = now_ms;
    update_effects(now_ms);

    /* 关键：每帧整屏清空，避免菜单/上一局的文字残留在结算或游戏画面里 */
    fill_bg(0, 0, SCREEN_W, SCREEN_H, C_BG);

    if (g->state == ST_MENU) {
        last_score_seen = 0;
        clear_effects();
        death_at = -1.0;
    } else {
        if (g->state == ST_DEAD && prev_state != ST_DEAD) death_at = now_ms;
        if (g->state != ST_DEAD) death_at = -1.0;

        if (g->score > last_score_seen) {
            spawn_burst(g->last_eat_x, g->last_eat_y, g->last_eat_kind);
            if (g->last_eat_x >= 0) spawn_float(g->last_eat_x, g->last_eat_y, g->last_eat_kind, g->speed);
            last_score_seen = g->score;
        } else if (g->score < last_score_seen) {
            last_score_seen = g->score;
        }
    }
    prev_state = g->state;
    last_now = now_ms;

    if (g->state == ST_MENU) draw_menu(g, now_ms);
    else                      draw_game(g, now_ms);
}

void render_present(void)
{
    if (g_out == INVALID_HANDLE_VALUE) return;
    COORD size = { SCREEN_W, SCREEN_H };
    COORD origin = { 0, 0 };
    SMALL_RECT region = { 0, 0, SCREEN_W - 1, SCREEN_H - 1 };
    WriteConsoleOutputW(g_out, fb, size, origin, &region);
}

bool render_dump(const wchar_t *path)
{
    FILE *fp = _wfopen(path, L"wb");
    if (!fp) return false;

    fprintf(fp, "# SCREEN %d %d\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < 16; i++) {
        fprintf(fp, "# PAL %d %02X%02X%02X\n", i,
                GetRValue(PAL[i]), GetGValue(PAL[i]), GetBValue(PAL[i]));
    }
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            const CHAR_INFO *c = &fb[y * SCREEN_W + x];
            fprintf(fp, "C %d %d %u %d %d\n", y, x,
                    (unsigned)c->Char.UnicodeChar,
                    c->Attributes & 0x0F,
                    (c->Attributes >> 4) & 0x0F);
        }
    }
    fclose(fp);
    return true;
}
