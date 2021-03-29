#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void recurve(int fd) {
    int status;
    int first = 1;
    int hasForked = 0;
    int firstNumber, num;
    int p1[2];

    while(read(fd, &num, sizeof(int)) != 0) {
        // 输出第一个数
        if(first == 1) {
            firstNumber = num;
            first = 0;
            printf("prime %d\n", num);
        }
        // 逐个检查之后的数, 若不能被记录的数整除, 传至下一进程
        else {
            if(num % firstNumber != 0) {
                if(hasForked == 0) {
                    Pipe(p1);
                    if(Fork() != 0) {
                        // father
                        close(p1[0]);
                    } else {
                        // child
                        close(fd);
                        close(p1[1]);
                        recurve(p1[0]);
                        exit(0); // 退出, 因为之后的代码都是父进程的
                    }
                    hasForked = 1;
                }
                // 写入管道
                write(p1[1], &num, sizeof(int));
            }
        }
    }
    if(hasForked == 1) {
        close(p1[1]);
        wait(&status);
    }
    close(fd);
}

// 核心问题就是每次只 write/read 一个 int
int main(int argc, char *argv[]) {
    // 忽略错误处理
    // 第一个进程输出 2-32 到管道里
    int status;
    int i;
    int p1[2];

    Pipe(p1);
    if (Fork() != 0) {
        // father
        close(p1[0]);
        for(i = 2; i < 35; ++i) {
            write(p1[1], &i, sizeof(int));
        }
        close(p1[1]);
        // 等待直到子进程全被回收
        while(wait(&status) != -1) {}
    } else {
        // child
        close(p1[1]);
        recurve(p1[0]);
    }
    exit(0);
}