#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "slab.h"

struct {
	struct spinlock lock;
	struct slab slab[NSLAB];
} stable;

void slabinit(){
	acquire(&stable.lock);
	/* fill in the blank */
	int i, j;
	int ssize = 8;
	for(i=0; i<NSLAB; i++){
		stable.slab[i].size = ssize;
		stable.slab[i].num_pages = 1;
		stable.slab[i].num_free_objects = 4096/stable.slab[i].size;
		stable.slab[i].num_used_objects = 0;
		stable.slab[i].num_objects_per_page = 4096/stable.slab[i].size;
		stable.slab[i].bitmap = kalloc(); 
		if(stable.slab[i].size == 1024){
			for(j=0; j<4096; j++)
				stable.slab[i].bitmap[j]= -16;
		}
		else if(stable.slab[i].size == 2048){
			for(j=0; j<4096; j++)
				stable.slab[i].bitmap[j]= -4;
		}
		else{
			for(j=0; j<4096; j++)
				stable.slab[i].bitmap[j]= 0;
		}
		stable.slab[i].page[0] = kalloc();
		ssize *= 2;
	}
	release(&stable.lock);
}

char *kmalloc(int size){
	/* fill in the blank */
	int i = 0;
	int ssize = 8;
	
	while(size > ssize){
		ssize *= 2;
		i++;
	}

	int byte_per_page = stable.slab[i].num_objects_per_page/8;
	if(byte_per_page == 0) byte_per_page++;

	if(stable.slab[i].num_free_objects == 0){ // more page

		acquire(&stable.lock);
		// create new page
		int pageidx = stable.slab[i].num_pages;
		stable.slab[i].num_pages++;
		cprintf("new page%d, %x ", pageidx, stable.slab[i].page[pageidx]);
		stable.slab[i].page[pageidx] = kalloc();
		cprintf("new page%d, %x\n", pageidx, stable.slab[i].page[pageidx]);
		stable.slab[i].num_free_objects = stable.slab[i].num_objects_per_page;
		
		// new object in page's first slot.
		stable.slab[i].num_free_objects--;
		stable.slab[i].num_used_objects++;
		stable.slab[i].bitmap[byte_per_page*pageidx] += 1;
		
		release(&stable.lock);
		return stable.slab[i].page[pageidx];
	}
	else{ //find free slot
		int bitidx;
		int page = 0, obj = 0;
		acquire(&stable.lock);
		for(bitidx = 0; bitidx<4096; bitidx++){
			if(stable.slab[i].bitmap[bitidx] != -1){ //free object
				int bit = (int)(unsigned char)stable.slab[i].bitmap[bitidx];
				int o1 = 0;
				int o2 = 1;
				while(bit != 0){
					if(bit % 2 == 0){
						break;
					}
					else{
						o1++;
						o2 *= 2;
						bit /= 2;
					}
				}
				//modify bitmap
				stable.slab[i].num_free_objects--;
				stable.slab[i].num_used_objects++;
				stable.slab[i].bitmap[bitidx] += o2;
				if(bitidx == 0){
					page = 0;
					obj = o1;
				}
				else{
					page = bitidx/byte_per_page;
					obj = (bitidx%byte_per_page)*8 + o1;
				}
				break;
			}
		}
		release(&stable.lock);
		return stable.slab[i].page[page] + obj*stable.slab[i].size; 
	}
}

char *clear_bit(char *Byte, int i){
	char *temp = Byte;
	temp += (i/8);
	i = i - (i/8)*8;
	unsigned char cri = 0x80;
	*(temp + i) = *(temp + i) & ~(cri >> i);
	return temp;
}

void kmfree(char *addr, int size){
	/* fill in the blank */
	int i=0, j, k;
	int ssize = 8;

	acquire(&stable.lock);
	
	while(size > ssize){
		ssize = ssize * 2;
		i++;
	}
	int decrease_cnt=0;
	for(j=0; j < stable.slab[i].num_pages; j++){
		for(k=0; k < stable.slab[i].num_objects_per_page; k++){
			if(addr == (stable.slab[i].page[j] + k*stable.slab[i].size)){
				stable.slab[i].num_free_objects += 1;
				stable.slab[i].num_used_objects -= 1;
				clear_bit(stable.slab[i].bitmap, j * stable.slab[i].num_objects_per_page + k);
				if(stable.slab[i].num_used_objects < ((stable.slab[i].num_pages - 1) + decrease_cnt) * stable.slab[i].num_objects_per_page){
					stable.slab[i].num_free_objects -= stable.slab[i].num_objects_per_page;
					kfree(stable.slab[i].page[stable.slab[i].num_pages - 1 + decrease_cnt]);
					decrease_cnt--;
				}
				stable.slab[i].num_pages += decrease_cnt;
				release(&stable.lock);
				return;
			}
		}
	}
	
	release(&stable.lock);
	return;
}

void slabdump(){
	struct slab *s;

	cprintf("size\tnum_pages\tused_objects\tfree_objects\n");
	for(s = stable.slab; s < &stable.slab[NSLAB]; s++){
		cprintf("%d\t%d\t\t%d\t\t%d\n", s->size, s->num_pages, s->num_used_objects, s->num_free_objects);
	}
}
