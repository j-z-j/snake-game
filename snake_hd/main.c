#include "game.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <mmsystem.h>

/* ============================================================
 * 控制台
 * ============================================================ */

static HANDLE g_hout = INVALID_HANDLE_VALUE;
static HANDLE g_hin = INVALID_HANDLE_VALUE;
static DWORD  g_old_in_mode = 0;
static bool   g_console_ok = false;

static LARGE_INTEGER g_freq;

static double now_ms(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)g_freq.QuadPart;
}

static bool set_font(short cx, short cy)
{
    CONSOLE_FONT_INFOEX fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.cbSize = sizeof(fi);
    if (!GetCurrentConsoleFontEx(g_hout, FALSE, &fi)) return false;

    wcscpy(fi.FaceName, L"Consolas");
    fi.dwFontSize.X = cx;
    fi.dwFontSize.Y = cy;
    return SetCurrentConsoleFontEx(g_hout, FALSE, &fi) != 0;
}

static void console_size_error(int bw, int bh, int vw, int vh)
{
    COORD home = { 0, 0 };
    DWORD written = 0;
    wchar_t msg[512];

    SetConsoleCursorPosition(g_hout, home);
    SetConsoleTextAttribute(g_hout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    _snwprintf(msg, 512,
        L"窗口太小，游戏无法正常显示。\r\n"
        L"当前缓冲区：%d x %d，可见窗口：%d x %d。\r\n"
        L"本游戏需要至少 100 x 29 字符。\r\n"
        L"请把窗口拉大，或把控制台字体调小后重新运行。\r\n\r\n"
        L"按任意键退出……\r\n", bw, bh, vw, vh);
    msg[511] = L'\0';
    WriteConsoleW(g_hout, msg, (DWORD)wcslen(msg), &written, NULL);

    FlushConsoleInputBuffer(g_hin);
    INPUT_RECORD rec;
    DWORD got;
    while (ReadConsoleInputW(g_hin, &rec, 1, &got) && got) {
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) break;
    }
}

static bool console_init(void)
{
    g_hout = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, 0, NULL);
    g_hin = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
    if (g_hout == INVALID_HANDLE_VALUE || g_hin == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "无法打开控制台。请在真实的控制台窗口中运行。\n");
        return false;
    }

    SetConsoleTitleW(L"贪吃蛇 · SNAKE HD");

    /* 尝试把缓冲区调到 100x29。即使失败（例如窗口比它大），只要实际尺寸 >=100x29 也能正常显示；
       不再先把窗口缩成 1x1，避免某些终端缩了之后不肯还原。 */
    COORD size = { SCREEN_W, SCREEN_H };
    SetConsoleScreenBufferSize(g_hout, size);

    /* 依次尝试字号，直到窗口能完整放下 100x29（照顾高 DPI / 小屏幕） */
    static const short fonts[][2] = { { 9, 18 }, { 8, 16 }, { 7, 14 }, { 6, 12 } };
    SMALL_RECT rc = { 0, 0, SCREEN_W - 1, SCREEN_H - 1 };
    bool fitted = false;
    for (int i = 0; i < 4 && !fitted; i++) {
        set_font(fonts[i][0], fonts[i][1]);
        COORD max = GetLargestConsoleWindowSize(g_hout);
        bool fits = (max.X <= 0 || (max.X >= SCREEN_W && max.Y >= SCREEN_H));
        if (fits) fitted = SetConsoleWindowInfo(g_hout, TRUE, &rc) != 0;
    }
    (void)fitted;   /* 有些终端（Windows Terminal）不允许程序改窗口大小，后面会检查实际尺寸 */

    render_apply_palette(g_hout);
    render_set_console(g_hout);

    /* 清空整个控制台缓冲区：防止上一个程序/上一次运行留下的文字、颜色块露在 100x29 之外 */
    CONSOLE_SCREEN_BUFFER_INFO bi;
    if (GetConsoleScreenBufferInfo(g_hout, &bi)) {
        DWORD cells = (DWORD)bi.dwSize.X * (DWORD)bi.dwSize.Y;
        COORD home = { 0, 0 };
        DWORD written = 0;
        FillConsoleOutputCharacterW(g_hout, L' ', cells, home, &written);
        FillConsoleOutputAttribute(g_hout, 0, cells, home, &written);
        SetConsoleCursorPosition(g_hout, home);

        int vw = bi.srWindow.Right - bi.srWindow.Left + 1;
        int vh = bi.srWindow.Bottom - bi.srWindow.Top + 1;
        if (bi.dwSize.X < SCREEN_W || bi.dwSize.Y < SCREEN_H ||
            vw < SCREEN_W || vh < SCREEN_H) {
            console_size_error(bi.dwSize.X, bi.dwSize.Y, vw, vh);
            return false;
        }
    }

    CONSOLE_CURSOR_INFO ci;
    if (GetConsoleCursorInfo(g_hout, &ci)) {
        ci.bVisible = FALSE;
        SetConsoleCursorInfo(g_hout, &ci);
    }

    if (GetConsoleMode(g_hin, &g_old_in_mode)) {
        DWORD mode = g_old_in_mode;
        mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT);
        mode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(g_hin, mode);
    }
    FlushConsoleInputBuffer(g_hin);
    g_console_ok = true;
    return true;
}

