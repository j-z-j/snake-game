#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* 方向向量：顺序必须与 Dir 一致 */
static const int DX[4] = { 0, 0, -1, 1 };
static const int DY[4] = { -1, 1, 0, 0 };

/* 5 档速度对应的每步毫秒数 */
static const int STEP_MS[5] = { 230, 190, 155, 125, 100 };

/* ============================================================
 * 基础工具
 * ============================================================ */

static SnakeNode *node_new(int x, int y)
{
    SnakeNode *n = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!n) return NULL;
    n->x = x;
    n->y = y;
    n->prev = NULL;
    n->next = NULL;
    return n;
}

static void list_push_front(Game *g, SnakeNode *n)
{
    n->prev = NULL;
    n->next = g->head;
    if (g->head) g->head->prev = n;
    else         g->tail = n;
    g->head = n;
    g->len++;
}

/* 摘下尾巴但不释放，便于“尾巴复用成新头” */
static SnakeNode *list_pop_tail(Game *g)
{
    SnakeNode *t = g->tail;
    if (!t) return NULL;
    g->tail = t->prev;
    if (g->tail) g->tail->next = NULL;
    else         g->head = NULL;
    t->prev = NULL;
    t->next = NULL;
    g->len--;
    return t;
}

void game_free(Game *g)
{
    SnakeNode *n = g->head;
    while (n) {
        SnakeNode *next = n->next;
        free(n);
        n = next;
    }
    g->head = NULL;
    g->tail = NULL;
    g->len = 0;
}

void game_exe_dir(wchar_t *out, size_t cap)
{
    DWORD n = GetModuleFileNameW(NULL, out, (DWORD)cap);
    if (n == 0 || n >= cap) {
        out[0] = L'\0';
        return;
    }
    wchar_t *p = wcsrchr(out, L'\\');
    if (p) p[1] = L'\0';
    else   out[0] = L'\0';
}

const wchar_t *game_death_text(DeathCause c)
{
    switch (c) {
    case DEATH_WALL:     return L"撞上了围墙";
    case DEATH_SELF:     return L"咬到了自己的身体";
    case DEATH_OBSTACLE: return L"撞上了障碍物";
    default:             return L"主动结束了本局";
    }
}

int game_value_of(FoodKind k)
{
    switch (k) {
    case FOOD_BLUE: return 30;
    case FOOD_RED:  return 25;
    default:        return 10;
    }
}

int game_step_ms(const Game *g)
{
    int s = g->speed;
    if (s < 1) s = 1;
    if (s > 5) s = 5;
    return STEP_MS[s - 1];
}

const Food *game_food_at(const Game *g, int x, int y)
{
    if (g->apple.active && g->apple.x == x && g->apple.y == y) return &g->apple;
    if (g->bonus.active && g->bonus.x == x && g->bonus.y == y) return &g->bonus;
    return NULL;
}

void game_set_toast(Game *g, const wchar_t *text)
{
    if (!text) return;
    wcsncpy(g->toast, text, 63);
    g->toast[63] = L'\0';
    g->toast_until = g->game_ms + 1800;
}

/* ============================================================
 * 最高分：文件放在 exe 同目录，避免工作目录不同导致“读不到”
 * ============================================================ */

static void path_join(wchar_t *out, size_t cap, const wchar_t *dir, const wchar_t *name)
{
    size_t d = wcslen(dir);
    if (d + wcslen(name) + 1 > cap) {
        out[0] = L'\0';
        return;
    }
    wcscpy(out, dir);
    wcscat(out, name);
}

bool game_load_best(Game *g)
{
    wchar_t dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    FILE *fp = NULL;
    int v = 0;

    game_exe_dir(dir, MAX_PATH);

    if (dir[0]) {
        path_join(path, MAX_PATH, dir, L"snake_hd_best.txt");
        if (path[0]) fp = _wfopen(path, L"r");
        if (!fp) {
            /* 兼容旧版数据文件名 */
            path_join(path, MAX_PATH, dir, L"high_score.txt");
            if (path[0]) fp = _wfopen(path, L"r");
        }
    }

    if (fp) {
        if (fscanf(fp, "%d", &v) == 1 && v > 0) g->best = v;
        fclose(fp);
        return true;
    }
    return false;
}

