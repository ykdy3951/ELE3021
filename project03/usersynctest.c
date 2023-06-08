#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define FILESIZE        (200*512)
#define BUFFERSIZE      512
#define BUF_PER_FILE    ((FILESIZE) / (BUFFERSIZE))

int
main(int argc, char *argv[])
{
    char cmd = argv[1][0], data[BUFFERSIZE];
    int fd, i, cnt, ret_sync, old_log_num, now_log_num;

    for(i = 0; i < sizeof(data); ++i) {
        data[i]=i%26+97;
    }
    old_log_num = -1;

    printf(1, "Sync Test\n");
    switch (cmd)
    {
    case '1':
        /* code */
        printf(1, "[Test 1] Start buffered i/o test\n");
        fd = open("lostdatafile", O_CREATE | O_RDWR);
        for(i = 0; i < BUF_PER_FILE; i++) {
            if (i % 100 == 0) {
                printf(1, "[File %d] %d bytes written\n", i * BUFFERSIZE);
            }
            if ((cnt = write(fd, data, sizeof(data))) != sizeof(data)) {
                printf(1, "[Error Test 1] Write returned %d\n", cnt);
                exit();
            }
            if ((now_log_num = read_log()) < 0) {
                printf(1, "[Error Test 1] read log : %d\n", now_log_num);
                exit();
            }
            printf(1, "[Log Info] %d to %d\n", old_log_num, now_log_num);
            old_log_num = now_log_num;
        }
        printf(1, "%d bytes written\n", BUF_PER_FILE * BUFFERSIZE);
        printf(1, "[Test 1] End buffered i/o test\n");
        close(fd);
        break;
    case '2':
        printf(1, "[Test 2] Start sync i/o test\n");
        fd = open("syncdatafile", O_CREATE | O_RDWR);
        for(i = 0; i < BUF_PER_FILE; i++) {
            if (i % 100 == 0) {
                printf(1, "[File %d] %d bytes written\n", i * BUFFERSIZE);
            }
            if ((cnt = write(fd, data, sizeof(data))) != sizeof(data)) {
                printf(1, "[Write Error] Write returned %d\n", cnt);
                exit();
            }
            if ((old_log_num = read_log()) < 0) {
                printf(1, "[Log Error] read log : %d\n", old_log_num);
                exit();
            }
            if ((ret_sync = sync()) == -1) {
                printf(1, "[Sync Error] Sync failed\n");
                exit();
            }
            if ((now_log_num = read_log()) < 0) {
                printf(1, "[Log Error] read log : %d\n", now_log_num);
                exit();
            }
            printf(1, "[Log Info] %d to %d\n", old_log_num, now_log_num);
            printf(1, "Flushed %d of blocks\n", ret_sync);
        }
        printf(1, "%d bytes written\n", BUF_PER_FILE * BUFFERSIZE);
        printf(1, "[Test 2] End sync i/o test\n");
        close(fd);
        break;
    default:
        printf(1, "WRONG CMD");
        break;
    }
    exit();
}