static void console_cleanup(void)
{
    if (!g_console_ok) return;

    if (g_old_in_mode) SetConsoleMode(g_hin, g_old_in_mode);

    CONSOLE_CURSOR_INFO ci;
    if (GetConsoleCursorInfo(g_hout, &ci)) {
        ci.bVisible = TRUE;
        SetConsoleCursorInfo(g_hout, &ci);
    }

    COORD home = { 0, 0 };
    DWORD written = 0;
    FillConsoleOutputCharacterW(g_hout, L' ', SCREEN_W * SCREEN_H, home, &written);
    FillConsoleOutputAttribute(g_hout,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        SCREEN_W * SCREEN_H, home, &written);
    SetConsoleCursorPosition(g_hout, home);

    if (g_hout != INVALID_HANDLE_VALUE) CloseHandle(g_hout);
    if (g_hin != INVALID_HANDLE_VALUE) CloseHandle(g_hin);
    g_hout = g_hin = INVALID_HANDLE_VALUE;
}

/* ============================================================
 * 背景音乐
 * ============================================================ */

static bool music_available = false;
static bool music_on = false;
static bool music_opened = false;

static bool file_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool build_path(wchar_t *out, size_t cap, const wchar_t *dir, const wchar_t *rel)
{
    size_t d = wcslen(dir);
    size_t r = wcslen(rel);
    if (d + r + 1 > cap) return false;
    wcscpy(out, dir);
    wcscat(out, rel);
    return true;
}

static void music_init(void)
{
    wchar_t dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    wchar_t cmd[600];

    game_exe_dir(dir, MAX_PATH);
    if (!dir[0]) return;

    if (!(build_path(path, MAX_PATH, dir, L"kldykxq.mp3") && file_exists(path)) &&
        !(build_path(path, MAX_PATH, dir, L"..\\kldykxq.mp3") && file_exists(path)) &&
        !(build_path(path, MAX_PATH, dir, L"..\\..\\kldykxq.mp3") && file_exists(path)) &&
        !(build_path(path, MAX_PATH, dir, L"..\\..\\..\\kldykxq.mp3") && file_exists(path))) {
        render_set_music(false, false);
        return;
    }

    wcscpy(cmd, L"open \"");
    wcscat(cmd, path);
    wcscat(cmd, L"\" type mpegvideo alias bgm");
    if (mciSendStringW(cmd, NULL, 0, NULL) != 0) {
        render_set_music(false, false);
        return;
    }

    music_opened = true;
    music_available = true;
    if (mciSendStringW(L"play bgm repeat", NULL, 0, NULL) == 0)
        music_on = true;
    render_set_music(music_available, music_on);
}

static void music_toggle(void)
{
    if (!music_available) return;

    if (music_on) {
        if (mciSendStringW(L"pause bgm", NULL, 0, NULL) == 0) music_on = false;
    } else {
        if (mciSendStringW(L"resume bgm", NULL, 0, NULL) != 0) {
            if (mciSendStringW(L"play bgm repeat", NULL, 0, NULL) == 0) music_on = true;
        } else {
            music_on = true;
        }
    }
    render_set_music(music_available, music_on);
}

