#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

const int MiB = 2048;
int stdout = 1;
char buf[8192];

char name[8][15] = {
    "1MiBfile",
    "2MiBfile",
    "4MiBfile",
    "8MiBfile",
    "16MiBfile",
    "32MiBfile",
    "64MiBfile",
    "128MiBfile",
};

void
filetest(char *path, int size, int option)
{
  int i, fd, n;

  printf(stdout, "Start %d MiB file test\n", size / MiB);

  fd = open(path, O_CREATE|O_RDWR);
  if(fd < 0){
    printf(stdout, "[Error] creat failed!\n");
    exit();
  }

  printf(stdout, "[Write start]\n");
  for(i = 0; i < size; i++){
    ((int*)buf)[0] = i;
    if(i % MiB == 0) {
        printf(stdout, "Write total %d MiB in file\n", i / MiB);
    }
    if(write(fd, buf, 512) != 512){
      printf(stdout, "[Error] write failed\n", i);
      exit();
    }
  }

  close(fd);

  fd = open(path, O_RDONLY);
  if(fd < 0){
    printf(stdout, "[Error] open failed!\n");
    exit();
  }

  n = 0;

  printf(stdout, "[Read start]\n");
  for(;;){
    i = read(fd, buf, 512);
    if (n % MiB == 0){
        printf(stdout, "Read total %d MiB in file\n", n / MiB);
    }
    if(i == 0){
      if(n == size - 1){
        printf(stdout, "read only %d blocks from big", n);
        exit();
      }
      break;
    } else if(i != 512){
      printf(stdout, "[Error] read failed %d\n", i);
      exit();
    }
    if(((int*)buf)[0] != n){
      printf(stdout, "read content of block %d is %d\n",
             n, ((int*)buf)[0]);
      exit();
    }
    n++;
  }
  if (sync() == -1) {
    printf(stdout, "[Error] sync failed\n");
    exit();
  }
  close(fd);
  if(option && unlink(path) < 0){
    printf(stdout, "[Error] unlink failed\n");
    exit();
  }
  printf(stdout, "End %d MiB file test\n", size / MiB);
  printf(stdout, "file size: %d bytes ok\n", size * 512);
}

void help()
{
    printf(stdout, "-------------------- Test List --------------------\n");
    printf(stdout, "- 0. 1 MiB file test                               \n");
    printf(stdout, "- 1. 2 MiB file test                               \n");
    printf(stdout, "- 2. 4 MiB file test                               \n");
    printf(stdout, "- 3. 8 MiB file test                               \n");
    printf(stdout, "- 4. 16 MiB file test                              \n");
    printf(stdout, "- 5. 32 MiB file test                              \n");
    printf(stdout, "- 6. 64 MiB file test                              \n");
    printf(stdout, "- 7. 128 MiB file test                             \n");
    printf(stdout, "---------------------------------------------------\n\n");
}

int pow2[] = {1, 2, 4, 8, 16, 32, 64, 128};

int main(int argc, char *argv[])
{
    int n = 1, i;
    int cmd = 0;
    if (argc == 2) {
        cmd = argv[1][0] - '0';
    }

    help();

    for(i = 0; i < 4; i++, n *= 2) {
        printf(stdout, "[Test %d] %d MiB file test\n", i, n);
        filetest(name[i], n * MiB, cmd);
        printf(stdout, "[Test %d] %d MiB file ok\n\n", i, n);
    }

    printf(stdout, "All tests Passed\n");
    exit();
}