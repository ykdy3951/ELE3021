#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
thread_t nexttid = 1; // nexttid like nextpid in proc.c
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

int 
setmemorylimit(int pid, int limit)
{
  int ret = -1;
  struct proc *p;

  if (limit < 0)
  {
    return ret;
  }

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == pid && p->sz <= limit)
    {
      p->memlim = limit;
      ret = 0;
      break;
    }
  release(&ptable.lock);
  return ret;
}

void
list(void)
{
  struct proc *p;
  acquire(&ptable.lock);
  cprintf("[Process Information]\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state != RUNNABLE && p->state != RUNNING) continue;
    cprintf("%s %d %d %d %d\n", p->name, p->pid, p->ssize, p->sz, p->memlim);
  }
  release(&ptable.lock);
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
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

// process의 main thread의 주소를 return한다.
struct thread*
mainthread(struct proc* p) {
  return p->ttable;
}

// process의 작동중인 thread의 주소를 return한다.
struct thread*
mythread(struct proc* p) {
  return &p->ttable[p->rectidx];
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
  struct thread *t;
  char *sp;
  
  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  for(t = p->ttable; t < &p->ttable[NPROC]; t++)
  {
    t->kstack = 0;
    t->state = UNUSED;
    t->tf = 0;
    t->context = 0;
    t->chan = 0;
    t->tid = 0;
    t->retval = 0;
  }

  t = mainthread(p);

  p->state = EMBRYO;
  p->pid = nextpid++;
  p->memlim = 0;
  p->ssize = 0;
  memset(p->_ustack, 0, sizeof(uint) * NPROC);

  t->tid = nexttid++;
  t->state = EMBRYO;

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0){
    t->state = UNUSED;
    p->state = UNUSED;
    release(&ptable.lock);
    return 0;
  }
  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;
  
  release(&ptable.lock);

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  struct thread *t;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  t = mainthread(p);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(t->tf, 0, sizeof(*t->tf));
  t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  t->tf->es = t->tf->ds;
  t->tf->ss = t->tf->ds;
  t->tf->eflags = FL_IF;
  t->tf->esp = PGSIZE;
  t->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  t->state = RUNNABLE;

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
  struct thread *nt;
  struct thread *curthread = mythread(curproc);

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  nt = mainthread(np);
  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(nt->kstack);
    nt->kstack = 0;
    nt->state = UNUSED;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *nt->tf = *curthread->tf;
  
  // copy memory limit variable
  np->memlim = curproc->memlim;
  
  // Clear %eax so that fork returns 0 in the child.
  nt->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  // copy ustack
  for(int i = 1, j = 0; i < NPROC && j < NPROC;) {
    if (j == curproc->rectidx) {
      j++;
      continue;
    }
    np->_ustack[i] = curproc->_ustack[j];
    i++;
    j++;
  }
  np->_ustack[0] = curproc->_ustack[curproc->rectidx];

  np->state = RUNNABLE;
  nt->state = RUNNABLE;

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
  struct thread *t;
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

  for(t = curproc->ttable; t < &curproc->ttable[NPROC]; t++){
    if (t->state != UNUSED) {
      t->state = ZOMBIE;
    }
  }

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
  struct thread *t;
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        for(t = p->ttable; t < &p->ttable[NPROC]; t++){
          if (t->state == UNUSED) continue;
          p->_ustack[t-p->ttable] = 0;
          kfree(t->kstack);
          t->kstack = 0;
          t->state = UNUSED;
          t->tid = 0;
        }
        // Found one.
        pid = p->pid;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->rectidx = 0;
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
  struct proc *p;
  struct thread *t = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      t = mythread(p);
      for(int i = 0; i < NPROC; i++, t++){
        if (t == &p->ttable[NPROC]) 
          t = p->ttable;

        if (t->state != RUNNABLE) continue;
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        p->rectidx = t - p->ttable;
        switchuvm(p);

        t->state = RUNNING;

        swtch(&(c->scheduler), t->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
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
  if(mythread(p)->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&mythread(p)->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  mythread(myproc())->state = RUNNABLE;
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
  struct thread *t;

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
  t = mythread(p);
  t->chan = chan;
  t->state = SLEEPING;
  sched();

  // Tidy up.
  t->chan = 0;

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
  struct thread *t;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == RUNNABLE || p->state == SLEEPING)
      for(t = p->ttable; t < &p->ttable[NPROC]; t++)
        if(t->state == SLEEPING && t->chan == chan)
          t->state = RUNNABLE;
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
      getcallerpcs((uint*)mythread(p)->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Thread
int 
thread_create(thread_t * thread, void *(*start_routine)(void *), void *arg)
{
  struct thread *t;
  struct proc *curproc = myproc();
  struct thread *curthread = mythread(curproc);
  uint sz;
  char *sp;

  acquire(&ptable.lock);

  for(t = curproc->ttable; t < &curproc->ttable[NPROC]; t++)
    if(t->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return -1;

found:
  t->tid = nexttid++;
  t->state = EMBRYO;

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0)
    goto bad;
  
  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *curthread->tf;
  t->tf = (struct trapframe*)sp;
  *t->tf = *curthread->tf;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;

  // from exec
  if (curproc->_ustack[t - curproc->ttable] == 0) {
    sz = PGROUNDUP(curproc->sz);
    if ((sz = allocuvm(curproc->pgdir, sz, sz + 2 * PGSIZE)) == 0)
      goto bad;
    clearpteu(curproc->pgdir, (char*)(sz - 2*PGSIZE));
    curproc->_ustack[t - curproc->ttable] = sz;
    curproc->sz = sz;
  }
  curproc->ssize++;

  sp = (char *)curproc->_ustack[t - curproc->ttable];
  sp -= 4;
  *(uint*)sp = (uint) arg;
  sp -= 4;
  *(uint*)sp = (uint) 0xffffffff;

  // Commit to the user image.
  t->tf->eip = (uint)start_routine;  // main
  t->tf->esp = (uint)sp;
  t->state = RUNNABLE;

  *thread = t->tid;
  release(&ptable.lock);
  return 0;

bad:
  if (t->kstack) {
    kfree(t->kstack);
  }
  t->kstack = 0;
  t->tid = 0;
  t->state = UNUSED;
  release(&ptable.lock);
  return -1;
}

void
thread_exit(void *retval){
  struct proc *p = myproc();
  struct thread *t = mythread(p);

  acquire(&ptable.lock);

  t->state = ZOMBIE;
  t->retval = retval;
  wakeup1((void *)t->tid);
  
  sched();
  panic("zombie exit");
}

int
thread_join(thread_t thread, void **retval) {
  struct thread *t;
  int havekids;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(t = curproc->ttable; t < &curproc->ttable[NPROC]; t++){
      if(t->tid != thread)
        continue;
      havekids = 1;
      if(t->state == ZOMBIE){
        if (retval != 0)
          *retval = t->retval;
        kfree(t->kstack);
        t->kstack = 0;
        t->state = UNUSED;
        t->tid = 0;
        release(&ptable.lock);
        return 0;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep((void *)thread, &ptable.lock);  //DOC: wait-sleep
  }
}