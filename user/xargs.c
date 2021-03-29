#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_BUFFER_SIZE 512
#define MAX_ARG_LENGTH 32

// xargs: 将前一个命令的输出作为后面的输入的附加参数, 按照 " "(空格)分开
// 例如: 当前文件目录下有文件 a.txt, b.txt
// ls | xargs rm
// 等价于
// rm a.txt b.txt

// echo -e : 字符转义

// 高级语法(未实现) 
// TODO
// -n num 后面加次数, 表示命令在执行的时候一次用的参数的个数, 默认是用所有的
// (1)
// echo -e "1\n2" | xargs -n 1 echo line
// line 1
// line 2
// (2)
// echo -e "1\n2" | xargs echo line
// line 1 2


// 单行读取, 每一行执行一次命令
// 将每一行按照空格分开, 作为参数传入 
int main(int argc, char **argv) {
    int i;
    int status;
    char pass[MAXARG][MAX_ARG_LENGTH];
    int n;
    char buffer[MAX_BUFFER_SIZE + 1]; // 多留一位
    char *start, *now, *last;
    int pos; // 当前应该写入的参数位置
    char *passHelp[MAXARG]; // 用于辅助传参
    
    pos = argc - 1;
    memset(pass, 0, sizeof(pass));
    memset(passHelp, 0, sizeof(passHelp));

    // 复制参数
    // xargs 这个参数不需要
    for(i = 1; i < argc; ++i) {
        strcpy(pass[i-1], argv[i]);
    }

    // 从标准输入流读取输出
    while((n = read(0, buffer, MAX_BUFFER_SIZE)) > 0) {
        buffer[n] = '\0'; //因为多保留了一位
        last = buffer + n;
        // 将读取到的字符按照空格/回车分开
        start = buffer;
        for(now = buffer; now < last; ++now) {
            if(*now == ' ') {
                *now = '\0';
                if(pos == MAXARG) {
                    unix_error("To Many Args!");
                }
                strcpy(pass[pos++], start);
                start = now + 1;
            } else if(*now == '\n') {
                *now = '\0';
                if(pos == MAXARG) {
                    unix_error("To Many Args!");
                }
                strcpy(pass[pos++], start);
                start = now + 1;
                // 每行执行一次
                for(i = 0;i < pos; ++i) {
                    passHelp[i] = pass[i];
                }
                passHelp[pos] = 0;
                if(Fork() == 0) {
                    exec(passHelp[0], passHelp);
                } else {
                    wait(&status);
                }
                pos = argc - 1;
            }
        }
        // 最后一个参数
        if(pos == MAXARG) {
            unix_error("To Many Args!");
        }
        strcpy(pass[pos++], start);
        // 要求下一次复制的时候, 保留最后一个参数
        // 处理一个字符串被分割为两段
        // 事实上无法区分是两个还是一个
    }
    // 因为我们不实现高级功能, 不需要使用 fork
    for(i = 0;i < pos; ++i) {
        passHelp[i] = pass[i];
    }
    passHelp[pos] = 0;
    if(Fork() == 0) {
        exec(passHelp[0], passHelp);
    } else {
        wait(&status);
    }

    exit(0);
}
//  echo 3 4 5 | xargs echo 1 2