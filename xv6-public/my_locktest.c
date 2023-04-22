#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int pid = 0;

    char cmd = argv[1][0];
    switch (cmd) {
    case '1':
        printf(1, "[Test 1] scheduler Lock/Unlock Test with system call\n");
        pid = fork();
        if (pid == 0) {
            schedulerLock(2019044711);
        }
        for(int i = 0; i < 10; i++) {
            if(pid == 0) {
                printf(1, "[child] %d\n", i);
            }
            else{
                printf(1, "[parent] %d\n", i);
            }
        }
        if(pid == 0){
            schedulerUnlock(2019044711);
            exit();
        }
        else{
            while(wait() != -1);
        }
        printf(1, "[Test 1] finished\n");
        break;
    case '2':
        printf(1, "[Test 2] scheduler Lock/Unlock Test with interrupt\n");
        pid = fork();
        if (pid == 0) {
            __asm__("int $129");
        }
        for(int i = 0; i < 10; i++) {
            if(pid == 0) {
                printf(1, "[child] %d\n", i);
            }
            else{
                printf(1, "[parent] %d\n", i);
            }
        }
        if(pid == 0){
            __asm__("int $130");
            exit();
        }
        else{
            while(wait() != -1);
        }
        printf(1, "[Test 2] finished\n");
        break;
    default:
        printf(1, "WRONG CMD\n");
        break;
    }
    printf(1, "done\n");
    exit();
};