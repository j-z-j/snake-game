#include"snake.h"

// 设置控制台光标的位置
void setpos(short x, short y) {
    HANDLE houtput = NULL;
    houtput = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { x,y };
    SetConsoleCursorPosition(houtput, pos);
}


//颜色设置
void color(int c)
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

// 用于显示游戏开始时的欢迎界面信息，包括欢迎语、操作提示等，并进行一些简单的交互（暂停、清屏）
void welcomegame() {
    setpos(40, 14);
    wprintf(L"欢迎来到贪吃蛇\n");
    Sleep(1000);
    setpos(40, 15);
    system("pause");
    system("cls");
    setpos(26, 5);
    wprintf(L"用箭头控制移动方向，F3加速，F4减速\n");
    setpos(32, 6);
    wprintf(L"加速可以获得更高的分数\n");
    setpos(32,7);
    wprintf(L"按小键盘的“1”可以暂停 / 播放音乐");
    setpos(32,8);
    wprintf(L"按方向键控制蛇的移动");
    setpos(44, 20);
    system("pause");
    system("cls");
    system("color 2f");
}

// 绘制游戏地图的边界（围墙）
void print_map() {
    int i = 0;
    for (i = 0; i <= 56; i++) {
        wprintf(L"%lc", wall);
    }
    setpos(0, 26);
    for (i = 0; i <= 56; i++) {
        wprintf(L"%lc", wall);
    }
    for (i = 1; i <= 25; i++) {
        setpos(0, i);
        wprintf(L"%lc", wall);
    }
    for (i = 1; i <= 25; i++) {
        setpos(56, i);
        wprintf(L"%lc", wall);

    }
}

// 初始化蛇的相关属性和初始形态
void initsnake(snake* ps) {
    int i = 0;
    snakenode* cur = NULL;
    for (i = 0; i < 3; i++) {
        cur = (snakenode*)malloc(sizeof(snakenode));
        if (cur == NULL) {
            perror("malloc:snakenode");
            return;
        }
        else {
            cur->next = NULL;
            cur->x = pos_x + 2 * i;
            cur->y = pos_y;
            if (ps->psnake == NULL) {
                ps->psnake = cur;
            }
            else {
                cur->next = ps->psnake;
                ps->psnake = cur;
            }
        }
    }
    cur = ps->psnake;
    while (cur) {
        setpos(cur->x, cur->y);
        wprintf(L"%lc", body);
        cur = cur->next;
    }
    ps->dir = right;
    ps->status = ok;
    ps->foodscore = 10;
    ps->all_score = 0;
    ps->sleep_time = 200;
}


// 在游戏区域内随机生成障碍物的位置，确保不和蛇身、食物重合
void set_obstacles(snake* ps) {
    for (int i = 0; i < num_obstacles; i++) {
        int x = 0;
        int y = 0;
    again:
        x = (rand() % 26 + 1) * 2;
        y = rand() % 25 + 1;
        snakenode* cur = ps->psnake;
        // 检查与蛇身不重合
        while (cur!= NULL) {
            if (x == cur->x && y == cur->y) {
                goto again;
            }
            cur = cur->next;
        }
        // 检查与已有的食物不重合
        for (int j = 0; j < 2; j++) {
            if (x == foodPositions[j].x && y == foodPositions[j].y) {
                goto again;
            }
        }
        // 检查与已生成的其他障碍物不重合
        for (int k = 0; k < i; k++) {
            if (x == ps->pObstacles[k].x && y == ps->pObstacles[k].y) {
                goto again;
            }
        }
        ps->pObstacles[i].x = x;
        ps->pObstacles[i].y = y;
        // 初始化旧位置成员变量
        ps->pObstacles[i].old_x = x;
        ps->pObstacles[i].old_y = y;

        setpos(x, y);
        wprintf(L"X");  // 用 'X' 表示障碍物
    }
}

