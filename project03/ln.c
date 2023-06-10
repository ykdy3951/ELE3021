#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  // Check the number of argument
  if(argc != 4){
    printf(2, "Usage: ln [-h | -s] old new\n");
    exit();
  }
  // If argv[1] is "-h" then call (hard) link syscall
  if(!strcmp(argv[1], "-h")) {
    if(link(argv[2], argv[3]) < 0)
      printf(2, "hard link %s %s: failed\n", argv[2], argv[3]);
  }
  // If argv[1] is "-s" then call symlink syscalls
  else if(!strcmp(argv[1], "-s")) {
    if(symlink(argv[2], argv[3]) < 0)
      printf(2, "symbolic link %s %s: failed\n", argv[2], argv[3]);
  }
  else
    printf(2, "Usage: ln [-h | -s] old new\n");
  exit();
}
