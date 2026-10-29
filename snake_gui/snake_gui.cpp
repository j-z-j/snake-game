// snake_gui.cpp - SNAKE HD GDI+ true-color emoji edition
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "game.h"

using namespace Gdiplus;

static const int WIN_W = 1100;
static const int WIN_H = 700;
static const float FX = 30.0f, FY = 64.0f, CELL = 26.0f;
static const float FW = FIELD_W * CELL;
static const float FH = FIELD_H * CELL;
static const float HX = 790.0f, HY = 52.0f, HW = 280.0f, HH = 596.0f;

static Color C(int r, int g, int b, int a = 255)
{
    return Color((BYTE)a, (BYTE)r, (BYTE)g, (BYTE)b);
}

static HWND g_hwnd = NULL;
static Bitmap *g_canvas = NULL;
static bool g_run = true;
static Game g_game;
static Dir g_face = DIR_RIGHT;
static double g_now = 0.0;
static double g_death_at = -1.0;
static double g_acc = 0.0;
static unsigned g_eat_seq = 0, g_death_seq = 0;
static ULONG_PTR g_token = 0;

static FontFamily *g_yh = NULL, *g_cons = NULL, *g_disp = NULL;
static Font *f_title = NULL, *f_sub = NULL, *f_state = NULL, *f_label = NULL;
static Font *f_score = NULL, *f_value = NULL, *f_small = NULL, *f_prompt = NULL;
static Font *f_menu = NULL, *f_menu_sub = NULL, *f_over = NULL, *f_big = NULL;
static Font *f_hud = NULL, *f_emoji = NULL;
static Image *img_apple = NULL, *img_ice = NULL, *img_stop = NULL, *img_barber = NULL;

static bool g_music_on = false, g_music_ok = false, g_music_open = false;
static std::wstring g_snd_eat, g_snd_bonus, g_snd_dead;

struct Particle { float x, y, vx, vy, life, maxlife, size; Color color; };
struct FloatText { float x, y, life; wchar_t text[16]; Color color; };
static std::vector<Particle> g_parts;
static std::vector<FloatText> g_floats;

// ---------- paths / assets ----------
static std::wstring ExeDir()
{
    wchar_t b[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, b, MAX_PATH);
    if (!n || n >= MAX_PATH) return L"";
    std::wstring s(b, n);
    size_t p = s.find_last_of(L"\\/");
    return p == std::wstring::npos ? L"" : s.substr(0, p + 1);
}

