#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
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
  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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

uint64 sys_sigalarm(void){
  struct proc *p = myproc();
  int n;
  uint64 handler;
  if(argint(0, &n) < 0)
    return -1;
  if(argaddr(1, &handler) < 0) 
    return -1;
  // 如果已经调用了一个 handler, 而且没有返回的话
  // 我们禁止这个新的调用(我们的设计不支持递归调用)
  if(p->is_alarm) {
    return 0;
  } 
  p->alarm_handler = (void(*)()) handler;
  p->ticks_for_alarm = n;
  p->ticks_used = 0;
  return 0;
}

uint64 sys_sigreturn(void) {
    struct proc *p = myproc();
    // 恢复寄存器状态(4 个 kernel 的不需要)
    // 只有保存了寄存器才需要恢复现场
    if(p->is_alarm) {
      memmove(
        ((uint64 *)p->trapframe) + 5,
        ((uint64 *)&(p->trapframe2)) + 5,
        sizeof(struct trapframe) - 5*sizeof(uint64)
      );
      p->trapframe->epc = p->trapframe2.epc;
    }
    p->is_alarm = 0;
    // p->ticks_used = 0;
    return 0;
}
