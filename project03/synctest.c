#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define FILESIZE        (200*512)
#define BUFFERSIZE      512
#define BUF_PER_FILE    ((FILESIZE) / (BUFFERSIZE))

int stdout = 1;
char data[BUFFERSIZE];

void 
synctest(char *path, int option) 
{
    int fd, i, cnt, ret_sync, old_log_num, now_log_num;

    fd = open(path, O_CREATE | O_RDWR);
    old_log_num = read_log();

    for(i = 0; i < BUF_PER_FILE; i++) {
        if (i % 100 == 0) {
            printf(stdout, "[File %d] %d bytes written\n", i * BUFFERSIZE);
        }
        if ((cnt = write(fd, data, sizeof(data))) != sizeof(data)) {
            printf(stdout, "[Write Error] Write returned %d\n", cnt);
            exit();
        }
        if ((old_log_num = read_log()) < 0) {
            printf(stdout, "[Log Error] read log : %d\n", old_log_num);
            exit();
        }
        if (option && (ret_sync = sync()) == -1) {
            printf(stdout, "[Sync Error] Sync failed\n");
            exit();
        }
        if ((now_log_num = read_log()) < 0) {
            printf(stdout, "[Log Error] read log : %d\n", now_log_num);
            exit();
        }
        printf(stdout, "[Log Info] %d to %d\n", old_log_num, now_log_num);
        printf(stdout, "Flushed %d of blocks\n", ret_sync);
    }
    printf(stdout, "%d bytes written\n", BUF_PER_FILE * BUFFERSIZE);
    close(fd);
}

int
main(int argc, char *argv[])
{
    char cmd = argv[1][0];
    int fd, i, cnt, ret_sync, old_log_num, now_log_num;

    for(i = 0; i < sizeof(data); ++i) {
        data[i]=i%26+97;
    }
    old_log_num = -1;

    printf(stdout, "Sync Test\n");
    switch (cmd)
    {
    case '1':
        /* code */
        printf(stdout, "[Test 1] Start buffered i/o test\n");
        synctest("lostdatafile", 0);      
        printf(stdout, "[Test 1] End buffered i/o test\n");
        break;
    case '2':
        printf(stdout, "[Test 2] Start sync i/o test\n");
        synctest("syncdatafile", 1);
        printf(stdout, "[Test 2] End sync i/o test\n");
        break;
    default:
        printf(stdout, "WRONG CMD\n");
        break;
    }
    exit();
}