static void music_close(void)
{
    if (music_opened) mciSendStringW(L"close bgm", NULL, 0, NULL);
    music_opened = false;
}

/* ============================================================
 * 音效：吃苹果 / 金果 / 死亡
 * ============================================================ */

static wchar_t g_snd_eat[MAX_PATH];
static wchar_t g_snd_bonus[MAX_PATH];
static wchar_t g_snd_dead[MAX_PATH];
static bool g_snd_eat_ok = false;
static bool g_snd_bonus_ok = false;
static bool g_snd_dead_ok = false;

static bool try_sound(const wchar_t *dir, const wchar_t *prefix,
                      const wchar_t *name, wchar_t *out)
{
    wchar_t rel[64];
    if (wcslen(prefix) + wcslen(name) + 1 > 64) return false;
    wcscpy(rel, prefix);
    wcscat(rel, name);
    return build_path(out, MAX_PATH, dir, rel) && file_exists(out);
}

static bool find_sound(const wchar_t *dir, const wchar_t *name, wchar_t *out)
{
    static const wchar_t *prefixes[] = {
        L"sounds\\", L"..\\sounds\\", L"..\\..\\sounds\\", L"..\\..\\..\\sounds\\",
        L"", L"..\\", L"..\\..\\"
    };
    for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); i++) {
        if (try_sound(dir, prefixes[i], name, out)) return true;
    }
    return false;
}

static void sound_init(void)
{
    wchar_t dir[MAX_PATH];
    game_exe_dir(dir, MAX_PATH);
    if (!dir[0]) return;

    g_snd_eat_ok = find_sound(dir, L"eat.wav", g_snd_eat);
    g_snd_bonus_ok = find_sound(dir, L"bonus.wav", g_snd_bonus);
    g_snd_dead_ok = find_sound(dir, L"dead.wav", g_snd_dead);
}

