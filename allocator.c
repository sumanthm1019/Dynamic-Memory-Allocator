/*
 * firstfit.c
 *
 *  Created on: May 21, 2016
 *      Author: sumanth
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct block {
	int free;
	struct block *next;
	struct block *prev;
	size_t size;
	void *ptr;
	char end[1];
} block_t;

void *start = NULL;

#define BLOCK_SIZE 20
#define align4(x)  ((((x - 1) >> 2) << 2) + 4) //4 byte aligned

block_t *findFirstFit(block_t **last, size_t size);
block_t *extendHeap(block_t *last, size_t size);
void blockSplit(block_t *b, size_t size);
void *sCalloc(size_t num, size_t size);
void *sMalloc(size_t size);
void free(void *addr);
block_t *blockFusion(block_t *b);
block_t *getBlock(void *addr);
void *realloc(void *p, size_t size);
void copyBlock(block_t *src, block_t dst);

int main() {

	char *x = (char *) sCalloc(4, sizeof(char));
	int i;
	for (i = 0; i < 32; i++) {
		printf("x[%d] = %c\n", i, x[i]);
	}
	return 0;
}

/*When a block is found but is of larger size than requested,
 *the large block is split into two blocks, one that is requested
 *and another with the remaining space only if the remaining space is
 sufficient to hold a block of data*/
void blockSplit(block_t *b, size_t size) {

	block_t *newnode;
	newnode = (block_t *) (b->end + size);
	newnode->next = b->next;
	newnode->free = 1;
	newnode->size = b->size - size - BLOCK_SIZE;
	newnode->prev = b;
	newnode->ptr = newnode->end;
	b->next = newnode;
	b->size = size;
	if (newnode->next)
		newnode->next->prev = newnode;

}

/*Calloc calls the malloc function and initializes every byte of
 * allocated memory to zero
 */
void *sCalloc(size_t num, size_t size) {
	unsigned int *b;
	int i;
	size_t s4;
	s4 = align4(num * size) >> 2;
	b = sMalloc(num * size);

	for (i = 0; i < s4; i++) {
		b[i] = 0;
	}
	return b;

}
/*The first fit algorithm, finds the first possible block of memory
 * for the requested size. This way the performance is very high, however the block
 * might not fit perfectly in the memory assigned leading to fragmentation
 */
block_t *findFirstFit(block_t **last, size_t size) {

	block_t *b = *last;

	while (b && !(b->free == 1 && (b->size >= size))) {
		*last = b;
		b = b->next;

	}
	return b;

}
/*The best fit algorithm tries to allocate a block of memory closest to the
 * requested size thereby reducing fragmentation. The search for the block
 * results in O(n) complexity reducing the performance.
 */
block_t *findBestFit(block_t **last, size_t size) {

	block_t *b = *last;
	size_t min = 100000000;
	block_t *minb = NULL;
	while (b) {
		if ((min > abs(b->size - size)) && b->free == 1 && b->size >= size) {
			min = abs(b->size - size);
			minb = b;
			if (min == 0)
				break;
		}
		*last = b;
		b = b->next;
	}
	return minb;

}
/*The worst fit algorithm tries to allocate the block in the largest memory
 * available therefore reducing very small chunks of memory in the heap.
 * Search takes O(n) time.
 */
block_t *findWorstFit(block_t **last, size_t size) {

	block_t *b = *last;
	size_t max = 0;
	block_t *maxb = NULL;
	while (b) {

		if ((max < b->size) && b->free == 1 && b->size >= size) {
			max = b->size;
			maxb = b;
		}
		*last = b;
		b = b->next;
	}
	return maxb;

}
/*The extend heap function, extends the heap memory by extending the breakpoint
 * and returning the older breakpoint address thereby creating new memory for usage.
 */
block_t *extendHeap(block_t *last, size_t size) {

	block_t *b;
	b = (block_t *) sbrk(0);
	if (sbrk(size + BLOCK_SIZE) == (void *) -1) {

		return NULL;
	}
	b->size = size;
	b->free = 0;
	b->next = NULL;
	b->prev = last;
	b->ptr = b->end;
	if (last) {
		last->next = b;
	}
	return b;

}
/*Malloc using the first fit algorithm
 * if a block is found using first fit, it is returned
 * after using block split to reduce fragmentation
 * else a new block is created using extend heap
 */
void *sMalloc(size_t size) {

	block_t *b, *last;
	size = align4(size);
	if (start) {
		last = start;
		b = findFirstFit(&last, size);
		if (b) {

			if ((b->size - size) > (BLOCK_SIZE + 4)) {
				blockSplit(b, size);
			}

			b->free = 0;

		} else {

			b = extendHeap(last, size);
			if (!b) {
				return NULL;
			}
		}

	} else {

		b = extendHeap(start, size);
		if (!b) {
			return NULL;
		}
		start = b;
	}

	return b->end;
}

block_t *getBlock(void *addr) {

	return addr - BLOCK_SIZE;
}
/*Check if the address provided to the "Free" function
 * is actually a valid address, one that was returned by a previous malloc
 * This is done by storing the address in a pointer
 */
int isAddrValid(void *addr) {

	if (start) {

		if (addr > start && addr < sbrk(0))
			return (addr == ((getBlock(addr))->ptr));

	}
	return 0;
}
/*Once a block is freed, an effort is made to coalesce memory blocks
 * together to keep memory away from fragmentation. The function checks
 * the next block and if it is free, fuses it with this block.
 */

block_t *blockFusion(block_t *b) {

	if (b->next && b->next->free) {
		b->size = BLOCK_SIZE + b->next->size;
		b->next = b->next->next;
		if (b->next)
			b->next->prev = b;
	}
	return b;

}
/*Free function frees the memory block specified by the start
 * address of the block. Block fusion with the previous and next
 * memory blocks is performed once it is freed
 */
void free(void *addr) {

	block_t *b;
	if (isAddrValid(addr)) {

		b = getBlock(addr);
		b->free = 1;
		if (b->prev && b->prev == free) {
			b = blockFusion(b->prev);
		}
		if (b->next) {
			b = blockFusion(b);
		} else {
			if (b->prev)
				b->prev->next = NULL;
			else
				start = NULL;

			brk(b);
		}
	}
}
/*Copy block of data from source to destination
 * Used when realloc is called and a larger size is requested
 */
void copyBlock(block_t *src, block_t dst) {
	char *s = src->ptr;
	char *d = dst->ptr;

	int i, n;
	n = src->size < dst->size ? src->size : dst->size;
	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
}
/*Realloc checks if the requested size is less than the current size
 * If it is larger, then the block is copied to a different region of memory
 * and current block is freed */
void *realloc(void *p, size_t size) {

	size_t s;
	block_t *b, *newnode;
	void *newp;
	if (!p) {
		return sMalloc(size);
	}
	if (isAddrValid(p)) {
		s = align4(size);
		b = getBlock(p);
		if (b->size >= s) {
			if (b->size - s >= (BLOCK_SIZE + 4))
				blockSplit(b, s);
		} else {

			if (b->next && b->next->free
					&& (b->size + BLOCK_SIZE + b->next->size) >= s) {
				blockFusion(b);
				if (b->size - s >= (BLOCK_SIZE + 4))
					blockSplit(b, s);
			} else {
				newp = sMalloc(s);
				if (!newp)
					return NULL;
				newnode = getBlock(newp);
				copyBlock(b, newnode);
				free(p);
				return newp;
			}

		}
		return p;
	}

	return NULL;

}
