// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define BUCKET_NUM 13
#define BUF_NUM NBUF

struct {
  struct spinlock lock_global;
  struct buf buf[BUF_NUM]; // 一共就 30 个 buffer
  // 修改为哈希桶(开链法)
  struct buf buckets[BUCKET_NUM];
  struct spinlock lock[BUCKET_NUM];
} bcache;

static inline int hash(uint val) {
  return (int)(val % BUCKET_NUM);
}

void
binit(void)
{
  // struct buf *b;

  // 初始化锁
  for(int i = 0;i < BUCKET_NUM; ++i) {
    initlock(&bcache.lock[i], "bcache");
  }
  initlock(&bcache.lock_global, "bcache_global");

  // 初始化初始引用
  for(int i = 0; i < BUCKET_NUM; ++i) {
    bcache.buckets[i].next = &bcache.buckets[i];
  }
  
  // 初始化 buffer
  for(int i = 0; i < BUF_NUM; ++i) {
    int id = hash(i);
    struct buf* now = &bcache.buf[i];
    now->blockno = id;
    now->refcnt = 0;
    // now->timestamp = ticks;
    initsleeplock(&now->lock, "buffer");
    // 插入
    now->next = bcache.buckets[id].next;
    bcache.buckets[id].next = now;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  // printf("f");
  struct buf *b;

  int id = hash(blockno);
  acquire(&bcache.lock[id]);

  // 1. 查找这个块是否被缓存, 如果被缓存直接返回即可
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }


  // 1.5 先在自己的桶里寻找
  uint timestamp_min = 0x8fffffff;
  struct buf* res = 0;
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next) {
    if(b->refcnt == 0 && b->timestamp < timestamp_min) {
      timestamp_min = b->timestamp;
      res = b;
    }
  }
  if(res) {
    res->dev = dev;
    res->blockno = blockno;
    res->valid = 0;
    res->refcnt = 1;
    release(&bcache.lock[id]);
    acquiresleep(&res->lock);
    return res;
  }


  // 2. 没有被缓存, 需要进行驱逐
  // 注意这里需要加全局锁
  // 不然可能出现两个进程同时找到同一个空块的情况
  acquire(&bcache.lock_global);

  // (1) 找到最早的时间戳
  // 可以直接对 buf 循环
  // uint 
  timestamp_min = 0x8fffffff;
  // struct buf* 
  res = 0;
  for(int i = 0; i < BUF_NUM; ++i) {
    b = &bcache.buf[i];
    if(b->refcnt == 0 && b->timestamp < timestamp_min) {
      timestamp_min = b->timestamp;
      res = b;
    }
  }

  // 如果没找到直接 panic
  if(!res) {
    release(&bcache.lock[id]);
    release(&bcache.lock_global);
    panic("bget: no buffers");
  }

  int id_new = hash(res->blockno);
  // (2) 判断 id 是否相同, 相同的话可以直接返回
  // if(id == id_new) {
  //   res->dev = dev;
  //   res->blockno = blockno;
  //   res->valid = 0;
  //   res->refcnt = 1;
  //   release(&bcache.lock[id]);
  //   release(&bcache.lock_global);
  //   acquiresleep(&res->lock);
  //   return res;
  // }

  // (3) id 如果不相同的话, 需要移动块到目标哈希桶中
  // 从原桶中删除
  acquire(&bcache.lock[id_new]);
  // 仅有一个元素
  // if(bcache.buckets[id_new] == res && res->next == 0) {
  //   bcache.buckets[id_new] = 0;
  // } else {
    for(b = &bcache.buckets[id_new]; b->next != &bcache.buckets[id_new]; b = b->next) {
      if(b->next == res) {
        b->next = res->next;
        break;
      }
    }
  // }
  // 插入新桶
  res->next = bcache.buckets[id].next;
  bcache.buckets[id].next = res;
  res->dev = dev;
  res->blockno = blockno;
  res->valid = 0;
  res->refcnt = 1;
  release(&bcache.lock[id_new]); // 具体释放位置应该在哪里?
  release(&bcache.lock[id]); 
  release(&bcache.lock_global);
  acquiresleep(&res->lock);
  return res;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks; 
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);

  // acquire(&bcache.lock_global);
  // b->refcnt++;
  // release(&bcache.lock_global);
}

void
bunpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);

  // acquire(&bcache.lock_global);
  // b->refcnt++;
  // release(&bcache.lock_global);
}