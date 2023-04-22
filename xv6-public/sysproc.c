#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// yield
int
sys_yield(void)
{
  yield(); // yield를 호출한다.
  return 0;
}

// getLevel
int
sys_getLevel(void)
{
  return getLevel(); // GetLevel function을 호출
}

// setPriority
int
sys_setPriority(void)
{
  int pid, priority;
  if(argint(0, &pid) < 0 || argint(1, &priority) < 0) // pid, priority가 정상적으로 들어오지 않았을 경우 -1을 반환한다.
    return -1;
  
  setPriority(pid, priority); // 받은 parameter을 기반으로 함수를 호출
  return 0;
}

// schedulerLock
int
sys_schedulerLock(void)
{
  int password;
  if(argint(0, &password) < 0) // parameter가 제대로 들어오지 않은경우 -1을 반환.
    return -1;
  
  schedulerLock(password); // 받은 parameter을 기반으로 함수를 호출
  return 0;
}

// schedulerUnlock
int
sys_schedulerUnlock(void)
{
  int password;
  if(argint(0, &password) < 0) // parameter가 제대로 들어오지 않은경우 -1을 반환.
    return -1;
  
  schedulerUnlock(password); // 받은 parameter을 기반으로 함수를 호출
  return 0;
}