void game_save_best(Game *g)
{
    wchar_t dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    FILE *fp;

    if (g->no_save) return;
    game_exe_dir(dir, MAX_PATH);
    if (!dir[0]) return;

    path_join(path, MAX_PATH, dir, L"snake_hd_best.txt");
    if (!path[0]) return;
    fp = _wfopen(path, L"w");
    if (!fp) return;            /* 只读目录也不应让游戏崩溃 */
    fwprintf(fp, L"%d\n", g->best);
    fclose(fp);
}

/* ============================================================
 * 食物 / 障碍物生成
 * ============================================================ */

/* 收集空格子；avoid_head=true 时排除蛇头附近 2 格，避免“贴脸刷新” */
static int collect_free(const Game *g, int *xs, int *ys, bool avoid_head, bool avoid_bonus)
{
    int n = 0;
    int hx = g->head ? g->head->x : -99;
    int hy = g->head ? g->head->y : -99;

    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            if (g->occ[y][x] != OCC_EMPTY) continue;

            if (avoid_bonus && g->bonus.active && g->bonus.x == x && g->bonus.y == y)
                continue;
            if (g->apple.active && g->apple.x == x && g->apple.y == y)
                continue;

            if (avoid_head) {
                int d = abs(x - hx) + abs(y - hy);
                if (d < 3) continue;
            }
            xs[n] = x;
            ys[n] = y;
            n++;
        }
    }
    return n;
}

static void spawn_apple(Game *g)
{
    int xs[FIELD_CELLS], ys[FIELD_CELLS];
    int n = collect_free(g, xs, ys, true, true);
    if (n == 0) n = collect_free(g, xs, ys, false, true);
    if (n == 0) {
        g->apple.active = false;
        return;
    }
    int k = rand() % n;
    g->apple.active = true;
    g->apple.kind = FOOD_APPLE;
    g->apple.x = xs[k];
    g->apple.y = ys[k];
    g->apple.expire_ms = 0;
}

static void spawn_bonus(Game *g)
{
    int xs[FIELD_CELLS], ys[FIELD_CELLS];
    int n;

    if (g->bonus.active) return;
    n = collect_free(g, xs, ys, true, false);
    if (n == 0) n = collect_free(g, xs, ys, false, false);
    if (n == 0) return;

    int k = rand() % n;
    g->bonus.active = true;
    g->bonus.kind = (rand() % 2) ? FOOD_BLUE : FOOD_RED;
    g->bonus.x = xs[k];
    g->bonus.y = ys[k];
    g->bonus.expire_ms = g->game_ms + 10000;

    game_set_toast(g, g->bonus.kind == FOOD_BLUE ? L"蓝宝石出现了！" : L"红宝石出现了！");
}

/* 障碍物是否适合放在 (x,y)：离蛇头远、不在正前方、不贴其他障碍/食物 */
static bool obstacle_spot_ok(const Game *g, int x, int y, int self)
{
    if (g->occ[y][x] != OCC_EMPTY) return false;
    if (game_food_at(g, x, y)) return false;
    if (!g->head) return false;

    if (abs(x - g->head->x) + abs(y - g->head->y) < 5) return false;

    /* 不放在蛇头正前方 5 格，避免“刷新即死” */
    for (int k = 1; k <= 5; k++) {
        if (g->head->x + DX[g->dir] * k == x &&
            g->head->y + DY[g->dir] * k == y)
            return false;
    }

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        if (i == self) continue;
        if (abs(x - g->obstacles[i].x) + abs(y - g->obstacles[i].y) <= 2)
            return false;
    }
    return true;
}

static bool find_obstacle_spot(const Game *g, int self, int *out_x, int *out_y)
{
    for (int t = 0; t < 400; t++) {
        int x = rand() % FIELD_W;
        int y = rand() % FIELD_H;
        if (obstacle_spot_ok(g, x, y, self)) {
            *out_x = x;
            *out_y = y;
            return true;
        }
    }
    return false;
}