// 更新障碍物的位置，重新随机生成符合要求的位置，并更新显示
void update_obstacles(snake* ps) {
    for (int i = 0; i < num_obstacles; i++) {
        int x = 0;
        int y = 0;
    again:
        x = (rand() % 26 + 1) * 2;
        y = rand() % 25 + 1;
        snakenode* cur = ps->psnake;
        // 检查与蛇身不重合
        while (cur!= NULL) {
            if (x == cur->x && y == cur->y) {
                goto again;
            }
            cur = cur->next;
        }
        // 检查与已有的食物不重合
        for (int j = 0; j < 2; j++) {
            if (x == foodPositions[j].x && y == foodPositions[j].y) {
                goto again;
            }
        }
        // 记录新的障碍物位置
        ps->pObstacles[i].x = x;
        ps->pObstacles[i].y = y;

        // 先清除原来位置显示的障碍物（使用空格覆盖原来的显示字符）
        setpos(ps->pObstacles[i].old_x, ps->pObstacles[i].old_y);
        printf("  ");

        // 在新位置显示障碍物
        setpos(x, y);
        wprintf(L"X");

        // 记录当前位置为旧位置，方便下次更新时清除
        ps->pObstacles[i].old_x = x;
        ps->pObstacles[i].old_y = y;
    }
}


// 在游戏区域内随机生成食物的位置，并创建对应的食物节点，同时确保食物位置不与蛇身重合
void  set_food(snake* ps) {
    int i;
    // 生成第一个食物
    int x1 = 0;
    int y1 = 0;
again:
    x1 = (rand() % 26 + 1)*2;
    y1 = rand() % 25 + 1;
    snakenode* cur = ps->psnake;
    while (cur!= NULL) {
        if (x1 == cur->x && y1 == cur->y) {
            goto again;
        }
        cur = cur->next;
    }
    // 存储第一个食物的位置
    foodPositions[0].x = x1;
    foodPositions[0].y = y1;
    snakenode* pd1 = NULL;
    pd1 = (snakenode*)malloc(sizeof(snakenode));
    if (pd1 == NULL) {
        perror("malloc:set_food for first food");
        return;
    }
    else {
        pd1->next = NULL;
        pd1->x = x1;
        pd1->y = y1;
        ps->pfood = pd1;
        setpos(x1, y1);
        switch (y1 % 8 + 1) {
        case 1:
            wprintf(L"%lc", food1);
            break;
        case 2:
            wprintf(L"%lc", food2);
            break;
        case 3:
            wprintf(L"%lc", food3);
            break;
        case 4:
            wprintf(L"%lc", food4);
            break;
        case 5:
            wprintf(L"%lc", food5);
            break;
        case 6:
            wprintf(L"%lc", food6);
            break;
        case 7:
            wprintf(L"%lc", food7);
            break;
        case 8:
            wprintf(L"%lc", food8);
            break;
        }
    }
}


// 进行游戏开始前的一系列初始化和准备工作
void game_start(snake* ps) {
    system("mode con cols=100 lines=30");
    system("color 3f");
    system("title 贪吃蛇游戏");
    HANDLE houtput = NULL;
    houtput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO CursorInfo;
    GetConsoleCursorInfo(houtput, &CursorInfo);
    CursorInfo.bVisible = false;
    SetConsoleCursorInfo(houtput, &CursorInfo);
    welcomegame();
    print_map();
    initsnake(ps);
    set_food(ps);
    // 初始化障碍物位置数组内存空间
    ps->pObstacles = (ObstaclePosition*)malloc(5 * sizeof(ObstaclePosition));
    if (ps->pObstacles == NULL) {
        perror("malloc:obstaclePositions");
        return;
    }
    // 生成初始的障碍物
    set_obstacles(ps);
     // 初始化上次更新障碍物位置的时间为当前时间
    ps->last_obstacle_update_time = clock();
}

// 用于打印游戏的帮助信息
void printhelpinfo() {
    setpos(64, 13);
    wprintf(L"蛇不能穿墙");
    setpos(64, 14);
    wprintf(L"使用箭头键盘来控制蛇移动的方向");
    setpos(64, 15);
    wprintf(L"按F3加速，按F4减速（速度有4个档次）");
    setpos(64, 16);
    wprintf(L"按ESC可以快速退出游戏，空格暂停");
    setpos(90, 19);
    wprintf(L"@wrl");
}