static void sound_play(const wchar_t *path, bool ok)
{
    if (ok && path[0]) {
        PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

static void sound_eat(FoodKind kind)
{
    if (kind == FOOD_APPLE) {
        if (g_snd_eat_ok) sound_play(g_snd_eat, true);
        else Beep(1000, 25);
    } else {
        if (g_snd_bonus_ok) sound_play(g_snd_bonus, true);
        else Beep(1400, 45);
    }
}

static void sound_death(void)
{
    if (g_snd_dead_ok) sound_play(g_snd_dead, true);
    else Beep(180, 120);
}

/* ============================================================
 * 输入
 * ============================================================ */

static bool handle_key(Game *g, WORD vk, wchar_t ch, bool *quit)
{
    /* 全局键：音乐 */
    if (vk == 'M') {
        music_toggle();
        return true;
    }

    switch (g->state) {
    case ST_MENU:
        if (vk == VK_RETURN || vk == VK_SPACE) {
            game_start_round(g);
        } else if (vk == VK_ESCAPE || vk == 'Q') {
            *quit = true;
        }
        break;

    case ST_PLAYING:
        if (vk == VK_UP || vk == 'W')          game_queue_dir(g, DIR_UP);
        else if (vk == VK_DOWN || vk == 'S')   game_queue_dir(g, DIR_DOWN);
        else if (vk == VK_LEFT || vk == 'A')   game_queue_dir(g, DIR_LEFT);
        else if (vk == VK_RIGHT || vk == 'D')  game_queue_dir(g, DIR_RIGHT);
        else if (vk == VK_SPACE || vk == 'P')  game_toggle_pause(g);
        else if (vk == VK_F3)                  game_adjust_speed(g, +1);
        else if (vk == VK_F4)                  game_adjust_speed(g, -1);
        else if (vk == VK_ESCAPE)              game_to_menu(g);
        break;

    case ST_PAUSED:
        if (vk == VK_SPACE || vk == 'P')       game_toggle_pause(g);
        else if (vk == VK_ESCAPE)              game_to_menu(g);
        else if (vk == VK_F3)                  game_adjust_speed(g, +1);
        else if (vk == VK_F4)                  game_adjust_speed(g, -1);
        break;

    case ST_DEAD:
        if (vk == 'R' || vk == VK_RETURN || vk == VK_SPACE) game_start_round(g);
        else if (vk == VK_ESCAPE || vk == 'Q')  game_to_menu(g);
        break;
    }

    (void)ch;
    return true;
}

static void poll_input(Game *g, bool *quit)
{
    DWORD count = 0;
    while (GetNumberOfConsoleInputEvents(g_hin, &count) && count > 0) {
        INPUT_RECORD rec;
        DWORD got = 0;
        if (!ReadConsoleInputW(g_hin, &rec, 1, &got) || got == 0) break;
        if (rec.EventType != KEY_EVENT) continue;

        const KEY_EVENT_RECORD *k = &rec.Event.KeyEvent;
        if (!k->bKeyDown) continue;

        handle_key(g, k->wVirtualKeyCode, k->uChar.UnicodeChar, quit);
        if (*quit) return;
    }
}

/* ============================================================
 * 自测：不开控制台，随机跑逻辑并检查不变量
 * ============================================================ */

static bool check_invariants(const Game *g)
{
    int snake_cells = 0, obstacle_cells = 0;
    bool seen[FIELD_H][FIELD_W];

    memset(seen, 0, sizeof(seen));
    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            if (g->occ[y][x] == OCC_SNAKE) snake_cells++;
            else if (g->occ[y][x] == OCC_OBSTACLE) obstacle_cells++;
        }
    }

    int count = 0;
    for (const SnakeNode *n = g->head; n; n = n->next) {
        count++;
        if (n->x < 0 || n->x >= FIELD_W || n->y < 0 || n->y >= FIELD_H) return false;
        if (g->occ[n->y][n->x] != OCC_SNAKE) return false;
        if (seen[n->y][n->x]) return false;
        seen[n->y][n->x] = true;
        if (n->next && n->next->prev != n) return false;
    }
    if (count != g->len || snake_cells != g->len) return false;
    if (g->tail && g->tail->next) return false;
    if (g->head && g->head->prev) return false;

    int obs = 0;
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        int x = g->obstacles[i].x, y = g->obstacles[i].y;
        if (x < 0 || x >= FIELD_W || y < 0 || y >= FIELD_H) return false;
        if (g->occ[y][x] != OCC_OBSTACLE) return false;
        obs++;
    }
    if (obs != obstacle_cells) return false;

    if (g->apple.active) {
        if (g->apple.x < 0 || g->apple.x >= FIELD_W || g->apple.y < 0 || g->apple.y >= FIELD_H) return false;
        if (g->occ[g->apple.y][g->apple.x] != OCC_EMPTY) return false;
    }
    if (g->bonus.active) {
        if (g->bonus.x < 0 || g->bonus.x >= FIELD_W || g->bonus.y < 0 || g->bonus.y >= FIELD_H) return false;
        if (g->occ[g->bonus.y][g->bonus.x] != OCC_EMPTY) return false;
        if (g->apple.active && g->apple.x == g->bonus.x && g->apple.y == g->bonus.y) return false;
    }
    if (g->score < 0) return false;
    return true;
}

static Dir opposite_dir(Dir d)
{
    switch (d) {
    case DIR_UP:    return DIR_DOWN;
    case DIR_DOWN:  return DIR_UP;
    case DIR_LEFT:  return DIR_RIGHT;
    default:        return DIR_LEFT;
    }
}

