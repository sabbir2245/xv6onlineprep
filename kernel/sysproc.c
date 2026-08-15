#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
//====new code =====
#include "syscall.h"

//====new code =====
struct syscall_stat {
  char syscall_name[16];
  int count;
  int accum_time;
};

//====new code =====
uint64
sys_trace(void)
{
  int syscall_number;

  argint(0, &syscall_number);
  if(syscall_number <= 0 || syscall_number > SYS_history)
    return -1;
  myproc()->trace_sys_num = syscall_number;
  return 0;
}

//====new code =====
uint64
sys_history(void)
{
  int syscall_number;
  uint64 user_stat_address;
  struct syscall_stat stat;

  argint(0, &syscall_number);
  argaddr(1, &user_stat_address);
  if(syscall_number <= 0 || syscall_number > SYS_history)
    return -1;
  syscall_stat_get(syscall_number, stat.syscall_name, &stat.count,
                   &stat.accum_time);
  if(copyout(myproc()->pagetable, user_stat_address, (char *)&stat,
             sizeof(stat)) < 0)
    return -1;
  return 0;
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