static bool Exists(const std::wstring &p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring Find(const wchar_t *rel)
{
    std::wstring d = ExeDir();
    const wchar_t *pre[] = { L"", L"..\\", L"..\\..\\", L"..\\..\\..\\" };
    for (int i = 0; i < 4; i++) {
        std::wstring p = d + pre[i] + rel;
        if (Exists(p)) return p;
    }
    return L"";
}

static Image *LoadImg(const wchar_t *rel)
{
    std::wstring p = Find(rel);
    if (p.empty()) return NULL;
    Image *im = Image::FromFile(p.c_str());
    if (im && im->GetLastStatus() != Ok) { delete im; return NULL; }
    return im;
}

static FontFamily *Family(const wchar_t *name, const wchar_t *fb)
{
    FontFamily *f = new FontFamily(name);
    if (f->GetLastStatus() != Ok) { delete f; f = new FontFamily(fb); }
    return f;
}

static void LoadAll()
{
    img_apple = LoadImg(L"emoji\\apple.png");
    img_ice = LoadImg(L"emoji\\ice.png");
    img_stop = LoadImg(L"emoji\\stop.png");
    img_barber = LoadImg(L"emoji\\barber.png");

    g_yh = Family(L"Microsoft YaHei", L"Microsoft Sans Serif");
    g_cons = Family(L"Consolas", L"Courier New");
    g_disp = Family(L"Impact", L"Arial Black");

    f_title = new Font(g_yh, 26, FontStyleBold, UnitPixel);
    f_sub = new Font(g_cons, 18, FontStyleBold, UnitPixel);
    f_state = new Font(g_yh, 18, FontStyleBold, UnitPixel);
    f_label = new Font(g_yh, 14, FontStyleRegular, UnitPixel);
    f_score = new Font(g_cons, 50, FontStyleBold, UnitPixel);
    f_value = new Font(g_cons, 20, FontStyleBold, UnitPixel);
    f_small = new Font(g_yh, 13, FontStyleRegular, UnitPixel);
    f_prompt = new Font(g_yh, 21, FontStyleBold, UnitPixel);
    f_menu = new Font(g_disp, 104, FontStyleBold, UnitPixel);
    f_menu_sub = new Font(g_yh, 34, FontStyleBold, UnitPixel);
    f_over = new Font(g_cons, 56, FontStyleBold, UnitPixel);
    f_big = new Font(g_cons, 78, FontStyleBold, UnitPixel);
    f_hud = new Font(g_yh, 15, FontStyleBold, UnitPixel);
    f_emoji = new Font(g_yh, 15, FontStyleRegular, UnitPixel);
}

// ---------- drawing helpers ----------
static void Setup(Graphics &g)
{
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetCompositingQuality(CompositingQualityHighQuality);
}

static void Round(GraphicsPath &p, const RectF &r, float rad)
{
    float d = rad * 2;
    if (d > r.Width) d = r.Width;
    if (d > r.Height) d = r.Height;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

static void FillRound(Graphics &g, const RectF &r, float rad, const Color &c)
{
    GraphicsPath p;
    Round(p, r, rad);
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

static void StrokeRound(Graphics &g, const RectF &r, float rad, const Color &c, float w)
{
    GraphicsPath p;
    Round(p, r, rad);
    Pen pen(c, w);
    g.DrawPath(&pen, &p);
}

static void TL(Graphics &g, const wchar_t *s, Font *f, const Color &c, float x, float y)
{
    SolidBrush b(c);
    g.DrawString(s, -1, f, PointF(x, y), &b);
}

static void TC(Graphics &g, const wchar_t *s, Font *f, const Color &c, const RectF &r)
{
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c);
    g.DrawString(s, -1, f, r, &fmt, &b);
}

static float TW(Graphics &g, const wchar_t *s, Font *f)
{
    RectF r;
    g.MeasureString(s, -1, f, PointF(0, 0), &r);
    return r.Width;
}

static void TR(Graphics &g, const wchar_t *s, Font *f, const Color &c, float right, float y)
{
    TL(g, s, f, c, right - TW(g, s, f), y);
}

static void Emoji(Graphics &g, Image *im, float cx, float cy, float sz, const Color &fb)
{
    if (im) {
        g.DrawImage(im, RectF(cx - sz / 2, cy - sz / 2, sz, sz),
                    0, 0, (REAL)im->GetWidth(), (REAL)im->GetHeight(), UnitPixel);
    } else {
        SolidBrush b(fb);
        g.FillEllipse(&b, RectF(cx - sz / 2, cy - sz / 2, sz, sz));
    }
}

// ---------- audio ----------
static void SoundInit()
{
    std::wstring p = Find(L"sounds\\eat.wav"); if (!p.empty()) g_snd_eat = p;
    p = Find(L"sounds\\bonus.wav"); if (!p.empty()) g_snd_bonus = p;
    p = Find(L"sounds\\dead.wav"); if (!p.empty()) g_snd_dead = p;
}

static void PlayFile(const std::wstring &p)
{
    if (!p.empty()) PlaySoundW(p.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

static void SoundEat(FoodKind k)
{
    if (k == FOOD_APPLE) { if (!g_snd_eat.empty()) PlayFile(g_snd_eat); else Beep(1000, 25); }
    else { if (!g_snd_bonus.empty()) PlayFile(g_snd_bonus); else Beep(1400, 45); }
}

static void SoundDeath()
{
    if (!g_snd_dead.empty()) PlayFile(g_snd_dead); else Beep(180, 120);
}

static void MusicInit()
{
    std::wstring d = ExeDir(), path;
    const wchar_t *rel[] = { L"kldykxq.mp3", L"..\\kldykxq.mp3", L"..\\..\\kldykxq.mp3", L"..\\..\\..\\kldykxq.mp3" };
    for (int i = 0; i < 4; i++) { std::wstring p = d + rel[i]; if (Exists(p)) { path = p; break; } }
    if (path.empty()) return;
    std::wstring cmd = L"open \"" + path + L"\" type mpegvideo alias bgm";
    if (mciSendStringW(cmd.c_str(), NULL, 0, NULL) != 0) return;
    g_music_open = g_music_ok = true;
    if (mciSendStringW(L"play bgm repeat", NULL, 0, NULL) == 0) g_music_on = true;
}

static void MusicToggle()
{
    if (!g_music_ok) return;
    if (g_music_on) { if (mciSendStringW(L"pause bgm", NULL, 0, NULL) == 0) g_music_on = false; }
    else {
        if (mciSendStringW(L"resume bgm", NULL, 0, NULL) != 0) {
            if (mciSendStringW(L"play bgm repeat", NULL, 0, NULL) == 0) g_music_on = true;
        } else g_music_on = true;
    }
}

static void MusicClose()
{
    if (g_music_open) mciSendStringW(L"close bgm", NULL, 0, NULL);
    g_music_open = false;
}

// ---------- scene ----------
static const wchar_t *StateText(GameState s)
{
    switch (s) {
    case ST_PLAYING: return L"游玩中";
    case ST_PAUSED: return L"已暂停";
    case ST_DEAD: return L"已结束";
    default: return L"主菜单";
    }
}

static Color StateCol(GameState s)
{
    switch (s) {
    case ST_PLAYING: return C(90, 240, 150);
    case ST_PAUSED: return C(255, 210, 90);
    case ST_DEAD: return C(255, 90, 90);
    default: return C(140, 160, 190);
    }
}

static void DrawBackground(Graphics &g)
{
    LinearGradientBrush bg(RectF(0, 0, (REAL)WIN_W, (REAL)WIN_H), C(8, 12, 22), C(14, 22, 38), LinearGradientModeVertical);
    g.FillRectangle(&bg, 0, 0, WIN_W, WIN_H);

    GraphicsPath glow;
    glow.AddEllipse(FX - 100, FY - 90, FW + 200, FH + 180);
    PathGradientBrush pgb(&glow);
    pgb.SetCenterColor(C(42, 40, 120, 90));
    Color surround(0, 0, 0, 0);
    int n = 1;
    pgb.SetSurroundColors(&surround, &n);
    g.FillPath(&pgb, &glow);
}

static void DrawField(Graphics &g)
{
    RectF outer(FX - 12, FY - 12, FW + 24, FH + 24);
    FillRound(g, outer, 18, C(11, 18, 32));
    StrokeRound(g, outer, 18, C(32, 46, 70), 2);
    FillRound(g, RectF(FX, FY, FW, FH), 12, C(8, 13, 24));

    Pen grid(C(18, 22, 38, 70), 1);
    for (int i = 0; i <= FIELD_W; i++) {
        float x = FX + i * CELL;
        g.DrawLine(&grid, x, FY, x, FY + FH);
    }
    for (int i = 0; i <= FIELD_H; i++) {
        float y = FY + i * CELL;
        g.DrawLine(&grid, FX, y, FX + FW, y);
    }
    TL(g, L"ARENA", f_small, C(90, 110, 140), FX + 4, FY - 24);
    TR(g, L"28 x 22", f_small, C(90, 110, 140), FX + FW, FY - 24);
}

static void DrawFood(Graphics &g, const Food &f, double now)
{
    if (!f.active) return;
    float cx = FX + f.x * CELL + CELL / 2;
    float cy = FY + f.y * CELL + CELL / 2;
    float bob = sinf((float)(now / 380.0) + f.x * 1.7f) * 1.6f;
    float sz = CELL * 1.12f;

    Color glow = f.kind == FOOD_BLUE ? C(70, 80, 220, 255) : C(70, 255, 80, 90);
    SolidBrush gb(glow);
    g.FillEllipse(&gb, RectF(cx - sz * 0.55f, cy - sz * 0.55f, sz * 1.1f, sz * 1.1f));
    SolidBrush sh(C(100, 0, 0, 0));
    g.FillEllipse(&sh, RectF(cx - sz * 0.35f, cy + CELL * 0.30f, sz * 0.7f, 7));

    Image *im = f.kind == FOOD_BLUE ? img_ice : f.kind == FOOD_RED ? img_stop : img_apple;
    Emoji(g, im, cx, cy + bob, sz, f.kind == FOOD_BLUE ? C(80, 200, 255) : C(255, 80, 80));
}

static void DrawObstacles(Graphics &g, const Game &gm, double now)
{
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        const Obstacle &o = gm.obstacles[i];
        float cx = FX + o.x * CELL + CELL / 2;
        float cy = FY + o.y * CELL + CELL / 2;
        float sz = CELL * 1.12f;
        if (o.warning) {
            int a = 120 + (int)(100 * (0.5 + 0.5 * sin(now / 110.0)));
            Pen ring(Color((BYTE)a, 255, 70, 70), 2.2f);
            g.DrawEllipse(&ring, RectF(cx - sz * 0.62f, cy - sz * 0.62f, sz * 1.24f, sz * 1.24f));
        }
        Emoji(g, img_barber, cx, cy, sz, C(220, 120, 120));
    }
}

static void DrawSnake(Graphics &g, const Game &gm, double now)
{
    if (!gm.head) return;

    GraphicsPath path;
    float hx = 0, hy = 0, tx = 0, ty = 0;
    int i = 0;
    for (const SnakeNode *n = gm.head; n; n = n->next, i++) {
        float x = FX + n->x * CELL + CELL / 2;
        float y = FY + n->y * CELL + CELL / 2;
        if (i == 0) { hx = x; hy = y; path.AddLine(x, y, x + 0.01f, y); }
        else path.AddLine(tx, ty, x, y);
        tx = x; ty = y;
    }

    bool flash = gm.state == ST_DEAD && g_death_at >= 0 && (now - g_death_at) < 700 &&
                 ((int)((now - g_death_at) / 90) & 1);
    Color dark = flash ? C(120, 40, 20) : C(8, 70, 42);
    Color c1 = flash ? C(255, 110, 110) : C(125, 255, 175);
    Color c2 = flash ? C(180, 30, 30) : C(24, 135, 72);

    Pen op(dark, CELL * 0.88f);
    op.SetLineJoin(LineJoinRound); op.SetStartCap(LineCapRound); op.SetEndCap(LineCapRound);
    g.DrawPath(&op, &path);

    LinearGradientBrush body(PointF(hx, hy), PointF(tx, ty), c1, c2);
    Pen bp(&body, CELL * 0.66f);
    bp.SetLineJoin(LineJoinRound); bp.SetStartCap(LineCapRound); bp.SetEndCap(LineCapRound);
    g.DrawPath(&bp, &path);

    Pen hi(Color(80, 210, 255, 225), CELL * 0.16f);
    hi.SetStartCap(LineCapRound); hi.SetEndCap(LineCapRound);
    g.DrawPath(&hi, &path);

    float r = CELL * 0.47f;
    SolidBrush head(flash ? C(255, 90, 90) : C(120, 255, 172));
    g.FillEllipse(&head, RectF(hx - r, hy - r, r * 2, r * 2));
    Pen hp(dark, 2);
    g.DrawEllipse(&hp, RectF(hx - r, hy - r, r * 2, r * 2));

    float dx = 0, dy = 0;
    switch (g_face) {
    case DIR_UP: dy = -1; break;
    case DIR_DOWN: dy = 1; break;
    case DIR_LEFT: dx = -1; break;
    default: dx = 1; break;
    }
    float ex = dx * 5.5f, ey = dy * 5.5f;
    float px = -dy * 5.5f, py = dx * 5.5f;
    SolidBrush eb(C(10, 20, 30));
    g.FillEllipse(&eb, RectF(hx + ex + px - 3, hy + ey + py - 3, 6, 6));
    g.FillEllipse(&eb, RectF(hx + ex - px - 3, hy + ey - py - 3, 6, 6));
}

static void SpawnEat(const Game &gm)
{
    if (gm.last_eat_x < 0) return;
    float cx = FX + gm.last_eat_x * CELL + CELL / 2;
    float cy = FY + gm.last_eat_y * CELL + CELL / 2;
    Color col = gm.last_eat_kind == FOOD_BLUE ? C(90, 210, 255) :
                gm.last_eat_kind == FOOD_RED ? C(255, 90, 90) : C(255, 120, 150);
    for (int i = 0; i < 16; i++) {
        Particle p;
        p.x = cx; p.y = cy;
        float a = (float)(rand() % 628) / 100.0f;
        float s = 20 + (float)(rand() % 80);
        p.vx = cosf(a) * s; p.vy = sinf(a) * s;
        p.maxlife = 0.45f + (float)(rand() % 40) / 100.0f;
        p.life = p.maxlife;
        p.size = 2 + (float)(rand() % 4);
        p.color = i % 4 == 0 ? C(255, 255, 255) : col;
        g_parts.push_back(p);
    }
    FloatText ft;
    ft.x = cx; ft.y = cy - 10; ft.life = 0.9f; ft.color = col;
    int gain = game_value_of(gm.last_eat_kind) + (gm.speed - 1) * 5;
    _snwprintf(ft.text, 16, L"+%d", gain);
    ft.text[15] = 0;
    g_floats.push_back(ft);
}

static void UpdateFx(double dt)
{
    for (size_t i = 0; i < g_parts.size();) {
        Particle &p = g_parts[i];
        p.life -= (float)dt; p.x += p.vx * (float)dt; p.y += p.vy * (float)dt;
        p.vy += 55 * (float)dt;
        if (p.life <= 0) { g_parts[i] = g_parts.back(); g_parts.pop_back(); }
        else i++;
    }
    for (size_t i = 0; i < g_floats.size();) {
        FloatText &f = g_floats[i];
        f.life -= (float)dt; f.y -= 22 * (float)dt;
        if (f.life <= 0) { g_floats[i] = g_floats.back(); g_floats.pop_back(); }
        else i++;
    }
}

static void DrawFx(Graphics &g)
{
    for (const Particle &p : g_parts) {
        int a = (int)(255 * (p.life / p.maxlife));
        if (a < 0) a = 0;
        SolidBrush b(Color((BYTE)a, p.color.GetR(), p.color.GetG(), p.color.GetB()));
        g.FillEllipse(&b, RectF(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size));
    }
    for (const FloatText &f : g_floats) {
        int a = (int)(255 * (f.life / 0.9f));
        if (a < 0) a = 0;
        TC(g, f.text, f_prompt, Color((BYTE)a, f.color.GetR(), f.color.GetG(), f.color.GetB()),
           RectF(f.x - 50, f.y - 12, 100, 24));
    }
}

static void DrawTop(Graphics &g)
{
    TL(g, L"贪吃蛇", f_title, C(120, 255, 170), 30, 10);
    TL(g, L"SNAKE HD", f_sub, C(140, 160, 195), 150, 18);
    TC(g, StateText(g_game.state), f_state, StateCol(g_game.state), RectF(0, 12, (REAL)WIN_W, 30));
    wchar_t b[64];
    _snwprintf(b, 64, L"BEST %06d", g_game.best); b[63] = 0;
    TR(g, b, f_sub, C(255, 210, 90), 1070, 16);
    Pen line(C(30, 34, 54), 1);
    g.DrawLine(&line, 30, 48, 1070, 48);
}

static void DrawFooter(Graphics &g)
{
    TC(g, L"方向键 / WASD 移动     空格 暂停     F3 / F4 调速     M 音乐     Esc 返回菜单",
       f_small, C(150, 165, 190), RectF(0, 654, (REAL)WIN_W, 22));
    TC(g, L"规则：吃食物变长 · 撞墙 / 自身 / 障碍物结束 · 障碍物每 9 秒换位 · 速度每档 +5 分",
       f_small, C(100, 115, 140), RectF(0, 676, (REAL)WIN_W, 16));
}

static void DrawHud(Graphics &g)
{
    FillRound(g, RectF(HX, HY, HW, HH), 18, C(17, 24, 39));
    StrokeRound(g, RectF(HX, HY, HW, HH), 18, C(34, 46, 68), 2);

    TL(g, L"STATUS", f_hud, C(100, 120, 150), HX + 20, HY + 14);
    TL(g, L"SCORE", f_label, C(100, 120, 150), HX + 20, HY + 44);
    TR(g, L"得分", f_label, C(100, 120, 150), HX + HW - 20, HY + 44);

    wchar_t b[64];
    int score = g_game.score > 99999 ? 99999 : g_game.score;
    _snwprintf(b, 64, L"%05d", score); b[63] = 0;
    TL(g, b, f_score, C(90, 235, 140), HX + 16, HY + 62);

    TL(g, L"BEST", f_label, C(100, 120, 150), HX + 20, HY + 148);
    _snwprintf(b, 64, L"%06d", g_game.best); b[63] = 0;
    TR(g, b, f_value, C(255, 210, 90), HX + HW - 20, HY + 140);

    TL(g, L"LEN", f_label, C(100, 120, 150), HX + 20, HY + 178);
    _snwprintf(b, 64, L"%d", g_game.len); b[63] = 0;
    TR(g, b, f_value, C(210, 225, 245), HX + HW - 20, HY + 170);

    TL(g, L"SPEED", f_label, C(100, 120, 150), HX + 20, HY + 208);
    for (int i = 0; i < 5; i++)
        FillRound(g, RectF(HX + 86 + i * 22, HY + 212, 14, 12), 3,
                  i < g_game.speed ? C(90, 235, 140) : C(40, 50, 68));
    _snwprintf(b, 64, L"%d/5", g_game.speed); b[63] = 0;
    TR(g, b, f_value, C(210, 225, 245), HX + HW - 20, HY + 204);

    Pen line(C(34, 46, 68), 1);
    g.DrawLine(&line, HX + 20, HY + 244, HX + HW - 20, HY + 244);
    TL(g, L"食物 FOOD", f_hud, C(100, 120, 150), HX + 20, HY + 262);

    float y = HY + 294;
    Emoji(g, img_apple, HX + 34, y + 15, 28, C(255, 80, 80));
    TL(g, L"苹果   +10", f_emoji, C(210, 225, 245), HX + 58, y + 4);
    y += 40;
    Emoji(g, img_ice, HX + 34, y + 15, 28, C(80, 200, 255));
    TL(g, L"蓝宝石 +30", f_emoji, C(210, 225, 245), HX + 58, y + 4);
    y += 40;
    Emoji(g, img_stop, HX + 34, y + 15, 28, C(255, 80, 80));
    TL(g, L"红宝石 +25", f_emoji, C(210, 225, 245), HX + 58, y + 4);

    TL(g, L"MUSIC", f_label, C(100, 120, 150), HX + 20, HY + 422);
    if (!g_music_ok) TR(g, L"NO MP3", f_label, C(100, 120, 150), HX + HW - 20, HY + 422);
    else TR(g, g_music_on ? L"ON" : L"OFF", f_label,
            g_music_on ? C(90, 220, 255) : C(100, 120, 150), HX + HW - 20, HY + 422);

    g.DrawLine(&line, HX + 20, HY + 452, HX + HW - 20, HY + 452);
    TL(g, L"方向键 / WASD", f_small, C(150, 165, 190), HX + 20, HY + 470);
    TL(g, L"空格 / P   暂停", f_small, C(150, 165, 190), HX + 20, HY + 492);
    TL(g, L"F3 / F4     调速", f_small, C(150, 165, 190), HX + 20, HY + 514);
    TL(g, L"M           音乐", f_small, C(150, 165, 190), HX + 20, HY + 536);
    TL(g, L"Esc         菜单", f_small, C(150, 165, 190), HX + 20, HY + 558);
}

static void DrawMenu(Graphics &g, double now)
{
    DrawBackground(g);
    float bob = sinf((float)(now / 500.0)) * 6;
    Emoji(g, img_apple, 120, 200 + bob, 72, C(255, 80, 80));
    Emoji(g, img_ice, 980, 200 - bob, 72, C(80, 200, 255));
    Emoji(g, img_stop, 120, 450 - bob, 72, C(255, 80, 80));
    Emoji(g, img_barber, 980, 450 + bob, 72, C(220, 120, 120));

    TC(g, L"SNAKE", f_menu, C(10, 40, 26), RectF(4, 104, (REAL)WIN_W, 130));
    TC(g, L"SNAKE", f_menu, C(120, 255, 175), RectF(0, 100, (REAL)WIN_W, 130));
    TC(g, L"贪 吃 蛇", f_menu_sub, C(255, 210, 90), RectF(0, 232, (REAL)WIN_W, 46));
    TC(g, L"SNAKE HD · GDI+ 真彩色 emoji 版", f_small, C(140, 160, 190),
       RectF(0, 280, (REAL)WIN_W, 22));

    wchar_t b[64];
    _snwprintf(b, 64, L"历史最高   %06d", g_game.best); b[63] = 0;
    TC(g, b, f_prompt, C(220, 235, 255), RectF(0, 320, (REAL)WIN_W, 30));

    RectF btn(370, 370, 360, 60);
    FillRound(g, btn, 17, C(34, 197, 94));
    StrokeRound(g, btn, 17, C(90, 255, 170), 2);
    TC(g, L"ENTER  开始游戏", f_prompt, C(8, 20, 14), btn);

    TC(g, L"方向键 / WASD 移动   ·   空格 暂停   ·   F3 / F4 调速   ·   M 音乐",
       f_small, C(150, 165, 190), RectF(0, 470, (REAL)WIN_W, 22));
    TC(g, L"28 x 22 竞技场   ·   3 个游走障碍   ·   苹果 / 蓝宝石 / 红宝石",
       f_small, C(100, 115, 140), RectF(0, 496, (REAL)WIN_W, 22));
    TL(g, L"SNAKE HD v3 · GDI+", f_small, C(80, 95, 120), 30, 662);
    TR(g, L"2023141231  王瑞龄", f_small, C(80, 95, 120), 1070, 662);
}

static void DrawPause(Graphics &g)
{
    SolidBrush dim(C(0, 0, 0, 170));
    g.FillRectangle(&dim, 0, 0, WIN_W, WIN_H);
    RectF p(330, 220, 440, 250);
    FillRound(g, p, 20, C(17, 24, 39));
    StrokeRound(g, p, 20, C(80, 96, 130), 2);
    TC(g, L"PAUSED", f_over, C(255, 210, 90), RectF(330, 240, 440, 64));
    wchar_t b[64];
    _snwprintf(b, 64, L"本局 %05d     长度 %d", g_game.score, g_game.len); b[63] = 0;
    TC(g, b, f_prompt, C(220, 235, 255), RectF(330, 320, 440, 28));
    TC(g, L"按 空格 继续", f_prompt, C(220, 235, 255), RectF(330, 370, 440, 28));
    TC(g, L"Esc 返回主菜单", f_small, C(150, 165, 190), RectF(330, 405, 440, 22));
}

static void DrawOver(Graphics &g, double now)
{
    /* 结算整屏不透明，避免右侧 HUD 露出 */
    SolidBrush black(C(0, 0, 0, 255));
    g.FillRectangle(&black, 0, 0, WIN_W, WIN_H);

    RectF p(260, 90, 580, 480);
    FillRound(g, p, 22, C(17, 24, 39));
    StrokeRound(g, p, 22, C(120, 40, 40), 2.5f);

    TC(g, L"GAME OVER", f_over, C(255, 80, 80), RectF(260, 115, 580, 70));
    TC(g, game_death_text(g_game.death), f_prompt, C(220, 235, 255), RectF(260, 195, 580, 28));

    wchar_t b[64];
    int s = g_game.score > 99999 ? 99999 : g_game.score;
    _snwprintf(b, 64, L"%05d", s); b[63] = 0;
    TC(g, b, f_big, C(90, 235, 140), RectF(260, 232, 580, 100));
    _snwprintf(b, 64, L"最高  %06d", g_game.best); b[63] = 0;
    TC(g, b, f_value, C(255, 210, 90), RectF(260, 345, 580, 26));

    if (g_game.new_record) {
        bool blink = ((int)(now / 380) & 1) != 0;
        TC(g, L"**  新 纪 录  **", f_prompt, blink ? C(255, 210, 90) : C(255, 255, 255),
           RectF(260, 384, 580, 26));
    } else {
        TC(g, L"再接再厉", f_small, C(140, 155, 180), RectF(260, 384, 580, 26));
    }
    TC(g, L"R 再来一局        Esc 返回主菜单", f_prompt, C(220, 235, 255),
       RectF(260, 425, 580, 26));

    Emoji(g, img_apple, 380, 500, 38, C(255, 80, 80));
    Emoji(g, img_ice, 480, 500, 38, C(80, 200, 255));
    Emoji(g, img_stop, 580, 500, 38, C(255, 80, 80));
    Emoji(g, img_barber, 680, 500, 38, C(220, 120, 120));
}

static void DrawScene(Graphics &g, double now)
{
    if (g_game.state == ST_MENU) {
        DrawMenu(g, now);
        return;
    }

    DrawBackground(g);
    DrawField(g);
    DrawFood(g, g_game.apple, now);
    DrawFood(g, g_game.bonus, now);
    DrawObstacles(g, g_game, now);
    DrawSnake(g, g_game, now);
    DrawFx(g);
    DrawTop(g);
    DrawHud(g);
    DrawFooter(g);

    if (g_game.state == ST_PAUSED) DrawPause(g);
    else if (g_game.state == ST_DEAD && g_death_at >= 0 && (now - g_death_at) >= 650)
        DrawOver(g, now);
}

// ---------- game / input ----------
static void ClearFx() { g_parts.clear(); g_floats.clear(); }

static void StartRound()
{
    game_start_round(&g_game);
    g_game.speed = 3;              /* 默认 3 档（155ms/步）；F3 可加速 */
    g_face = g_game.dir;
    g_acc = 0;
    g_death_at = -1;
    ClearFx();
    g_eat_seq = g_game.eat_seq;
    g_death_seq = g_game.death_seq;
}

static void ToMenu()
{
    game_to_menu(&g_game);
    g_acc = 0;
    g_death_at = -1;
    ClearFx();
}

static void Update(double dt)
{
    if (g_game.state == ST_PLAYING) {
        g_acc += dt;
        int step = game_step_ms(&g_game);
        int guard = 0;
        bool ticked = false;
        while (g_acc >= step && guard < 4) {
            game_tick(&g_game);
            ticked = true;
            g_acc -= step;
            step = game_step_ms(&g_game);
            guard++;
            if (g_game.state != ST_PLAYING) break;
        }
        if (ticked) g_face = g_game.dir;
    } else {
        g_acc = 0;
    }

    if (g_game.eat_seq != g_eat_seq) {
        SoundEat(g_game.last_eat_kind);
        SpawnEat(g_game);
        g_eat_seq = g_game.eat_seq;
    }
    if (g_game.death_seq != g_death_seq) {
        SoundDeath();
        g_death_at = g_now;
        g_death_seq = g_game.death_seq;
    }
    if (g_game.state != ST_DEAD) g_death_at = -1;
    if (g_game.state == ST_MENU) ClearFx();

    UpdateFx(dt);
}

static void Key(WPARAM vk)
{
    if (vk == 'M') { MusicToggle(); return; }
    switch (g_game.state) {
    case ST_MENU:
        if (vk == VK_RETURN || vk == VK_SPACE) StartRound();
        else if (vk == VK_ESCAPE || vk == 'Q') g_run = false;
        break;
    case ST_PLAYING:
        if (vk == VK_UP || vk == 'W') { if (game_queue_dir(&g_game, DIR_UP)) g_face = DIR_UP; }
        else if (vk == VK_DOWN || vk == 'S') { if (game_queue_dir(&g_game, DIR_DOWN)) g_face = DIR_DOWN; }
        else if (vk == VK_LEFT || vk == 'A') { if (game_queue_dir(&g_game, DIR_LEFT)) g_face = DIR_LEFT; }
        else if (vk == VK_RIGHT || vk == 'D') { if (game_queue_dir(&g_game, DIR_RIGHT)) g_face = DIR_RIGHT; }
        else if (vk == VK_SPACE || vk == 'P') game_toggle_pause(&g_game);
        else if (vk == VK_F3) game_adjust_speed(&g_game, 1);
        else if (vk == VK_F4) game_adjust_speed(&g_game, -1);
        else if (vk == VK_ESCAPE) ToMenu();
        break;
    case ST_PAUSED:
        if (vk == VK_SPACE || vk == 'P') game_toggle_pause(&g_game);
        else if (vk == VK_ESCAPE) ToMenu();
        else if (vk == VK_F3) game_adjust_speed(&g_game, 1);
        else if (vk == VK_F4) game_adjust_speed(&g_game, -1);
        break;
    case ST_DEAD:
        if (vk == 'R' || vk == VK_RETURN || vk == VK_SPACE) StartRound();
        else if (vk == VK_ESCAPE || vk == 'Q') ToMenu();
        break;
    }
}

static void Present()
{
    if (!g_hwnd || !g_canvas) return;
    HDC dc = GetDC(g_hwnd);
    if (!dc) return;
    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(g_canvas, 0, 0, WIN_W, WIN_H);
    ReleaseDC(g_hwnd, dc);
}

static void Render()
{
    if (!g_canvas) return;
    Graphics g(g_canvas);
    Setup(g);
    DrawScene(g, g_now);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) { Key(w); return 0; }
    if (m == WM_LBUTTONDOWN) { SetFocus(h); return 0; }
    if (m == WM_ACTIVATE && LOWORD(w) == WA_ACTIVE) { SetFocus(h); return 0; }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        Present();
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_DESTROY) { PostQuitMessage(0); g_run = false; return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void Loop()
{
    ULONGLONG last = GetTickCount64();
    while (g_run) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_run = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_run) break;
        ULONGLONG now = GetTickCount64();
        double dt = (double)(now - last);
        last = now;
        if (dt > 100) dt = 100;
        g_now = (double)now;
        Update(dt);
        Render();
        Present();
        Sleep(7);
    }
}

// ---------- preview ----------
static int Encoder(const WCHAR *mime, CLSID *cid)
{
    UINT n = 0, sz = 0;
    GetImageEncodersSize(&n, &sz);
    if (!sz) return -1;
    ImageCodecInfo *ci = (ImageCodecInfo *)malloc(sz);
    if (!ci) return -1;
    GetImageEncoders(n, sz, ci);
    int r = -1;
    for (UINT i = 0; i < n; i++) {
        if (wcscmp(ci[i].MimeType, mime) == 0) { *cid = ci[i].Clsid; r = (int)i; break; }
    }
    free(ci);
    return r;
}

static bool SavePng(Bitmap *b, const wchar_t *path)
{
    CLSID cid;
    if (Encoder(L"image/png", &cid) < 0) return false;
    return b->Save(path, &cid, NULL) == Ok;
}

static void Feed(Game *g, int x, int y)
{
    g->apple.active = true;
    g->apple.kind = FOOD_APPLE;
    g->apple.x = x;
    g->apple.y = y;
    game_tick(g);
}

static void Demo(Game *g)
{
    game_init(g);
    g->no_save = true;
    game_start_round(g);
    for (int y = 0; y < FIELD_H; y++)
        for (int x = 0; x < FIELD_W; x++)
            if (g->occ[y][x] == OCC_OBSTACLE) g->occ[y][x] = OCC_EMPTY;
    for (int i = 0; i < OBSTACLE_COUNT; i++) { g->obstacles[i].x = 0; g->obstacles[i].y = 0; }

    for (int i = 0; i < 8; i++) Feed(g, g->head->x + 1, g->head->y);
    game_queue_dir(g, DIR_UP);
    for (int i = 0; i < 4; i++) Feed(g, g->head->x, g->head->y - 1);
    game_queue_dir(g, DIR_LEFT);
    for (int i = 0; i < 8; i++) Feed(g, g->head->x - 1, g->head->y);
    game_queue_dir(g, DIR_DOWN);
    for (int i = 0; i < 2; i++) Feed(g, g->head->x, g->head->y + 1);
    game_queue_dir(g, DIR_RIGHT);
    for (int i = 0; i < 3; i++) Feed(g, g->head->x + 1, g->head->y);

    for (int y = 0; y < FIELD_H; y++)
        for (int x = 0; x < FIELD_W; x++)
            if (g->occ[y][x] == OCC_OBSTACLE) g->occ[y][x] = OCC_EMPTY;
    int ox[3] = { 5, 21, 13 }, oy[3] = { 15, 4, 17 };
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        g->obstacles[i].x = ox[i]; g->obstacles[i].y = oy[i];
        g->obstacles[i].warning = false;
        g->occ[oy[i]][ox[i]] = OCC_OBSTACLE;
    }
    g->apple.active = true; g->apple.kind = FOOD_APPLE; g->apple.x = 20; g->apple.y = 9;
    g->bonus.active = true; g->bonus.kind = FOOD_BLUE; g->bonus.x = 14; g->bonus.y = 14;
    g->bonus.expire_ms = 999999;
    g->score = 1280; g->best = 2048; g->speed = 3; g->apples_eaten = 25;
    g->next_obstacle_ms = 999999;
    g->state = ST_PLAYING;
    g->last_eat_x = -1;
    g->toast[0] = 0;
}

