#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
//==========new code =====================================
extern uint64 sys_trace(void);
extern uint64 sys_history(void);
//==========END new code =================================

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
//==========new code =====================================
[SYS_trace]   sys_trace,
[SYS_history] sys_history,
//==========END new code =================================
};

//==========new code =====================================
static char *syscall_names[] = {
[SYS_fork] "fork", [SYS_exit] "exit", [SYS_wait] "wait",
[SYS_pipe] "pipe", [SYS_read] "read", [SYS_kill] "kill",
[SYS_exec] "exec", [SYS_fstat] "fstat", [SYS_chdir] "chdir",
[SYS_dup] "dup", [SYS_getpid] "getpid", [SYS_sbrk] "sbrk",
[SYS_sleep] "sleep", [SYS_uptime] "uptime", [SYS_open] "open",
[SYS_write] "write", [SYS_mknod] "mknod", [SYS_unlink] "unlink",
[SYS_link] "link", [SYS_mkdir] "mkdir", [SYS_close] "close",
[SYS_trace] "trace", [SYS_history] "history",
};
//==========END new code =================================

//==========new code =====================================
struct syscall_stat_entry {
  struct spinlock lock;
  int count;
  int accum_time;
};
//==========END new code =================================

//==========new code =====================================
static struct syscall_stat_entry syscall_stats[NELEM(syscalls)];
//==========END new code =================================

//==========new code =====================================
void
syscall_stats_init(void)
{
  int i;
  for(i = 1; i < NELEM(syscalls); i++)
    initlock(&syscall_stats[i].lock, "syscall_stat");
}
//==========END new code =================================

//==========new code =====================================
static uint
syscall_ticks(void)
{
  uint now;
  acquire(&tickslock);
  now = ticks;
  release(&tickslock);
  return now;
}
//==========END new code =================================

//==========new code =====================================
static void
syscall_stat_record(int number, uint elapsed)
{
  acquire(&syscall_stats[number].lock);
  syscall_stats[number].count++;
  syscall_stats[number].accum_time += elapsed;
  release(&syscall_stats[number].lock);
}
//==========END new code =================================

//==========new code =====================================
void
syscall_stat_get(int number, char *name, int *count, int *accum_time)
{
  acquire(&syscall_stats[number].lock);
  safestrcpy(name, syscall_names[number], 16);
  *count = syscall_stats[number].count;
  *accum_time = syscall_stats[number].accum_time;
  release(&syscall_stats[number].lock);
}
//==========END new code =================================

//==========new code =====================================
static void
trace_string(uint64 address)
{
  char string[MAXPATH];
  if(fetchstr(address, string, sizeof(string)) < 0)
    printf("%p", (void *)address);
  else
    printf("%s", string);
}
//==========END new code =================================

//==========new code =====================================
static void
trace_arguments(int number, uint64 args[])
{
  switch(number) {
  case SYS_fork: case SYS_getpid: case SYS_uptime:
    break;
  case SYS_exit: case SYS_kill: case SYS_dup: case SYS_sbrk:
  case SYS_sleep: case SYS_close: case SYS_trace:
    printf("%d", (int)args[0]); break;
  case SYS_wait: case SYS_pipe:
    printf("%p", (void *)args[0]); break;
  case SYS_read: case SYS_write:
    printf("%d, %p, %d", (int)args[0], (void *)args[1], (int)args[2]); break;
  case SYS_exec:
    trace_string(args[0]); printf(", %p", (void *)args[1]); break;
  case SYS_fstat:
    printf("%d, %p", (int)args[0], (void *)args[1]); break;
  case SYS_chdir: case SYS_unlink: case SYS_mkdir:
    trace_string(args[0]); break;
  case SYS_open:
    trace_string(args[0]); printf(", %d", (int)args[1]); break;
  case SYS_mknod:
    trace_string(args[0]); printf(", %d, %d", (int)args[1], (int)args[2]); break;
  case SYS_link:
    trace_string(args[0]); printf(", "); trace_string(args[1]); break;
  case SYS_history:
    printf("%d, %p", (int)args[0], (void *)args[1]); break;
  }
}
//==========END new code =================================

void
syscall(void)
{
  int num;
  int i;
  uint start_ticks;
  uint64 args[6];
  char exec_path[MAXPATH] = {0};
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    //==========new code =====================================
    for(i = 0; i < NELEM(args); i++)
      args[i] = argraw(i);
    // exec replaces the process page table, so preserve its pathname before
    // dispatching the system call for tracing below.
    if(num == SYS_exec) {
      if(fetchstr(args[0], exec_path, sizeof(exec_path)) < 0)
        safestrcpy(exec_path, "???", sizeof(exec_path));
    }
    start_ticks = syscall_ticks();
    // SYS_exit never returns, so record statistics and print the trace
    // line before dispatching the system call.
    if(num == SYS_exit) {
      syscall_stat_record(num, 0);
      if(p->trace_sys_num == num) {
        printf("pid: %d, syscall: %s, args: (%d), return: 0\n",
               p->pid, syscall_names[num], (int)args[0]);
      }
      p->trapframe->a0 = syscalls[num]();
      return;
    }
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
    //==========END new code =================================
    //==========new code =====================================
    syscall_stat_record(num, syscall_ticks() - start_ticks);
    if(p->trace_sys_num == num) {
      printf("pid: %d, syscall: %s, args: (", p->pid, syscall_names[num]);
      if(num == SYS_exec)
        printf("%s, %p", exec_path, (void *)args[1]);
      else
        trace_arguments(num, args);
      printf("), return: %d\n", (int)p->trapframe->a0);
    }
    //==========END new code =================================
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
