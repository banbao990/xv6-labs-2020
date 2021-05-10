// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char *ref_cnt;   // 用于记录 reference count 的一个数组
  uint64 ref_start; // 开始分配的物理内存的起始地址
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  // 数组元素为 char(最多 64 进程)
  // sizeof(char) = 1, 不需要乘
  // 计算大小(偏大)
  uint64 size = ((uint64) pa_end - (uint64) pa_start) >> PGSHIFT;
  // 初始化为全 1
  // memset 逐字节设置
  memset(pa_start, 1, size);
  // 开始分配物理内存的位置
  p = (char *)pa_start + size;
  p = (char *)PGROUNDUP((uint64)p);
  acquire(&kmem.lock);
  // 数组起始位置设置为 pa_start
  kmem.ref_cnt = (char *)pa_start;
  kmem.ref_start = (uint64)p;
  release(&kmem.lock);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run*)pa;
  char num = ref_increment((uint64)pa, (char)-1);
  // 引用计数不为 0
  if(num > 0) {
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    kmem.ref_cnt[get_ref_cnt_index((uint64)r)] = 1;
  }
  return (void*)r;
}

/* 带锁 */
char ref_increment(uint64 pa, char inc) {
  acquire(&kmem.lock);
  char ans = (kmem.ref_cnt[get_ref_cnt_index(pa)] += inc);
  release(&kmem.lock);
  return ans;
}

/* 不带锁 */
uint64 get_ref_cnt_index(uint64 pa) {
  uint64 ans = (pa - kmem.ref_start) >> PGSHIFT;
  return ans;
}