static int Dump(const wchar_t *path, const wchar_t *scene)
{
    Game game;
    double now = 2400;
    g_death_at = -1;

    if (wcscmp(scene, L"menu") == 0) {
        game_init(&game);
        game.best = 2048;
        game.state = ST_MENU;
    } else {
        Demo(&game);
        if (wcscmp(scene, L"pause") == 0) game.state = ST_PAUSED;
        else if (wcscmp(scene, L"dead") == 0) {
            game.state = ST_DEAD;
            game.death = DEATH_SELF;
            game.score = 1240;
            game.new_record = false;
            g_death_at = now - 1000;
        }
    }

    g_game = game;
    Bitmap bmp(WIN_W, WIN_H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    Setup(g);
    DrawScene(g, now);
    bool ok = SavePng(&bmp, path);
    game_free(&game);
    ZeroMemory(&g_game, sizeof(g_game));
    return ok ? 0 : 1;
}

// ---------- entry ----------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    SetProcessDPIAware();
    GdiplusStartupInput input;
    if (GdiplusStartup(&g_token, &input, NULL) != Ok) {
        MessageBoxW(NULL, L"GDI+ init failed", L"SNAKE HD", MB_OK);
        return 1;
    }
    LoadAll();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 3 && wcscmp(argv[1], L"--dump") == 0) {
        int rc = Dump(argv[2], argc >= 4 ? argv[3] : L"play");
        LocalFree(argv);
        MusicClose();
        PlaySoundW(NULL, NULL, 0);
        game_free(&g_game);
        delete g_canvas;
        delete f_title; delete f_sub; delete f_state; delete f_label; delete f_score;
        delete f_value; delete f_small; delete f_prompt; delete f_menu; delete f_menu_sub;
        delete f_over; delete f_big; delete f_hud; delete f_emoji;
        delete g_yh; delete g_cons; delete g_disp;
        delete img_apple; delete img_ice; delete img_stop; delete img_barber;
        GdiplusShutdown(g_token);
        return rc;
    }
    LocalFree(argv);

    game_init(&g_game);
    SoundInit();
    MusicInit();
    g_eat_seq = g_game.eat_seq;
    g_death_seq = g_game.death_seq;

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"SnakeGuiWnd2";
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&rc, style, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    g_hwnd = CreateWindowExW(0, L"SnakeGuiWnd2", L"贪吃蛇 · SNAKE HD",
                             style, sx, sy, ww, wh, NULL, NULL, hInst, NULL);
    if (!g_hwnd) {
        MessageBoxW(NULL, L"CreateWindow failed", L"SNAKE HD", MB_OK);
        return 1;
    }

    g_canvas = new Bitmap(WIN_W, WIN_H, PixelFormat32bppPARGB);
    ShowWindow(g_hwnd, SW_SHOW);
    BringWindowToTop(g_hwnd);
    SetForegroundWindow(g_hwnd);
    SetActiveWindow(g_hwnd);
    SetFocus(g_hwnd);
    UpdateWindow(g_hwnd);

    timeBeginPeriod(1);
    Loop();
    timeEndPeriod(1);

    MusicClose();
    PlaySoundW(NULL, NULL, 0);
    game_free(&g_game);
    delete g_canvas;

    delete f_title; delete f_sub; delete f_state; delete f_label; delete f_score;
    delete f_value; delete f_small; delete f_prompt; delete f_menu; delete f_menu_sub;
    delete f_over; delete f_big; delete f_hud; delete f_emoji;
    delete g_yh; delete g_cons; delete g_disp;
    delete img_apple; delete img_ice; delete img_stop; delete img_barber;
    GdiplusShutdown(g_token);
    return 0;
}
