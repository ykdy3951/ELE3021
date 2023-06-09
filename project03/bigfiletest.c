#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

const int MiB = 1024 * 1024;
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
filetest(char *path, int size)
{
  int i, fd, n;

  printf(stdout, "big files %d MiB test\n", size / MiB);

  fd = open(path, O_CREATE|O_RDWR);
  if(fd < 0){
    printf(stdout, "error: creat big failed!\n");
    exit();
  }

  for(i = 0; i < size; i++){
    ((int*)buf)[0] = i;
    if(write(fd, buf, 512) != 512){
      printf(stdout, "error: write big file failed\n", i);
      exit();
    }
  }

  close(fd);

  fd = open(path, O_RDONLY);
  if(fd < 0){
    printf(stdout, "error: open big failed!\n");
    exit();
  }

  n = 0;
  for(;;){
    i = read(fd, buf, 512);
    if(i == 0){
      if(n == size - 1){
        printf(stdout, "read only %d blocks from big", n);
        exit();
      }
      break;
    } else if(i != 512){
      printf(stdout, "read failed %d\n", i);
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
    printf(stdout, "sync failed\n");
    exit();
  }
  close(fd);
  if(unlink(path) < 0){
    printf(stdout, "unlink big failed\n");
    exit();
  }
  printf(stdout, "big files size: %d bytes ok\n", size);
}

int main(int argc, char *argv[])
{
    int n = 1, i;
    for(i = 0; i < 8; i++, n *= 2) {
        printf(stdout, "[Test %d] %dMiB file test\n", i, n);
        filetest(name[i], n * MiB);
        printf(stdout, "[Test %d] %dMiB file ok\n", i, n);
    }
    exit();
}