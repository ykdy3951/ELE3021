#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define PASSWORD 2019044711

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// MLFQ LEVEL
#define MLFQ_LEVEL 3

// Queue type for process
struct proc_queue_t {
  struct proc* data[NPROC];
  
  // for traversal
  int front;
  int rear;
  int size;

  // for schedulerLock, Unlock
  int is_locked;
};

// MLFQ structure
// 3-Level feedback queue
struct {
  struct proc_queue_t queue[MLFQ_LEVEL];
  // is_locked for schedulerLock, Unlock
  int is_locked;
  // global_ticks is tick for MLFQ scheduler
  uint global_ticks;
} MLFQ;

// MLFQ initialization
void 
mlfq_initalization(void) {
  
  // For traversal
  struct proc_queue_t *temp;
  
  // Initialize queue 
  for (int l = 0; l < MLFQ_LEVEL; ++l){
    temp = &MLFQ.queue[l];
    
    memset(temp->data, 0, sizeof(struct proc *) * NPROC);
    
    temp->front = 1;
    temp->rear = 0;
    temp->size = 0;
    temp->is_locked = 0;
  }

  // For schedulerLock,Unlock
  MLFQ.is_locked = 0;

  // For MLFQ ticks
  MLFQ.global_ticks = 0;
}

// Enqueue function for MLFQ
// This function takes process struct pointer and level
int
mlfq_enqueue(struct proc* proc, int level){
  
  struct proc_queue_t *target = &MLFQ.queue[level];
  
  // maximum number of possible processes in the queue (of the level)
  if (target->size == NPROC)
    return -1;
  
  // if queue[level] has empty space
  int target_idx = (target->rear + 1) % NPROC;
  target->data[target_idx] = proc;
  target->rear = target_idx;
  ++target->size;
  return 0;
}

// Dequeue function for MLFQ
// level을 인자로 받아 level에 맞는 queue의 process를 꺼내서 리턴한다.
struct proc*
mlfq_dequeue(int level){
  
  // target queue를 가져온다.
  struct proc_queue_t *target = &MLFQ.queue[level];

  // target이 비어있을 경우
  if (target->size == 0)
    return 0;
  
  // 해당 queue에 1개 이상의 process가 존재하는 경우
  struct proc* ret = target->data[target->front];
  target->data[target->front] = 0;
  target->front = (target->front + 1) % NPROC;
  --target->size;

  return ret;
}

// MLFQ lock
// Turn on is_locked variables
void
mlfq_locking(struct proc* p){
  MLFQ.is_locked=1;
  MLFQ.queue[p->level].is_locked=1;
  p->is_locked=1;
}

// MLFQ unlock
// Turn off is_locked variables
void
mlfq_unlocking(struct proc* p){
  MLFQ.is_locked=0;
  MLFQ.queue[p->level].is_locked=0;
  p->is_locked=0;
}

// push function for MLFQ
// 해당하는 level의 queue의 맨 앞에 process를 넣는 함수
// locked process가 lock이 풀릴 때만 사용된다.
void mlfq_push(struct proc* proc, int level){
  struct proc_queue_t *target = &MLFQ.queue[level];

  int target_idx = (target->front-1+NPROC)%NPROC;
  target->data[target_idx] = proc;
  target->front = (target->front-1+NPROC)%NPROC;
  ++target->size;
}

// Priority boosting
void
mlfq_priority_boosting(void){
  struct proc_queue_t *target = &MLFQ.queue[0];
  struct proc *proc;
  MLFQ.global_ticks = 0;

  // L0 queue의 모든 것들을 모두 초기화한다.
  for(int i = 0; i < target->size; ++i){
    proc = mlfq_dequeue(0);
    proc->priority = 3;
    proc->time_quantum = 0;

    mlfq_enqueue(proc, 0);
  }

  // L1, L2의 모든 process를 L0로 옮긴다.
  for(int lev = 1; lev <= 2; ++lev){
    target = &MLFQ.queue[lev];

    for(int i = 0; i < target->size; ++i){
      proc = mlfq_dequeue(lev);
      proc->priority = 3;
      proc->time_quantum = 0;
      proc->level = 0;

      mlfq_enqueue(proc, 0);
    }
  }
}