static void place_initial_obstacles(Game *g)
{
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        g->obstacles[i].warning = false;
        if (find_obstacle_spot(g, i, &g->obstacles[i].x, &g->obstacles[i].y))
            g->occ[g->obstacles[i].y][g->obstacles[i].x] = OCC_OBSTACLE;
        else {
            g->obstacles[i].x = 1;
            g->obstacles[i].y = 1;
            g->occ[1][1] = OCC_OBSTACLE;
        }
    }
}

static void relocate_obstacles(Game *g)
{
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        int ox = g->obstacles[i].x;
        int oy = g->obstacles[i].y;
        int nx, ny;

        g->occ[oy][ox] = OCC_EMPTY;
        if (find_obstacle_spot(g, i, &nx, &ny)) {
            g->obstacles[i].x = nx;
            g->obstacles[i].y = ny;
            g->occ[ny][nx] = OCC_OBSTACLE;
        } else {
            g->occ[oy][ox] = OCC_OBSTACLE;
        }
        g->obstacles[i].warning = false;
    }
}

/* ============================================================
 * 回合生命周期
 * ============================================================ */

void game_init(Game *g)
{
    memset(g, 0, sizeof(*g));
    g->state = ST_MENU;
    g->speed = 3;
    g->death = DEATH_NONE;
    game_load_best(g);
}

void game_to_menu(Game *g)
{
    game_free(g);
    g->state = ST_MENU;
    g->pending_count = 0;
    g->toast[0] = L'\0';
    g->toast_until = 0;
}

void game_start_round(Game *g)
{
    int saved_best = g->best;
    bool saved_no_save = g->no_save;

    game_free(g);
    memset(g->occ, 0, sizeof(g->occ));
    memset(&g->apple, 0, sizeof(g->apple));
    memset(&g->bonus, 0, sizeof(g->bonus));
    memset(g->obstacles, 0, sizeof(g->obstacles));

    g->dir = DIR_RIGHT;
    g->pending_count = 0;
    g->score = 0;
    g->best = saved_best;
    g->no_save = saved_no_save;
    g->speed = 3;
    g->apples_eaten = 0;
    g->game_ms = 0;
    g->next_obstacle_ms = 9000;
    g->obstacle_move_ms = 0;
    g->obstacle_warning = false;
    g->state = ST_PLAYING;
    g->death = DEATH_NONE;
    g->new_record = false;
    g->last_eat_x = -1;
    g->last_eat_y = -1;
    g->last_eat_ms = -1;
    g->last_eat_kind = FOOD_APPLE;
    g->toast[0] = L'\0';
    g->toast_until = 0;

    /* 初始 4 节，头在右 */
    int hy = FIELD_H / 2;
    for (int x = 5; x <= 8; x++) {
        SnakeNode *n = node_new(x, hy);
        if (!n) break;
        list_push_front(g, n);
        g->occ[hy][x] = OCC_SNAKE;
    }
    if (!g->head) {
        g->state = ST_MENU;
        return;
    }

    spawn_apple(g);
    place_initial_obstacles(g);
}

/* ============================================================
 * 移动与判定
 * ============================================================ */

static void die(Game *g, DeathCause cause)
{
    g->state = ST_DEAD;
    g->death = cause;
    g->new_record = false;
    g->death_seq++;

    if (g->score > g->best) {
        g->best = g->score;
        g->new_record = true;
        game_save_best(g);
    }
}

static void eat_food(Game *g, Food *f)
{
    int gain = game_value_of(f->kind) + (g->speed - 1) * 5;

    g->score += gain;
    g->eat_seq++;
    g->last_eat_x = f->x;
    g->last_eat_y = f->y;
    g->last_eat_ms = g->game_ms;
    g->last_eat_kind = f->kind;

    if (f == &g->apple) {
        f->active = false;
        g->apples_eaten++;
        spawn_apple(g);
        if (g->apples_eaten % 4 == 0) spawn_bonus(g);
    } else {
        f->active = false;
    }
}