/* 自测用的贪心“电脑玩家”：朝最近的食物走，避开立即碰撞 */
static Dir bot_dir(const Game *g)
{
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { -1, 1, 0, 0 };
    int tx = g->apple.active ? g->apple.x : -1;
    int ty = g->apple.active ? g->apple.y : -1;

    if (g->bonus.active) {
        int d1 = g->apple.active ? abs(g->bonus.x - g->head->x) + abs(g->bonus.y - g->head->y) : 9999;
        int d0 = g->apple.active ? abs(tx - g->head->x) + abs(ty - g->head->y) : 9999;
        if (!g->apple.active || d1 <= d0) {
            tx = g->bonus.x;
            ty = g->bonus.y;
        }
    }
    if (tx < 0) return g->dir;

    Dir best = g->dir;
    int best_d = 1 << 30;
    for (int d = 0; d < 4; d++) {
        if (d == (int)opposite_dir(g->dir)) continue;
        int nx = g->head->x + dx[d];
        int ny = g->head->y + dy[d];
        if (nx < 0 || nx >= FIELD_W || ny < 0 || ny >= FIELD_H) continue;
        if (g->occ[ny][nx] != OCC_EMPTY) continue;
        int dist = abs(nx - tx) + abs(ny - ty);
        if (dist < best_d) {
            best_d = dist;
            best = (Dir)d;
        }
    }
    return best;
}

static int run_selftest(void)
{
    Game g;
    int rounds = 0;
    int max_len = 0, max_score = 0;

    game_init(&g);
    g.no_save = true;
    game_start_round(&g);
    rounds++;

    for (int i = 0; i < 40000; i++) {
        if (g.state == ST_MENU || g.state == ST_DEAD) {
            game_start_round(&g);
            rounds++;
        }
        if (i % 211 == 0) game_queue_dir(&g, (Dir)(rand() % 4));
        else if (g.pending_count == 0 && g.head)
            game_queue_dir(&g, bot_dir(&g));
        if (i % 991 == 0) game_toggle_pause(&g);

        game_tick(&g);
        if (g.len > max_len) max_len = g.len;
        if (g.score > max_score) max_score = g.score;

        if (!check_invariants(&g)) {
            fprintf(stderr, "selftest FAILED at tick %d (state=%d len=%d score=%d)\n",
                    i, (int)g.state, g.len, g.score);
            game_free(&g);
            return 1;
        }
    }

    printf("selftest OK: 40000 ticks, %d rounds, max len=%d, max score=%d\n",
           rounds, max_len, max_score);
    game_free(&g);
    return 0;
}

/* ============================================================
 * 预览帧构造（--dump 用，不参与正常游戏）
 * ============================================================ */

static void demo_feed(Game *g, int x, int y)
{
    g->apple.active = true;
    g->apple.kind = FOOD_APPLE;
    g->apple.x = x;
    g->apple.y = y;
    game_tick(g);
}

static void build_demo_play(Game *g)
{
    game_init(g);
    g->no_save = true;
    game_start_round(g);

    /* 先清掉随机障碍，避免它们挡在演示路径上（最后再摆到指定位置） */
    for (int y = 0; y < FIELD_H; y++)
        for (int x = 0; x < FIELD_W; x++)
            if (g->occ[y][x] == OCC_OBSTACLE) g->occ[y][x] = OCC_EMPTY;
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        g->obstacles[i].x = 0;
        g->obstacles[i].y = 0;
    }

    for (int i = 0; i < 8; i++) demo_feed(g, g->head->x + 1, g->head->y);
    game_queue_dir(g, DIR_UP);
    for (int i = 0; i < 4; i++) demo_feed(g, g->head->x, g->head->y - 1);
    game_queue_dir(g, DIR_LEFT);
    for (int i = 0; i < 8; i++) demo_feed(g, g->head->x - 1, g->head->y);
    game_queue_dir(g, DIR_DOWN);
    for (int i = 0; i < 2; i++) demo_feed(g, g->head->x, g->head->y + 1);
    game_queue_dir(g, DIR_RIGHT);
    for (int i = 0; i < 3; i++) demo_feed(g, g->head->x + 1, g->head->y);

    for (int y = 0; y < FIELD_H; y++)
        for (int x = 0; x < FIELD_W; x++)
            if (g->occ[y][x] == OCC_OBSTACLE) g->occ[y][x] = OCC_EMPTY;

    int ox[3] = { 5, 21, 13 };
    int oy[3] = { 15, 4, 17 };
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        g->obstacles[i].x = ox[i];
        g->obstacles[i].y = oy[i];
        g->obstacles[i].warning = false;
        g->occ[oy[i]][ox[i]] = OCC_OBSTACLE;
    }

    g->apple.active = true;
    g->apple.kind = FOOD_APPLE;
    g->apple.x = 20;
    g->apple.y = 9;
    g->bonus.active = true;
    g->bonus.kind = FOOD_BLUE;
    g->bonus.x = 14;
    g->bonus.y = 14;
    g->bonus.expire_ms = 999999;

    g->score = 1280;
    g->best = 2048;
    g->speed = 4;
    g->apples_eaten = 25;
    g->next_obstacle_ms = 999999;
    g->state = ST_PLAYING;
    g->last_eat_x = -1;
    g->last_eat_ms = -1;
    g->toast[0] = L'\0';
}

