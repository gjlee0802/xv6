// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct {
	struct spinlock lock;
	int use_lock;
	int numfreepages;
	uint ref[PHYSTOP >> PGSHIFT];
} pmem;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
	initlock(&pmem.lock, "pmemlock");
	pmem.use_lock = 0;
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{   
	pmem.use_lock = 1;
        acquire(&pmem.lock);
	memset(&pmem.ref, 0, sizeof(uint) * (PHYSTOP >> PGSHIFT));	
	pmem.numfreepages = 0;
	release(&pmem.lock);
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
// reference counter APIs 
int freemem()
{
	int numfreepages; 
	acquire(&pmem.lock);
	numfreepages = pmem.numfreepages;
	release(&pmem.lock);
	return numfreepages;
}

uint
get_ref(uint pa)
{       uint ref ;
          acquire(&pmem.lock);
          ref= pmem.ref[pa>>PGSHIFT]; 
          release(&pmem.lock);
	return ref;
}

void
inc_ref(uint pa)
{       
        acquire(&pmem.lock);
       pmem.ref[pa>>PGSHIFT]++;
        release(&pmem.lock); 
	
}

void
dec_ref(uint pa)
{        acquire(&pmem.lock);
       { pmem.ref[pa>>PGSHIFT]--;}
          release(&pmem.lock);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{  
  struct run *r;
  uint count,pa ;
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.

 pa = V2P(v)>>PGSHIFT ;
  if(kmem.use_lock)
    acquire(&kmem.lock);
  if(pmem.use_lock)
       acquire(&pmem.lock); 
   count = pmem.ref[pa];
   if(count >0)
  { --pmem.ref[pa];
       count --;}
       
if(count==0){
  r = (struct run*)v;
  memset(v,1,PGSIZE);
  r->next = kmem.freelist;
  kmem.freelist = r;
  pmem.numfreepages ++;
 }
  if(kmem.use_lock)
    release(&kmem.lock);
         


	if(pmem.use_lock)
		release(&pmem.lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;
  uint pa;
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
  pa=V2P((char*)r)>>PGSHIFT;
		if(pmem.use_lock)
			acquire(&pmem.lock);                   
                 pmem.ref[pa]++;
		pmem.numfreepages--;
		if(pmem.use_lock)
			release(&pmem.lock);

	}
  if(kmem.use_lock)
    release(&kmem.lock);

  return (char*)r;
}

