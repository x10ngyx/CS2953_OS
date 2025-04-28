#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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
  acquire(&tickslock);
  ticks0 = ticks;
#ifdef LAB_TRAPS
  backtrace();
#endif
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


#ifdef LAB_PGTBL
uint64
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va;
  int n;
  uint64 buf;
  argaddr(0, &va);
  argint(1, &n);
  argaddr(2, &buf);

  struct proc *p = myproc();
  pagetable_t pagetable = p->pagetable;
  uint64 res = 0;

  for(int i = 0; i < n; i++){
    pte_t* pte = walk(pagetable, va + PGSIZE * i, 0);
    if(*pte & PTE_A){ // check and clear
      res |= (1L << i);
      *pte &= (~PTE_A);
    }
  }

  copyout(pagetable, buf, (char*)&res, sizeof(uint64));
  return 0;
}
#endif

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

uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc() -> mask = mask;
  return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  argaddr(0, &addr);
  struct sysinfo info;
  info.freemem = get_freemem();
  info.nproc = get_nproc();
  struct proc* p = myproc();
  if(copyout(p->pagetable, addr, (char*)(&info), sizeof(info)) < 0)
      return -1;
  return 0;
}

uint64
sys_sigalarm(void)
{
  struct proc *p = myproc();
  int n;
  uint64 handler;
  argint(0, &n);
  argaddr(1, &handler);
  p->alarm_handler = (void(*)()) handler;
  p->ticks_for_alarm = n;
  p->ticks_used = 0;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  if (p->in_alarm) {
    p->in_alarm = 0;
    *p->trapframe = *p->alarmframe;
    p->ticks_used = 0;
  }
  return p->alarmframe->a0;
}