static void build_demo_menu(Game *g)
{
    game_init(g);
    g->best = 2048;
    g->state = ST_MENU;
}

/* ============================================================
 * 主循环
 * ============================================================ */

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    srand((unsigned)time(NULL) ^ (unsigned)GetTickCount());

    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0)
        return run_selftest();

    QueryPerformanceFrequency(&g_freq);

    if (argc >= 2 && strcmp(argv[1], "--dump") == 0) {
        Game g;
        const char *scene = argc >= 4 ? argv[3] : "play";
        const char *file = argc >= 3 ? argv[2] : "frame.txt";
        wchar_t wpath[512];

        MultiByteToWideChar(CP_UTF8, 0, file, -1, wpath, 512);
        if (strcmp(scene, "menu") == 0) {
            build_demo_menu(&g);
        } else if (strcmp(scene, "pause") == 0) {
            build_demo_play(&g);
            g.state = ST_PAUSED;
        } else if (strcmp(scene, "dead") == 0) {
            build_demo_play(&g);
            g.state = ST_DEAD;
            g.death = DEATH_SELF;
            g.score = 1240;
            g.new_record = false;
            g.last_eat_x = -1;
        } else {
            build_demo_play(&g);
        }

        render_set_music(true, true);
        if (g.state != ST_MENU) {
            /* 先渲染一帧菜单，再切回目标场景：用于验证整屏清屏没有残影 */
            GameState st = g.state;
            g.state = ST_MENU;
            render_draw(&g, 1000.0);
            g.state = st;
        }
        render_draw(&g, 2400.0);
        if (g.state == ST_DEAD) render_draw(&g, 3300.0);   /* 跳过死亡闪烁，直接出结算面板 */
        bool ok = render_dump(wpath);
        game_free(&g);
        if (!ok) {
            fprintf(stderr, "dump failed\n");
            return 1;
        }
        printf("dumped %s (%s)\n", file, scene);
        return 0;
    }

    if (!console_init()) return 1;
    timeBeginPeriod(1);
    music_init();
    sound_init();

    Game g;
    game_init(&g);

    bool quit = false;
    double last = now_ms();
    double acc = 0.0;
    unsigned int last_eat_seq = 0;
    unsigned int last_death_seq = 0;

    while (!quit) {
        double t = now_ms();
        double dt = t - last;
        last = t;

        poll_input(&g, &quit);
        if (quit) break;

        if (g.state == ST_PLAYING) {
            acc += dt;
            int step = game_step_ms(&g);
            int guard = 0;
            while (acc >= step && guard < 3) {
                game_tick(&g);
                acc -= step;
                step = game_step_ms(&g);
                guard++;
                if (g.state != ST_PLAYING) break;
            }
            if (acc > step * 2.0) acc = 0.0;
        } else {
            acc = 0.0;
        }

        if (g.eat_seq != last_eat_seq) {
            sound_eat(g.last_eat_kind);
            last_eat_seq = g.eat_seq;
        }
        if (g.death_seq != last_death_seq) {
            sound_death();
            last_death_seq = g.death_seq;
        }

        render_draw(&g, t);
        render_present();

        double used = now_ms() - t;
        if (used < 16.0) Sleep((DWORD)(16.0 - used));
    }

    PlaySoundW(NULL, NULL, 0);
    music_close();
    timeEndPeriod(1);
    game_free(&g);
    console_cleanup();
    return 0;
}
