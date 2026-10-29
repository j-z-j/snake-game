#include"snake.h"

int main() {
    // 调用播放音乐的函数，尝试播放名为"kldykxq.mp3"的音乐文件，用于为游戏添加背景音乐等音频效果
    playMusic("kldykxq.mp3");
    system("color 3f");
    // 设置本地化环境，以便正确处理本地化相关的内容，这里设置为空字符串表示采用系统默认的本地化设置
    setlocale(LC_ALL, "");
    // 通过srand函数结合time函数，以当前时间作为随机数生成器的种子,用于游戏中随机生成食物位置等需要随机性的场景
    srand((unsigned int)time(NULL));

    char ch = 0;
   // snake *ps;
    do {
        // 调用test函数，这个函数是用来执行一次完整的贪吃蛇游戏流程，包括游戏开始、运行、结束等环节
        test();
        // 获取最高分
        int highScore = getHighScore();
        // 使用system函数调用命令行命令"color 3f",这里将背景颜色设置为淡蓝色，文本色为亮白色
        system("color 3f");
        Sleep(1000);
        system("cls");

        if(currentScore < highScore){
            setpos(20, 14);
            printf("您当局得分为%d分,与最高分相差%d分", currentScore, highScore - currentScore);
            setpos(20, 15);
            printf("请再接再励哦，你是最棒哒！！！");
            setpos(20,16);
            printf("是否需要再来一局呢？？？(y/n)");
        }
        else {
            setpos(20,12);
            printf("棒棒哒！！！");
            setpos(20,14);
            printf("恭喜您打破历史最高记录！！！");
            setpos(20,16);
            printf("是否需要再来一局呢？？？(y/n)");
        }

        ch = getchar();
        // 判断输入是否合法，如果不合法则提示重新输入
        while (ch!= 'Y' && ch!= 'y' && ch!= 'N' && ch!= 'n') {
            printf("回答的格式错误了哦，请重新输入正确格式(Y/y/N/n)！\n");
            while (getchar()!= '\n');  // 清空输入缓冲区
            ch = getchar();
        }
        if (ch == 'N' || ch == 'n') {
            system("cls");
            setpos(20, 15);
            printf("游戏结束！\n");
            break;
        }
        else if (ch == 'Y' || ch == 'y') {
            continue;
        }
        // 清空输入缓冲区中剩余的字符，确保下一次输入不受干扰
        while (getchar()!= '\n');

    } while (true);
    return 0;
}
