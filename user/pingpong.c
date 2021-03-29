#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ARRAY_SIZE 10

int main(int argc, char *argv[]) {
    // 变量定义
    // parent to child, child to parent
    // 使用一个管道会引发锁的问题
    int fdP2C[2];
    int fdC2P[2];
    int id;
    char msg[ARRAY_SIZE];
    int readNumber;
    
    // start
    readNumber = 0;
    memset(msg, 0, sizeof(msg));
    
    // 创建一个管道
    // 返回两个文件描述符 fd[0] 读, fd[1] 写
    // fd[1] 的输出是 fd[0] 的输入
    Pipe(fdP2C);
    Pipe(fdC2P);

    // 创建父子进程
    id = Fork();
    if(id != 0) {
        // father
        close(fdP2C[0]);
        close(fdC2P[1]);
        // write
        write(fdP2C[1], "ping", 4);
        // read
        readNumber = read(fdC2P[0], msg, ARRAY_SIZE);
        if(readNumber > 0) {
            fprintf(1, "%d: received %s\n", getpid(), msg);
        }
        close(fdP2C[1]);
        close(fdC2P[0]);
    } else {
        // child
        close(fdP2C[1]);
        close(fdC2P[0]);
        // read
        readNumber = read(fdP2C[0], msg, ARRAY_SIZE);
        if(readNumber > 0) {
            fprintf(1, "%d: received %s\n", getpid(), msg);
        }
        // write
        write(fdC2P[1], "pong", 4);
        close(fdP2C[0]);
        close(fdC2P[1]);
        // sleep(10);
    }
    exit(0);
}