void game_tick(Game *g)
{
    if (g->state != ST_PLAYING || !g->head) return;

    g->game_ms += game_step_ms(g);

    /* 奖励食物过期 */
    if (g->bonus.active && g->game_ms >= g->bonus.expire_ms) {
        g->bonus.active = false;
    }

    /* 应用缓存的转向（一帧内可连续转两次） */
    if (g->pending_count > 0) {
        g->dir = g->pending[0];
        for (int i = 1; i < g->pending_count; i++) g->pending[i - 1] = g->pending[i];
        g->pending_count--;
    }

    int nx = g->head->x + DX[g->dir];
    int ny = g->head->y + DY[g->dir];

    if (nx < 0 || nx >= FIELD_W || ny < 0 || ny >= FIELD_H) {
        die(g, DEATH_WALL);
        return;
    }

    Food *f = (Food *)game_food_at(g, nx, ny);
    bool grow = (f != NULL);

    /* 不吃食物时尾巴会让出格子，因此可以合法移动到“当前尾巴” */
    if (!grow) {
        g->occ[g->tail->y][g->tail->x] = OCC_EMPTY;
    }

    if (g->occ[ny][nx] != OCC_EMPTY) {
        if (!grow) g->occ[g->tail->y][g->tail->x] = OCC_SNAKE;
        die(g, g->occ[ny][nx] == OCC_OBSTACLE ? DEATH_OBSTACLE : DEATH_SELF);
        return;
    }

    if (grow) {
        SnakeNode *n = node_new(nx, ny);
        if (!n) {
            g->state = ST_DEAD;
            g->death = DEATH_NONE;
            return;
        }
        list_push_front(g, n);
    } else {
        SnakeNode *t = list_pop_tail(g);
        t->x = nx;
        t->y = ny;
        list_push_front(g, t);
    }
    g->occ[ny][nx] = OCC_SNAKE;

    if (f) eat_food(g, f);

    /* 障碍物：每 9 秒预警 1.4 秒后换位 */
    if (!g->obstacle_warning && g->game_ms >= g->next_obstacle_ms) {
        g->obstacle_warning = true;
        g->obstacle_move_ms = g->game_ms + 1400;
        for (int i = 0; i < OBSTACLE_COUNT; i++) g->obstacles[i].warning = true;
        game_set_toast(g, L"障碍物即将换位！");
        g->toast_until = g->game_ms + 1400;
    }
    if (g->obstacle_warning && g->game_ms >= g->obstacle_move_ms) {
        relocate_obstacles(g);
        g->obstacle_warning = false;
        g->next_obstacle_ms = g->game_ms + 9000;
    }
}

/* ============================================================
 * 输入 / 状态切换
 * ============================================================ */

static Dir opposite(Dir d)
{
    switch (d) {
    case DIR_UP:    return DIR_DOWN;
    case DIR_DOWN:  return DIR_UP;
    case DIR_LEFT:  return DIR_RIGHT;
    default:        return DIR_LEFT;
    }
}

bool game_queue_dir(Game *g, Dir d)
{
    if (g->state != ST_PLAYING) return false;
    if (g->pending_count >= DIR_QUEUE) return false;

    Dir last = g->pending_count ? g->pending[g->pending_count - 1] : g->dir;
    if (d == last || d == opposite(last)) return false;

    g->pending[g->pending_count++] = d;
    return true;
}

void game_toggle_pause(Game *g)
{
    if (g->state == ST_PLAYING) {
        g->state = ST_PAUSED;
        game_set_toast(g, L"已暂停");
    } else if (g->state == ST_PAUSED) {
        g->state = ST_PLAYING;
        game_set_toast(g, L"继续游戏");
    }
}

void game_adjust_speed(Game *g, int delta)
{
    if (g->state != ST_PLAYING && g->state != ST_PAUSED) return;
    int s = g->speed + delta;
    if (s < 1) s = 1;
    if (s > 5) s = 5;
    if (s == g->speed) return;
    g->speed = s;

    wchar_t buf[32];
    _snwprintf(buf, 32, L"速度 %d / 5", s);
    buf[31] = L'\0';
    game_set_toast(g, buf);
}
