#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if(argc != 2) {
        // 输出到标准错误流
        fprintf(2, "Invalid args! Usage: sleep <number>!\n");
        exit(1);
    }
    int sleepTime = atoi(argv[1]);
    sleep(sleepTime);
    exit(0);
}
