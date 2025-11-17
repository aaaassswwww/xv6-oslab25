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

#define NBUCKETS 13

// 修改bcache的结构 变成hash table
struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf hash[NBUCKETS];
} bcache;

uint hash(uint blockno){
  return blockno % NBUCKETS;
}

void binit(void)
{
  // TODO: modify here
  struct buf *b;

  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache");
    bcache.hash[i].prev = &bcache.hash[i];
    bcache.hash[i].next = &bcache.hash[i];

  }

  // uint i = 0;
  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hash[0].next;
    b->prev = &bcache.hash[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hash[0].next->prev = b;
    bcache.hash[0].next = b;
    // i = (i + 1) % NBUCKETS;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf* bget(uint dev, uint blockno) {
  // TODO: 修改bget() 和 brelse() 使得缓存区并发的查询和释放不容易发生锁争用
  struct buf *b;

  uint bucket = hash(blockno);

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.hash[bucket].next; b != &bcache.hash[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // b = 0;

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //先在当前桶中寻找空闲块
  for(b = bcache.hash[bucket].prev; b != &bcache.hash[bucket]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
      // acquiresleep(&b->lock);
      // return b;
    }
  }
  release(&bcache.lock[bucket]);

  //找不到则按序遍历其他桶
  // if(b == 0) {
    for(int other_bucket=0;other_bucket<NBUCKETS;other_bucket++){
      if(other_bucket == bucket) continue;

      // if(other_bucket > bucket) {
      //   acquire(&bcache.lock[other_bucket]);
      // } else {
      //   release(&bcache.lock[bucket]);
      //   acquire(&bcache.lock[other_bucket]);
      //   acquire(&bcache.lock[bucket]);
      //
      //   //重新检查桶中是否已缓存了需要的块
      //           
      //   for(b = bcache.hash[bucket].next; b != &bcache.hash[bucket]; b = b->next){
      //     if(b->dev == dev && b->blockno == blockno){
      //       b->refcnt++;
      //       release(&bcache.lock[bucket]);
      //       release(&bcache.lock[other_bucket]);
      //       acquiresleep(&b->lock);
      //       return b;
      //     }
      //   }
      // }

      acquire(&bcache.lock[other_bucket]);

      for(b = bcache.hash[other_bucket].prev; b != &bcache.hash[other_bucket]; b = b->prev){
        if(b->refcnt == 0) {
          //将块移动到对应桶
          b->prev->next = b->next;
          b->next->prev = b->prev;
          release(&bcache.lock[other_bucket]);
          acquire(&bcache.lock[bucket]);
          b->prev = &bcache.hash[bucket];
          b->next = bcache.hash[bucket].next;
          bcache.hash[bucket].next->prev = b;
          bcache.hash[bucket].next = b;
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          release(&bcache.lock[bucket]);
          acquiresleep(&b->lock);
          return b;

        }
      }

      release(&bcache.lock[other_bucket]);
      // if(other_bucket <= bucket) {
      //   // 重新检查当前桶
      //   for(b = bcache.hash[bucket].next; b; b = b->next){
      //     if(b->dev == dev && b->blockno == blockno){
      //       b->refcnt++;
      //       release(&bcache.lock[bucket]);
      //       acquiresleep(&b->lock);
      //       return b;
      //     }
      //   }
      // }
    }
    panic("bget: no buffers");
  // }

// found:
//     b->dev = dev;
//     b->blockno = blockno;
//     b->valid = 0;
//     b->refcnt = 1;
//     release(&bcache.lock[bucket]);
//     acquiresleep(&b->lock);
//     return b;

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

  uint index = hash(b->blockno);

  acquire(&bcache.lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hash[index].next;
    b->prev = &bcache.hash[index];
    bcache.hash[index].next->prev = b;
    bcache.hash[index].next = b;
  }
  
  release(&bcache.lock[index]);
}

void
bpin(struct buf *b) {
  uint index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt++;
  release(&bcache.lock[index]);
}

void
bunpin(struct buf *b) {
  uint index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt--;
  release(&bcache.lock[index]);
}


