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

char offset[9] = {64, 32, 16, 8, 4, 2, 1, 1, 1};

void slabinit(){
	/* fill in the blank */
	int i;
	int ssize = 8;
	int page_size = 4096;
	
	acquire(&stable.lock);
	for(i=0; i < NSLAB; i++){
		stable.slab[i].size = ssize;
		stable.slab[i].num_pages = 1;
		stable.slab[i].num_free_objects = page_size/stable.slab[i].size;
		stable.slab[i].num_used_objects = 0;
		stable.slab[i].num_objects_per_page = page_size/stable.slab[i].size;
		stable.slab[i].bitmap = kalloc();
		stable.slab[i].page[0] = kalloc();
		ssize = ssize * 2;
	}
	release(&stable.lock);
}

char get_bit(char Byte, int n){
	return (Byte & (1 << (7-n))) >> (7-n);
}

char set_bit(char Byte, int n, int bit){
	if(bit)
		return Byte | (1 << (7-n));
	return Byte & (~(1 << (7-n)));
}

char *clear_bit(char *Byte, int i){
	char *temp = Byte;
	temp += (i/8);
	i = i - (i/8)*8;
	unsigned char cri = 0x80;
	*(temp + i) = *(temp + i) & ~(cri >> i);
	return temp;
}

int find_free_obj(int index, int page){
	int i, j;
	char *bitmap = stable.slab[index].bitmap + page*offset[index];
	char Byte;

	for(i=0; i < offset[index]; i++){
		Byte = *(bitmap + i);
		for(j=0; j < 8; j++){
			if(j >= stable.slab[index].num_objects_per_page){
				break;
			}
			if(get_bit(Byte, i) == 0){
				*(bitmap + i) = set_bit(Byte, j, 1);
				return i*8 + j;
			}
		}
	}
	return -1;
}

char *kmalloc(int size){
	/* fill in the blank */
	int index = 0;
	int object = 0;
	int ssize = 8;
	while(size > ssize){
		ssize = ssize * 2;
		index++;
	}

	acquire(&stable.lock);
	if(stable.slab[index].num_free_objects == 0){
		stable.slab[index].page[stable.slab[index].num_pages] = kalloc();
		stable.slab[index].num_pages++;
		stable.slab[index].num_free_objects += stable.slab[index].num_objects_per_page;
	}
	
	for(int i=0; i < stable.slab[index].num_pages; i++){
		object = find_free_obj(index, i);	// bitmap
		if(object != -1){
			// update metadata
			stable.slab[index].num_free_objects -= 1;
			stable.slab[index].num_used_objects += 1;
			release(&stable.lock);
			return stable.slab[index].page[i] + object*stable.slab[index].size;
		}
	}

	release(&stable.lock);
	return 0;
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
	int down_cnt=0;
	for(j=0; j < stable.slab[i].num_pages; j++){
		for(k=0; k < stable.slab[i].num_objects_per_page; k++){
			if(addr == (stable.slab[i].page[j] + k*stable.slab[i].size)){
				stable.slab[i].num_free_objects += 1;
				stable.slab[i].num_used_objects -= 1;
				clear_bit(stable.slab[i].bitmap, j * stable.slab[i].num_objects_per_page + k);
				if(stable.slab[i].num_used_objects < ((stable.slab[i].num_pages - 1) + down_cnt) * stable.slab[i].num_objects_per_page){
					stable.slab[i].num_free_objects -= stable.slab[i].num_objects_per_page;
					kfree(stable.slab[i].page[stable.slab[i].num_pages - 1 + down_cnt]);
					down_cnt--;
				}
				stable.slab[i].num_pages += down_cnt;
				release(&stable.lock);
				return;
			}
		}
	}
	
	release(&stable.lock);
	return;
}

void slabdump(){

	cprintf("__slabdump__\n");

	struct slab *s;
	cprintf("size\tnum_pages\tused_objects\tfree_objects\n");
	
	for(s = stable.slab; s < &stable.slab[NSLAB]; s++){
		cprintf("%d\t%d\t\t%d\t\t%d\n", 
			s->size, s->num_pages, s->num_used_objects, s->num_free_objects);
	}

}