// 实现游戏暂停功能
void snake_pause() {
    while (1) {
        Sleep(200);
        if (judgkeypush(VK_SPACE)) {
            break;
        }
    }
}

// 判断蛇头下一个位置是否是食物所在位置
int nextisfood(snakenode* p, snake* ps) {
    if (ps->pfood->x == p->x && ps->pfood->y == p->y) {
        return 1;
    }
    else {
        return 0;
    }
}

// 处理蛇吃到食物的情况
void eatfood(snakenode* p, snake* ps) {
    ps->pfood->next = ps->psnake;
    ps->psnake = ps->pfood;
    free(p);
    p = NULL;
    snakenode* cur = ps->psnake;
    while (cur) {
        setpos(cur->x, cur->y);
        wprintf(L"%lc", body);
        cur = cur->next;
    }
    ps->all_score += ps->foodscore;
    set_food(ps);
}

// 处理蛇没有吃到食物的情况
void noeatfood(snakenode* p, snake* ps) {
    p->next = ps->psnake;
    ps->psnake = p;
    snakenode* cur = ps->psnake;
    while (cur->next->next!= NULL) {
        setpos(cur->x, cur->y);
        wprintf(L"%lc", body);
        cur = cur->next;
    }
    setpos(cur->x, cur->y);
    wprintf(L"%lc", body);
    setpos(cur->next->x, cur->next->y);
    printf("  ");
    free(cur->next);
    cur->next = NULL;
}

// 检查蛇是否撞到墙
void killbywall(snake* ps) {
    if (ps->psnake->x == 0 || ps->psnake->x == 56 || ps->psnake->y == 0 || ps->psnake->y == 26) {
        ps->status = kill_by_wall;
    }
}


// 检查蛇是否撞到自己的身体
void killbyself(snake* ps) {
    snakenode* cur = ps->psnake->next;
    while (cur) {
        if (cur->x == ps->psnake->x && cur->y == ps->psnake->y) {
            ps->status = kill_by_self;
            break;
        }
        cur = cur->next;
    }
}

// 实现蛇的移动逻辑
void snakemove(snake* ps) {
    snakenode* smove = (snakenode*)malloc(sizeof(snakenode));
    if (smove == NULL) {
        perror("snakemove:malloc");
        return;
    }
    else {
        switch (ps->dir) {
        case up:
            smove->x = ps->psnake->x;
            smove->y = ps->psnake->y - 1;
            break;
        case down:
            smove->x = ps->psnake->x;
            smove->y = ps->psnake->y + 1;
            break;
        case right:
            smove->x = ps->psnake->x + 2;
            smove->y = ps->psnake->y;
            break;
        case left:
            smove->x = ps->psnake->x - 2;
            smove->y = ps->psnake->y;
            break;
        }
        // 判断是否碰到障碍物
        if (check_hit_obstacle(smove, ps)) {
            ps->status = kill_by_obstacle;
            return;
        }
        if (nextisfood(smove, ps)) {
            eatfood(smove, ps);

        }
        else {
            noeatfood(smove, ps);
        }
        killbywall(ps);
        killbyself(ps);
        Sleep(ps->sleep_time);
    }
}

// 检查蛇头是否碰到障碍物
int check_hit_obstacle(snakenode* p, snake* ps) {
    for (int i = 0; i < num_obstacles; i++) {
        if (p->x == ps->pObstacles[i].x && p->y == ps->pObstacles[i].y) {
            return 1;
        }
    }
    return 0;
}
// 播放音乐
void playMusic(const char* filePath) {
    char command[256];
    sprintf(command, "open kldykxq.mp3", filePath);
    mciSendString(command, NULL, 0, NULL);
    mciSendString("play kldykxq.mp3", NULL, 0, NULL);
    flag = 1;
}



