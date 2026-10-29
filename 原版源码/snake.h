#include<stdio.h>
#include<stdlib.h>
#include<windows.h>
#include<stdbool.h>
#include<time.h>
#include<locale.h>

// 定义更新障碍物位置的时间间隔（单位：毫秒）
#define OBSTACLE_UPDATE_INTERVAL 5000

// 判断虚拟键是否被按下
#define judgkeypush(vk) ( (GetAsyncKeyState(vk)&1)?1:0)

#define num_obstacles 3    // 障碍物的个数

// 定义蛇初始位置相关的宏
#define pos_x 24
#define pos_y 5

// 定义游戏中食物和蛇身的字符表示
#define wall L'*'
#define food1 L'☆'
#define food2 L'★'
#define food3 L'◆'
#define food4 L'○'
#define food5 L'◇'
#define food6 L'▲'
#define food7 L'□'
#define food8 L'▼'
#define body L'■'

// 音乐播放的标志
int flag;

int currentScore; // 记录当局得分


// 枚举类型：蛇的移动方向
enum dirction {
    up = 1,  // 向上
    down,    // 向下
    left,    // 向左
    right    // 向右
};

// 枚举类型：游戏状态
enum game_Status {
    ok,          // 游戏正常进行
    kill_by_wall,    // 蛇撞到墙导致游戏结束
    kill_by_self,    // 蛇撞到自己导致游戏结束
    kill_by_obstacle, // 蛇撞到障碍物导致游戏结束
    end_normal       // 玩家正常退出游戏
};


// 障碍物储存结构，添加记录旧位置的成员变量
typedef struct {
    int x;
    int y;
    int old_x;  // 记录上一次的横坐标位置，用于清除原来位置的显示
    int old_y;  // 记录上一次的纵坐标位置，用于清除原来位置的显示
} ObstaclePosition;
ObstaclePosition obstaclePositions[];  // 游戏中同时存在的障碍物数量


// 食物储存结构
typedef struct {
    int x;
    int y;
} FoodPosition;
FoodPosition foodPositions[2];  // 用于存储两个食物的位置

// 蛇身节点结构体
typedef struct snakenode {
    int x;      // 节点的横坐标
    int y;      // 节点的纵坐标
    struct snakenode* next;    // 指向下一个节点的指针
} snakenode;

// 蛇结构体，包含蛇身、食物、游戏状态等信息
typedef struct snake {
    snakenode* psnake;   // 指向蛇身链表头节点的指针
    snakenode* pfood;    // 指向食物节点的指针
    // 指向障碍物数组的指针（方便管理多个障碍物）
    ObstaclePosition* pObstacles;
    enum game_Status status;    // 游戏当前状态
    enum dirction dir;       // 蛇当前移动方向
    int foodscore;       // 吃到每个食物获得的分数
    int all_score;       // 游戏总分数
    int sleep_time;      // 蛇每次移动的时间间隔，用于控制移动速度
    clock_t last_obstacle_update_time;  // 障碍物更新时间
} snake;


//
void color(int c);
// 游戏开始，进行初始化操作
void game_start(snake* ps);
// 设置控制台光标位置
void setpos(short x, short y);
// 显示游戏欢迎界面
void welcomegame();
// 绘制游戏地图（围墙）
void print_map();
// 初始化蛇
void initsnake(snake* ps);
// 在游戏区域内随机生成食物
void  set_food(snake* ps);
// 游戏运行主函数，处理游戏过程中的操作
void game_run(snake* ps);
// 实现游戏暂停功能
void snake_pause();
// 打印游戏帮助信息
void printhelpinfo();
// 判断蛇头下一个位置是否是食物
int nextisfood(snakenode* p, snake* ps);
// 处理蛇吃到食物情况
void eatfood(snakenode* p, snake* ps);
// 处理蛇没吃到食物情况
void noeatfood(snakenode* p, snake* ps);
// 检查蛇是否撞到墙
void killbywall(snake* ps);
// 检查蛇是否撞到自己
void killbyself(snake* ps);
// 游戏结束处理
void endgame(snake* ps);
// 获取游戏历史最高分
int getHighScore();
// 更新游戏历史最高分
void updateHighScore(int newScore);
// 获取当局得分
int getCurrentScore(snake* ps);
// 播放音乐
void playMusic(const char* filePath);
// 设置障碍物
void set_obstacles(snake* ps);
// 更新障碍物位置
void update_obstacles(snake* ps);
// 检查蛇头是否碰到障碍物的函数
int check_hit_obstacle(snakenode* p, snake* ps);
void test();
