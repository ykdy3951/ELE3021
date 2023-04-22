#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define NUM_YIELD 20000
#define NUM_SLEEP 500

#define NUM_THREAD 4
#define MAX_LEVEL 3 // 3
#define PASSWORD 2019044711
int parent;

int fork_children()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++) {
    p = fork();
    if (p == 0)
    {
      sleep(10);
      return getpid();
    }
    sleep(50);
  }
  return parent;
}


int fork_children2()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      int pid = getpid();
      printf(1, "before sleep pid: %d, lv: %d\n", pid, getLevel());
      sleep(250);
      printf(1, "after sleep pid: %d, lv: %d\n", pid, getLevel());
      return getpid();
    }
    else
    {
      setPriority(p, i);
    }
    sleep(50);
  }
  return parent;
}

int max_level;

int fork_children3()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0) // child sleep
    {
      sleep(10);
      max_level = i;
      return getpid();
    }
    sleep(50);
  }
  return parent;
}
void exit_children()
{
  if (getpid() != parent) {
    exit();
  }
  while (wait() != -1);
}

int main(int argc, char* argv[])
{
  int child = 5;
  parent = getpid();

  printf(1, "MLFQ test start\n");
  int i = 0, pid = 0;
  int count[MAX_LEVEL] = { 0, 0, 0 };
  char cmd = argv[1][0];
  switch (cmd) {
  case '1':
    printf(1, "[Test 1] default\n");
    pid = fork_children();

    if (pid != parent)
    {
      for (i = 0; i < NUM_LOOP; i++)
      {
        int x = getLevel();
        if (x < 0 || x > 2)
        {
          printf(1, "Wrong level: %d\n", x);
          exit();
        }
        count[x]++;
      }
      printf(1, "Process %d\n", pid);
      for (i = 0; i < MAX_LEVEL; i++)
        printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);
    }
    exit_children();
    printf(1, "[Test 1] finished\n");
    break;
  case '2':
    printf(1, "[Test 2] priorities\n");
    pid = fork_children2();

    if (pid != parent)
    {
      for (i = 0; i < NUM_LOOP; i++)
      {
        int x = getLevel();
        if (x < 0 || x > 2)
        {
          printf(1, "Wrong level: %d\n", x);
          exit();
        }
        count[x]++;
      }
      printf(1, "Process %d\n", pid);
      for (i = 0; i < MAX_LEVEL; i++)
        printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);

    }
    exit_children();
    printf(1, "[Test 2] finished\n");
    break;
  case '3':
    printf(1, "[Test 3] yield\n");
    pid = fork_children2();

    if (pid != parent)
    {
      for (i = 0; i < NUM_YIELD; i++)
      {
        int x = getLevel();
        if (x < 0 || x > 2)
        {
          printf(1, "Wrong level: %d\n", x);
          exit();
        }
        count[x]++;
        if (pid % 2 == 0) yield();
      }
      printf(1, "Process %d\n", pid);
      for (i = 0; i < MAX_LEVEL; i++)
        printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);

    }
    exit_children();
    printf(1, "[Test 3] finished\n");
    break;
  case '4':
    printf(1, "[Test 4] sleep\n");
    pid = fork_children2();
    if (pid != parent)
    {
      int my_pid = getpid();
      printf(1, "pid: %d fork: %d\n", my_pid, getLevel());

      for (i = 0; i < NUM_SLEEP; i++)
      {
        int x = getLevel();
        if (x < 0 || x > 2)
        {
          printf(1, "Wrong level: %d\n", x);
          exit();
        }
        count[x]++;
        sleep(1);
      }
      sleep(my_pid * 3);
      printf(1, "Process %d\n", pid);
      for (i = 0; i < MAX_LEVEL; i++)
        printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);

    }
    exit_children();
    printf(1, "[Test 4] finished\n");
    break;
  case '5':
    printf(1, "[Test 5] setPriority\n");
    child = fork();

    if (child == 0)
    {
      // int r;
      int grandson;
      sleep(10);
      grandson = fork();
      if (grandson == 0)
      {
        setPriority(getpid() - 2, 0);
        setPriority(getpid() - 3, 0);
      }
      else
      {
        setPriority(grandson, 0);
        setPriority(getpid() + 1, 0);
      }
      sleep(20);
      wait();
    }
    else
    {
      int child2 = fork();
      sleep(20);
      if (child2 == 0)
        sleep(10);
      else
      {
        setPriority(child, -1);
        setPriority(child, 11);
        setPriority(child, 10);
        setPriority(child + 1, 10);
        setPriority(child + 2, 10);
        setPriority(parent, 5);
      }
    }

    exit_children();
    printf(1, "done\n");
    printf(1, "[Test 5] finished\n");
    break;
  case '6':
    printf(1, "[Test 6] sleep\n");
    schedulerLock(PASSWORD);
    int pid = fork();
    if (pid != 0) {
      exit();
    } else {
      sleep(100);
    }
    printf(1, "[Test 6] sleep end\n");
    break;
  case '7':
    printf(1, "[Test 7] schedulerLock & Unlock Test\n");
    child = fork();

    if (child == 0) // child
    {
      int grandson;
      printf(1, "Before Lock Process : %d \n", getpid());
      schedulerLock(PASSWORD);
      printf(1, "After Lock Process : %d \n", getpid());
      grandson = fork();
      if (grandson == 0) // gradnsond
      {
        printf(1, "GRANDSON : %d\n", getpid());
        printf(1, "Before try Unlock Process : %d \n", getpid());
        schedulerUnlock(PASSWORD);
        printf(1, "After try Unlock Process : %d \n", getpid());
        schedulerLock(PASSWORD);

      }
      else // child
      {
        printf(1, " CHILD : %d\n", getpid());
        printf(1, "Before try Unlock Process : %d \n", getpid());
        schedulerUnlock(PASSWORD);
        printf(1, "After try Unlock Process : %d \n", getpid());
        printf(1, "Process : %d still work \n", getpid());
      }
      sleep(20);
      wait();
    }
    else // parent
    {
      printf(1, "Else\n");

      int child2 = fork();
      sleep(20);
      if (child2 == 0) {//child2
        printf(1, " CHILD2 : %d\n", getpid());

        printf(1, "Before try Lock Process : %d \n", getpid());

        schedulerLock(PASSWORD);
        printf(1, "After try Lock Process : %d \n", getpid());
        sleep(2);
        for (int i = 0;i < NUM_SLEEP * 1000000;i++) {
          if (i % 100000000 == 0) {
            printf(1, "spend time for priority boosting while running\n");
          }
        }
        printf(1, "Before try Lock Again Process : %d \n", getpid());

        schedulerLock(PASSWORD);
        printf(1, "After try Lock Again Process : %d \n", getpid());
      }
      else
      {
        printf(1, " PARENT : %d\n", getpid());
        schedulerLock(PASSWORD);
        printf(1, "test before for sleep priority Boost\n");
        sleep(100);
        printf(1, "test after for sleep priority Boost\n");

      }
    }
    printf(1, "Process : %d\n", getpid());

    exit_children();
    printf(1, "Process : %d\n", getpid());
    printf(1, "[Test 7] finished\n");

    break;
  case '8':
    printf(1, "[Test 8] schedulerLock / Unlock Normal Case\n");
    child = fork();
    if (child == 0) { //child
      printf(1, "Process %d schedluer Lock\n", getpid());
      schedulerLock(PASSWORD);
      printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
      schedulerUnlock(PASSWORD);
      printf(1, "Process %d schedluer Unlock\n", getpid());
    }
    else { // parent
      printf(1, "Process %d schedluer Lock\n", getpid());
      schedulerLock(PASSWORD);
      printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
      schedulerUnlock(PASSWORD);
      printf(1, "Process %d schedluer Unlock\n", getpid());
    }
    exit_children();
    printf(1, "[Test 8] finished\n");

    break;
  case '9':

    printf(1, "[Test 9] schedulerLock / Unlock Wrong Case1 : PASSWORD ERROR\n");
    child = fork();
    if (child == 0) { //child
      printf(1, "Process %d schedluer Lock\n", getpid());
      printf(1, "This procedure intends to scheduler Lock Error for PASSWORD\n");

      schedulerLock(PASSWORD + 1);
      // 아래부분 실행 안되어야함
      printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
      schedulerUnlock(PASSWORD + 1);
      printf(1, "Process %d schedluer Unlock\n", getpid());
    }
    else { // parent
      int child2 = fork();
      if (child2 == 0) {
        printf(1, "Process %d schedluer Lock\n", getpid());
        schedulerLock(PASSWORD);
        printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
        printf(1, "This procedure intends to scheduler Unlock Error for PASSWORD\n");
        schedulerUnlock(PASSWORD + 1);
        //아래 부분 실행 안되어야함.
        printf(1, "Process %d schedluer Unlock\n", getpid());
      }
    }
    exit_children();
    printf(1, "[Test 9] finished\n");

    break;
  case 'A':

    printf(1, "[Test 10] schedulerLock / Unlock Wrong Case2 : duplication ERROR\n");
    child = fork();
    if (child == 0) { //child
      printf(1, "Process %d schedluer Lock\n", getpid());

      schedulerLock(PASSWORD);
      printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
      printf(1, "This procedure intends to scheduler Lock Error for duplication\n");

      // duplicate Lock
      schedulerLock(PASSWORD);
      printf(1, "Process %d schedluer Unlock\n", getpid());
    }
    else { // parent
      int child2 = fork();
      if (child2 == 0) {
        printf(1, "Process %d schedluer Lock\n", getpid());
        schedulerLock(PASSWORD);
        printf(1, "Process %d in Q level %d\n", getpid(), getLevel());
        printf(1, "This procedure intends to scheduler Unlock Error for duplication\n");
        schedulerUnlock(PASSWORD);
        printf(1, "Process %d schedluer Unlock\n", getpid());
        schedulerUnlock(PASSWORD);
        printf(1, "Process %d schedluer Unlock Again\n", getpid());

      }
    }
    exit_children();
    printf(1, "[Test 10] finished\n");
    break;
  case 'B':
    printf(1, "Lock\n");
    schedulerLock(2019044711);
    pid = fork();
    sleep(1000);
    printf(1, "sleep end %d\n", pid);
    if (pid != 0) {
      printf(1, "waiting...\n");
      while(wait() != -1);
      printf(1, "[Test 11] parent end");
    }
    else
      printf(1, "[Test 11] child end\n");
    break;
  case 'C':
    printf(1, "Lock\n");
    schedulerLock(2019044711);
    pid = fork();
    sleep(10);
    printf(1, "sleep end %d\n", pid);
    if (pid != 0) {
      printf(1, "waiting...\n");
      while(wait() != -1);
      printf(1, "[Test 12] parent end");
    }
    else
      printf(1, "[Test 12] child end\n");
    break;
  default: 
    printf(1, "WRONG CMD\n");
    break;
  }


  printf(1, "done\n");
  exit();
}