// A function that removes zombie process from MLFQ and frees up the space
void 
mlfq_kill(struct proc *proc){
  // Using target process's level, Access the target queue.
  struct proc_queue_t *target = &MLFQ.queue[proc->level];

  // Find target process's index from MLFQ
  int idx;
  for(idx = 0; idx < NPROC; ++idx){
    if (target->data[idx] == proc){
      break;
    }
  }

  // parameter인 target process가 존재하지 않을 경우
  if (idx == NPROC) return;

  // remove zombie process in MLFQ
  // target 프로세스를 기준으로 왼쪽의 process를 한 칸씩 오른쪽으로 옮긴다.
  int nxt; 
  while(idx != target->front){
    nxt = idx - 1;
    if (nxt < 0)
      nxt += NPROC;
    
    target->data[idx]=target->data[nxt];
    idx=nxt;
  }

  // front를 한 칸 옮기고 원래 front였던 자리는 0으로 초기화 한다.
  target->data[target->front] = 0;
  target->front = (target->front + 1) % NPROC;
  --target->size;
}

// schedulerLock
// 비밀번호를 인자로 받아 비밀번호가 올바르면 해당 process에 lock을 건다.
void 
schedulerLock(int password){
  acquire(&ptable.lock);
  // MLFQ has already locked process
  if (MLFQ.is_locked == 1) {
    cprintf("Duplicate Lock\n");
    release(&ptable.lock);
    kill(myproc()->pid);
    return;
  }

  // password is not correct 
  if(password != PASSWORD){
    cprintf("%d %d %d\n", myproc()->pid, myproc()->time_quantum, myproc()->level);
    release(&ptable.lock);
    kill(myproc()->pid);
    return;
  }

  // password is correct!
  MLFQ.global_ticks = 0;
  mlfq_locking(myproc());
  release(&ptable.lock);
}

// schedulerUnlock
// 비밀번호를 인자로 받아 비밀번호가 올바르면 해당 process에 lock을 푼다.
void
schedulerUnlock(int password){

  acquire(&ptable.lock);

  // MLFQ has already locked process
  if(MLFQ.is_locked == 0) {
    cprintf("Duplicate Unlock\n");
    release(&ptable.lock);
    return;
  }

  // password is not correct 
  if(password != PASSWORD){
    cprintf("%d %d %d\n", myproc()->pid, myproc()->time_quantum, myproc()->level);
    mlfq_unlocking(myproc());
    release(&ptable.lock);
    kill(myproc()->pid);
    return;
  }

  // password is correct!
  mlfq_unlocking(myproc());
  mlfq_dequeue(myproc()->level);
  myproc()->level = 0;
  myproc()->time_quantum = 0;
  myproc()->priority = 3;
  mlfq_push(myproc(), 0);

  release(&ptable.lock);
}

// process의 level을 return하는 함수
int 
getLevel(void){
  int cur_level = myproc()->level;

  return cur_level;
}