// 游戏运行的主循环
void game_run(snake* ps) {
    printhelpinfo();
    do {
        setpos(64, 10);
        printf("总分数：%d", ps->all_score);
        updateHighScore(ps->all_score);
        currentScore = ps->all_score;
        setpos(64,9);
        printf("历史最高分：%d",getHighScore());
        setpos(64, 11);
        printf("每个食物分数：%3d", ps->foodscore);
        if (judgkeypush(VK_UP) && ps->dir!= down && ps->psnake->y > 1) {
            ps->dir = up;
        }
        else if (judgkeypush(VK_DOWN) && ps->dir!= up && ps->psnake->y < 25) {
            ps->dir = down;
        }
        else if (judgkeypush(VK_LEFT) && ps->dir!= right && ps->psnake->x > 2) {
            ps->dir = left;
        }
        else if (judgkeypush(VK_RIGHT) && ps->dir!= left && ps->psnake->x < 54) {
            ps->dir = right;
        }
        else if (judgkeypush(VK_SPACE)) {
            snake_pause();
        }
        else if (judgkeypush(VK_ESCAPE)) {
            ps->status = end_normal;
            //exit(0);
        }
        else if (judgkeypush(VK_F3)) {
            if (ps->sleep_time > 50) {
                ps->sleep_time -= 50;
                ps->foodscore += 5;
            }
        }
        else if (judgkeypush(VK_F4)) {
            if (ps->foodscore > 2) {
                ps->sleep_time += 50;
                ps->foodscore -= 5;
            }
        }
        else if (judgkeypush(VK_NUMPAD1)){
            while(1){
                    if(flag == 1){
                    mciSendString("pause kldykxq.mp3", NULL, 0, NULL);
                    flag = 0;
                    break;
                }

                else if(flag == 0){
                    mciSendString("open kldykxq.mp3",NULL,0,NULL);
                    mciSendString("play kldykxq.mp3",NULL,0,NULL);
                    flag = 1;
                    break;
                }
            }

        }
        snakemove(ps);

        // 获取当前时间
        clock_t current_time = clock();
        // 检查是否达到更新障碍物位置的时间间隔
        if (current_time - ps->last_obstacle_update_time >= OBSTACLE_UPDATE_INTERVAL) {
            update_obstacles(ps);
            // 更新上次更新障碍物位置的时间为当前时间
            ps->last_obstacle_update_time = current_time;
        }

    } while (ps->status == ok);
}

// 游戏结束时的处理函数
void endgame(snake* ps) {
    setpos(24, 12);
    switch (ps->status) {
    case kill_by_wall:
        wprintf(L"你撞到墙死亡了！！！");
        break;
    case kill_by_obstacle:
        wprintf(L"你撞到障碍物了！！！");
        break;
    case kill_by_self:
        wprintf(L"你撞到自己了！！！");
        break;
    case end_normal:
        wprintf(L"%ls", L"你主动退出游戏了！！！");
        break;
    }
    snakenode* cur = ps->psnake;
    snakenode* del = NULL;
    while (cur) {
        del = cur;
        cur = cur->next;
        free(del);
    }
    // 释放障碍物位置数组内存
    free(ps->pObstacles);
}

// 获取最高分函数
int getHighScore() {
    FILE *fp;
    int highScore = 0;
    fp = fopen("贪吃蛇历史最高分记录.txt", "r");
    if (fp!= NULL) {
        fscanf(fp, "%d", &highScore);
        fclose(fp);
    }
    return highScore;
}

// 更新最高分函数
void updateHighScore(int newScore) {
    int highScore = getHighScore();
    if (newScore > highScore) {
        FILE *fp;
        fp = fopen("贪吃蛇历史最高分记录.txt", "w");
        if (fp!= NULL) {
            fprintf(fp, "%d", newScore);
            fclose(fp);
        } else {
            perror("无法打开文件写入最高分");
            exit(EXIT_FAILURE);
        }
    }
}

void test() {
    snake bs = { 0 };
    game_start(&bs);
    game_run(&bs);
    endgame(&bs);
}