// // #include "types.h"
// // #include "stat.h"
// // #include "user.h"

// // int
// // main(int argc, char *argv[])
// // {
// // 	__asm__("int $128");
// // 	return 0;
// // 	/*
// // 	if (argc <= 1)
// // 		exit();

// // 	if (strcmp(argv[1], "\"user\"") != 0)
// // 		exit();

// // 	char *buf = "Hello xv6!";
// // 	int ret_val;
// // 	ret_val = myfunction(buf);
// // 	printf(1, "Return value : 0x%x\n", ret_val);
// // 	exit();
// // 	*/
// // };

// #include "types.h"
// #include "stat.h"
// #include "user.h"

// int
// main(int argc, char *argv[])
// {
//     schedulerLock(2019044711);
//     int pid = fork();

//     if (pid == 0) 
//     {
//         printf(1, "[child]\n");
//         exit();
//     }
//     while(wait() != -1);

//     printf(1, "[parent]\n");
//     schedulerUnlock(2019044711);
//     exit();
//     // int pid = fork();

//     // for(int i = 0; i < 10; i++) {
//     //     if(pid == 0) {
//     //         printf(1, "[child] %d\n", i);
//     //         sleep(10);
//     //     }
//     //     else{
//     //         printf(1, "[parent] %d\n", i);
//     //         sleep(10);
//     //     }
//     // }
//     // if (pid == 0)
//     //     exit();
//     // while (wait() != -1);
//     // printf(1, "[Test 1] finished\n");
//     // printf(1, "done\n");
//     // exit();
// };