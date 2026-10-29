/**
 * snake_hd - 贪吃蛇（高清控制台版）
 * 游戏数据与逻辑接口
 *
 * 与旧版相比：
 *   - 用 28x22 的逻辑网格代替像素/光标坐标，碰撞与渲染彻底解耦
 *   - 蛇身使用「头尾双指针链表」，每次移动零 malloc，杜绝逐帧分配
 *   - 用占用表 occ[][] 做 O(1) 碰撞检测，食物/障碍生成不再 goto 重试
 */
#ifndef SNAKE_HD_GAME_H
#define SNAKE_HD_GAME_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER       0x0601
#endif

#include <stdbool.h>
#include <wchar.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 场地尺寸（逻辑格） ---------- */
#define FIELD_W 28
#define FIELD_H 22
#define FIELD_CELLS (FIELD_W * FIELD_H)

#define OBSTACLE_COUNT 3
#define DIR_QUEUE      3

/* 占用表取值 */
#define OCC_EMPTY    0
#define OCC_SNAKE    1
#define OCC_OBSTACLE 2

typedef enum { DIR_UP = 0, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;

typedef enum {
    ST_MENU,      /* 主菜单 */
    ST_PLAYING,   /* 游戏中 */
    ST_PAUSED,    /* 暂停 */
    ST_DEAD       /* 本局结束，等待重开 */
} GameState;

typedef enum {
    FOOD_APPLE,   /* 苹果 +10 */
    FOOD_BLUE,    /* 蓝宝石 +30 */
    FOOD_RED      /* 红宝石 +25 */
} FoodKind;

typedef enum {
    DEATH_NONE = 0,
    DEATH_WALL,
    DEATH_SELF,
    DEATH_OBSTACLE
} DeathCause;

/* 蛇身节点：双向链表，尾部出队 O(1) */
typedef struct SnakeNode {
    int x, y;
    struct SnakeNode *prev, *next;
} SnakeNode;

typedef struct {
    bool active;
    int  x, y;
    FoodKind kind;
    int  expire_ms;      /* 仅奖励食物使用 */
} Food;

typedef struct {
    int  x, y;
    bool warning;        /* 即将换位，闪烁预警 */
} Obstacle;

typedef struct {
    /* --- 蛇 --- */
    SnakeNode *head, *tail;
    int len;
    unsigned char occ[FIELD_H][FIELD_W];

    /* --- 输入 --- */
    Dir dir;
    Dir pending[DIR_QUEUE];
    int pending_count;

    /* --- 食物 / 障碍 --- */
    Food apple;
    Food bonus;
    Obstacle obstacles[OBSTACLE_COUNT];

    /* --- 计分 / 节奏 --- */
    int score;
    int best;
    int speed;           /* 1..5 */
    int apples_eaten;
    int game_ms;         /* 仅统计游戏内时间（暂停不计） */
    int next_obstacle_ms;
    int obstacle_move_ms;
    bool obstacle_warning;

    /* --- 状态 --- */
    GameState state;
    DeathCause death;
    bool new_record;
    bool no_save;        /* 自测模式下不写最高分文件 */

    /* --- 供渲染层做特效 --- */
    int last_eat_x, last_eat_y, last_eat_ms;
    FoodKind last_eat_kind;

    /* --- 供音效层检测事件（只增不减） --- */
    unsigned int eat_seq;
    unsigned int death_seq;

    wchar_t toast[64];
    int toast_until;
} Game;

/* ---------- 生命周期 ---------- */
void game_init(Game *g);
void game_start_round(Game *g);
void game_to_menu(Game *g);
void game_free(Game *g);

/* ---------- 每帧逻辑 ---------- */
void game_tick(Game *g);
bool game_queue_dir(Game *g, Dir d);
void game_toggle_pause(Game *g);
void game_adjust_speed(Game *g, int delta);

/* ---------- 查询 ---------- */
int  game_step_ms(const Game *g);
const Food *game_food_at(const Game *g, int x, int y);
int  game_value_of(FoodKind k);
const wchar_t *game_death_text(DeathCause c);

/* ---------- 提示 / 存档 ---------- */
void game_set_toast(Game *g, const wchar_t *text);
bool game_load_best(Game *g);
void game_save_best(Game *g);
void game_exe_dir(wchar_t *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* SNAKE_HD_GAME_H */
