#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define MAX_SIZE 512
char fileDir[MAX_SIZE]; // 保存当前文件夹的位置
char *target;

// p 指向 fileDir 的结尾, fileName 是当前文件名
void find(char *p, char *fileName) {
    int fd;
    struct dirent de;
    struct stat st;
    char* temp;
    char buf[MAX_SIZE];
    
    // 拼出完整路径
    strcpy(buf, fileDir);
    temp = buf+strlen(fileDir);
    *temp = '/';
    strcpy(++temp, fileName);
    temp += strlen(fileName);
    *temp = '\0';

    // 检查能否打开(存在, 权限)
    // 0 只读
    if((fd = open(buf, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", buf);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", buf);
        close(fd);
        return;
    }
    
    switch(st.type){
    case T_FILE:
        if(strcmp(target, fileName) == 0) {
            printf("%s%s\n", fileDir, target);
        }
        break;
    case T_DIR:
        // 入口的 "." 需要打开, 忽略之后的 ".",".."
        if(p != fileDir) {
            if(strcmp(".", fileName) == 0 || strcmp("..", fileName) == 0){
                break;
            }
        }
        // 尝试打开文件夹
        temp = p;
        if(strlen(fileName) + strlen(fileDir) + 3 > MAX_SIZE) {
            printf("find: path too long\n");
            break;
        }
        strcpy(p, fileName);
        p += strlen(fileName);
        *p = '/';
        *(++p) = '\0';
        // 对文件循环
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0) {
                continue;
            }
            find(p, de.name);
        }
        p = temp;
        *p = '\0';
        break;
    default:
        break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(2, "Invalid Args! Usage: find <fileName> <fileName>\n");
        exit(1);
    }
    char *p = fileDir;
    target = argv[2];
    memset(fileDir, 0, sizeof(fileDir));
    find(p, argv[1]);
    exit(0);
}