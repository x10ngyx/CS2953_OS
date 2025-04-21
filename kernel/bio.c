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

struct {
  struct spinlock lock_global; // for LRU
  struct buf buf[NBUF];
  struct buf *head[NBUCKET];
  struct spinlock lock[NBUCKET];
} bcache;

void
binit(void)
{
  initlock(&bcache.lock_global, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    bcache.head[i] = 0;
    initlock(&bcache.lock[i], "bcache");
  }

  struct buf* b;
  for(int i = 0; i < NBUF; ++i) {
    int id = i % NBUCKET;
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.head[id];
    bcache.head[id] = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  // Is the block already cached?
  for(b = bcache.head[id]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // First try to evict within the same bucket.
  for(b = bcache.head[id]; b; b = b->next){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  // If failed, try to evict from the other buckets.

  release(&bcache.lock[id]);


  for (int i = 0; i < NBUCKET; i++) 
    acquire(&bcache.lock[i]);
  acquire(&bcache.lock_global);
  // Now we have all the locks.

  for(int i = 0; i < NBUCKET; i++) {
    if (i == id) continue;
    for(b = bcache.head[i]; b; b = b->next){
      if(b->refcnt == 0) {
        // remove the buffer from the old bucket
        // add the buffer to the new bucket
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // remove from old bucket
        struct buf *prev = bcache.head[i];
        if (prev == b) {
          bcache.head[i] = b->next;
        } else {
          while (prev->next != b) {
            prev = prev->next;
          }
          prev->next = b->next;
        }
        // add to new bucket
        b->next = bcache.head[id];
        bcache.head[id] = b;
        release(&bcache.lock_global);
        for (int j = 0; j < NBUCKET; j++) {
          release(&bcache.lock[j]);
        }
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
  panic("bget: no buffers");
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
  int id = b->blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]); 
}

void
bpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


