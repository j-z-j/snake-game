#ifndef SNAKE_HD_RENDER_H
#define SNAKE_HD_RENDER_H

#include "game.h"

/* 控制台画布：100 x 29 字符（兼容默认 120x29 控制台窗口） */
#define SCREEN_W 100
#define SCREEN_H 29

void render_set_console(HANDLE hOut);
void render_apply_palette(HANDLE hOut);
void render_draw(const Game *g, double now_ms);
void render_present(void);
bool render_dump(const wchar_t *path);
void render_set_music(bool available, bool on);

#endif /* SNAKE_HD_RENDER_H */
