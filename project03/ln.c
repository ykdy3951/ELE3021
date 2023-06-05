#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    printf(2, "Usage: ln [-h | -s] old new\n");
    exit();
  }
  if(!strcmp(argv[1], "-h"))
    if(link(argv[2], argv[3]) < 0)
      printf(2, "hard link %s %s: failed\n", argv[2], argv[3]);
  if(!strcmp(argv[1], "-s"))
    if(symlink(argv[2], argv[3]) < 0)
      printf(2, "symbolic link %s %s: failed\n", argv[2], argv[3]);
  exit();
}