// 특정 pid의 process의 priority를 변경하는 함수
// L0, L1, L2 어디에 있던지 사용가능.
void 
setPriority(int pid, int priority){
  struct proc *p;
  
  // 지정할 수 없는 priority면 setPriority를 끝낸다.
  if (priority < 0 || priority > 3) {
    cprintf("This priority %d is not allowed in MLFQ\n", priority);
    return;
  }
  acquire(&ptable.lock);

  // 해당 pid를 가진 process를 탐색후 변경
  // 없는 경우 그냥 종료한다.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->pid == pid){
      p->priority = priority;
      break;
    }  

  release(&ptable.lock);
}

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  
  // for mlfq
  // mlfq initalization part
  mlfq_initalization();
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // for MLFQ
  p->level = 0;
  p->is_locked = 0;
  p->priority = 3;
  p->time_quantum = 0;

  // mlfq에 자리가 없으면 종료한다.
  if(mlfq_enqueue(p, p->level) != 0){
    release(&ptable.lock);
    return 0;
  }

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){

        // ZOMBIE가 발견되면 MLFQ내에서 지워버린다.
	      mlfq_kill(p);
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// MLFQ에 locked process가 없는 경우 runnable한 process를 찾기 위한 함수
// return 값은 process의 주소이다.
struct proc*
find_runnable_proc(void)
{
  // 사용될 변수 초기화
  struct proc *r_proc = 0;
  int lev = 0, pid = -1, priority = 4;

  struct proc_queue_t *traversal;
  
  // while문을 돌면서 runnable한 process를 탐색한다.
  while(lev < MLFQ_LEVEL) {
    traversal = &MLFQ.queue[lev];
    
    // 해당 level을 가진 queue에 process가 없을 경우
    if(traversal->size == 0){
      ++lev;
      continue;
    }

    // circular queue이므로 L0, L1인 경우 한바퀴를 돌면서 runnable한 process를 탐색한다.
    if(lev < 2){
      for(int i = 0; i < traversal->size; ++i){
        r_proc = traversal->data[traversal->front];
        if(r_proc->state == RUNNABLE){
          return r_proc;
        }
        mlfq_dequeue(lev);
        mlfq_enqueue(r_proc, lev);
      }
    }

    // level 2 queue인경우
    // priority를 max_priority + 1로 잡고 순회하면서
    // FCFS, priority 기준으로 적절한 process를 탐색한다.
    else{
      struct proc * temp;
      pid = -1, priority = 4;
      for(int i = 0; i < traversal->size; ++i){
        int idx = (traversal->front+i)%NPROC;
        temp = traversal->data[idx];
        if(temp->state != RUNNABLE) continue;
        if(priority > temp->priority){
          priority = temp->priority;
          pid = temp->pid;
        }
      }

      // 발견하지 못했을 경우
      if (pid == -1) 
        break;

      // 발견한 process가 맨 앞에 오도록 circular queue를 회전시킨다.
      r_proc = traversal->data[traversal->front];
      while(pid != r_proc->pid){
        mlfq_dequeue(lev);
        mlfq_enqueue(r_proc, lev);
        r_proc = traversal->data[traversal->front];
      }
    }

    // runnable한 process를 발견했으면 return 한다.
    if (r_proc->state == RUNNABLE) {
      return r_proc;
    }

    // runnable한 process가 없을 경우
    ++lev;
  }

  return 0;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc_queue_t *traversal;
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    
    // MLFQ Scheduling
    // proc initialization
    p = 0;
    // MLFQ has locked process
    if(MLFQ.is_locked == 1){
      // Find queue's level that is locked
      int lev;
      for(lev = 0; lev < MLFQ_LEVEL; ++lev){
        traversal = &MLFQ.queue[lev];
        if(traversal->is_locked == 1) break;
      }
      
      // is_locked가 켜져있는 process를 탐색할 때까지 회전시킨다.
      for(int i = 0; i < traversal->size; i++){
        p = traversal->data[traversal->front];
        if(p->is_locked == 1)
          break;
        
        mlfq_dequeue(lev);
        mlfq_enqueue(p, lev);
      }

      // locked process가 zombie process인 경우
      // 해당 process에 lock을 해제한다.
      if (p->state == ZOMBIE){
        mlfq_unlocking(p);
        release(&ptable.lock);
        continue;
      }
    }
    // MLFQ has no locked process.
    else {
      p = find_runnable_proc();
    }

    if (p != 0) {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      int lev = p->level;

      // process가 정상적으로 끝났으면 time quantum을 상승시킨다.
      if (p->state == RUNNABLE){        
        p->time_quantum++;
        MLFQ.global_ticks++;
      }
      else if (p->is_locked == 1){
        MLFQ.global_ticks++;
      }
      // whether current process is locked or not and ticks is 100 then run priority_boosting.
      if(MLFQ.global_ticks >= 100){
        int flag = 0;

        // if current process is locked, then the flag value sets 1
        if(p->is_locked == 1) {
          flag = 1;
          // MLFQ Unlocking
          mlfq_unlocking(p);
        }

        // Remove the current process from MLFQ and Enqueue this process in L0 queue
        if (flag == 1){
          mlfq_dequeue(lev);
          
          p->level=0;
          p->priority=3;
          p->time_quantum=0;
          mlfq_push(p, 0);
        }
        // myproc이 time quantum을 다 썼으면 뒤로 보내고 boosting한다.
        else if (p->time_quantum >= 2 * lev + 4){
          if(lev < 2){
              ++p->level;
          }
          else{
            int priority = p->priority ? p->priority - 1 : 0;
            p->priority = priority;
          }
          mlfq_dequeue(lev);
          mlfq_enqueue(p, p->level);
        }
        // Run priority boosting function.

        mlfq_priority_boosting();
      }
      // MLFQ is not locked and current process runs out of time quantums
      // if level of current process is 0 or 1 then raises the level
      // else process's priority down
      else if (MLFQ.is_locked == 0){

        // time quantum을 다 사용하였는지 check
        if (p->time_quantum >= 2 * lev + 4){
          p->time_quantum = 0;

          // L0, L1인경우 level을 상승시킨다.
          if(lev < 2){
              ++p->level;
          }

          // L2인경우 priority를 낮춘다. (단, 최소 0)
          else{
            int priority = p->priority ? p->priority - 1 : 0;
            p->priority = priority;
          }
        }

        // For round robin
        // 현재 process를 뒤로 보낸다.
        mlfq_dequeue(lev);
        mlfq_enqueue(p, p->level);
      }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
