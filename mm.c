/* This is the explicit list.
 * Add prev ptr
 * Add next ptr
 * Add static void insertfree_block(void * freeblkptr);
 * Add static void removefree_block(void * freeblkptr);
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#include <stdint.h> // For uintptr_t


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "az",
    /* First member's email address */
    "az@email.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4           // Word and header/footer size (bytes)
#define DSIZE       8           // Double word size (bytes)
#define CHUNKSIZE   (1 << 12)   // Extend heap by this amount (bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, compute addr of up/down blocks in queue of free blocks.*/
#define DNBLKP(bp) (char *) GET(bp) // addr of next block down the queue.
#define UPBLKP(bp) (char *) GET(bp + WSIZE) // addr of next block up the queue.

/* Given bp, get addr stored in free block for its down/up blocks in queue.*/
#define DN_BLK(bp) (size_t) GET(bp)
#define UP_BLK(bp) (size_t) GET(bp + WSIZE)

/* Turn on/off heap checker */
#define verbose 0 

#define heapcheck()
//#define heapcheck() mm_heapcheck();
//# define heapcheck() printf("%s: ", __func__); mm_heapcheck(__LINE__);
//#define DEBUG 0 
//#ifdef DEBUG
//#define heapcheck() mm_heapcheck();
//# define heapcheck(lineno) printf("%s: ", __func__); mm_heapcheck(__LINE__);
//#endif

/* Private global variables */
static char * heap_listp;
static char * root = NULL;
int count = 0;
int malloc_calls = 0;
int free_calls = 0;
/* =========================== Function prototypes =========================*/
/* malloc related */
static void * extend_heap(size_t words); 
static void * coalesce(void * bp); 
void *mm_malloc(size_t size); 
static void * find_fit(size_t asize); 
static void place(void * bp, size_t asize); 
void mm_free(void *ptr); 
void *mm_realloc(void *ptr, size_t size);

/* Insert, remove and splice blocks*/
static void spliceBlock(char * bp); 
static void insertFreeBlock(char * bp); 
static void removeBlock(void * bp); 

/* Debug related */
void mm_heapcheck();
static void checkHeapBlockInvariants();
static void checkListBlockInvariants(); 
static void printBlocks(); 
static void printFreeList(); 
static void checkAllFreeBlocks(); 
static void noAllocBlocks(); 

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Prologue header
    PUT(heap_listp + (3*WSIZE), PACK(0,1));         // Epilogue header
    heap_listp += (2*WSIZE);

    //printf("before extend...\n");
    //heapcheck();
    //exit(0);
    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    //printf("after extend\n");
    //heapcheck();
    //exit(0);
    return 0;
}

static void * extend_heap(size_t words) {
    if (verbose) {printf("in extend heap\n");}
    char * bp;
    size_t size;

    // Allocate an even number of words to maintain aligment.
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    // Initialise free block header/footer and the epilogue header.
    PUT(HDRP(bp), PACK(size, 0));           // Free block header
    PUT(FTRP(bp), PACK(size, 0));           // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // New epilogue header

    // Initialise next and prev pointers.
    PUT(bp, 0); // next ptr points to nothing.
    PUT(bp + WSIZE, 0); // prev ptr points to nothing.

    // Coalesce if the previous block was free.
    return coalesce(bp);
}


/*
 * coalesce: 
 */
static void * coalesce(void * bp) {

    if (verbose) {printf("Running coalesce.\n");}
    //heapcheck();

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // Prev block free?
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // Next block free?
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) { // case 1
        if (verbose) {printf("coalesce case 1. Before insertFreeBlock()\n");}
        insertFreeBlock(bp);
        if (verbose) {printf("coalesce case 1. After insertFreeBlock()\n");}
        //heapcheck();
        //exit(0);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { // case 2
        
        // Splice out successor block.
        spliceBlock(NEXT_BLKP(bp));

        // Coalesce current block with succesor block.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // Add free block's size.
        PUT(HDRP(bp), PACK(size, 0)); // Update block's header to size | 0.
        PUT(FTRP(bp), PACK(size, 0)); // Update block's footer to size | 0.

        // Since it has been coalesced, remove successor block from queue.

        

        // Move the new block to the head of queue.
        if (verbose) {printf("coalesce case 2. Before insertFreeBlock()\n");}
        insertFreeBlock(bp);
        if (verbose) {printf("coalesce case 2. After insertFreeBlock()\n");}
        //heapcheck();
        //exit(0);
    }

    else if (!prev_alloc && next_alloc) { // case 3

        // Optimisation: Coalesce & let the block remain in current queue posn.
        // Don't splice block or move the new block to the head of queue. 
        // spliceBlock(PREV_BLKP(bp));Splice out predecessor block.

        //Coalesce current block with predecessor block.
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (verbose) {printf("coalesce case 3\n");}
        //heapcheck();
        //exit(0);
    }

    else {

        // Splice out predecessor and successor blocks.
        //if (verbose) {printf("coalesce case 4. Before splicing prev block.\n");}
        //heapcheck();
        spliceBlock(PREV_BLKP(bp));
        //if (verbose) {printf("coalesce case 4. Before splicing next block.\n");}
        //heapcheck();
        spliceBlock(NEXT_BLKP(bp));
        //if (verbose) {printf("coalesce case 4. After splicing next block.\n");}
        //heapcheck();
        
        // Coalesce all three blocks.
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        if (verbose) {printf("coalesce case 4. After coalescing 3 blocks.\n");}
        //heapcheck();
        //exit(0);
        
        // Move the new block to the head of queue.
        //if (verbose) {printf("coalesce case 4. Before insertFreeBlock()\n");}
        insertFreeBlock(bp);
        //if (verbose) {printf("coalesce case 4. After insertFreeBlock()\n");}
        //heapcheck();
        //exit(0);
    }

    return bp;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    //malloc_calls++;
    if (verbose) {printf("\nmalloc(%d) with malloc_calls: %d\n", 
            size, malloc_calls);}
    size_t asize; // Adjusted block size.
    size_t extendsize; // Amount to extend heap if no fit.
    char * bp;

    // Ignore spurious requests.
    if (size == 0) {return NULL;}

    // Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE){
        asize = 8*DSIZE; // 4 hdr, 4 ftr, min 8 malloc.
    }
    else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // Search the free list for a fit.
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        removeBlock(bp);
        
        if (verbose) {printf("after malloc(%d)\n", size);}
        //heapcheck();
        //exit(0);
        //if (malloc_calls == 8) exit(0);

        return bp;
    }

    // No fit found. Get more memory and place the block.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    removeBlock(bp);
    
    if (verbose) { printf("after malloc(%d)\n", size);}
    //heapcheck(); 
    //exit(0);
    
    //if (malloc_calls == 8) exit(0);
    return bp;
}

static void * find_fit(size_t asize) {
    if (verbose) {printf("in find_fit (%d)\n", asize);}
    // First fit search
    void * bp = root; // Start searching for free block from root node
    while(bp) { //until no more blocks remain.
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
        bp = (void *) GET(bp); // Go to the next block down the queue.
    }
    return NULL; // No fit.
}

static void place(void * bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }

    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}



/*
 * mm_free - 
 */
void mm_free(void *ptr) {
    //free_calls++;
    
    if (ptr == 0) {return;}
    
    size_t size = GET_SIZE(HDRP(ptr));
    if (verbose) {printf("\nmm_free (%d), with free_calls: %d\n", 
            size, free_calls);}
    
    if(heap_listp == 0) { // Why need this?
        mm_init();
    }

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    if (free_calls == 8) {exit(0);}
    //heapcheck(); 
    //exit(0);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/* 
 * insertFreeBlock: adds this block to the head of a doubly linked list of 
 * free blocks. Results in the free blocks list maintaining a LIFO policy.
 * 
 * Expects root to point to the head of the free blocks or NULL.
 * Need logic for root->NULL for the following edge case:
 * When mm_init() calls heap_extend() for the first time, root does not point
 * to anything. The subsequent chain of func calls leads to inserting the newly
 * created [by extend_heap()] free block to the head of the queue. At this
 * point, root is still pointing to NULL.
 *
 * Expects 4 byte addresses.
 *
 * Expects bp to point to the start of a free block of at least 4*WSIZE bytes.
 * Function will use the bytes in the block, meaning if the block is not of 
 * [min] 4*WSIZE bytes mdriver will raise "gobbled bytes" error.
 * The same error will be raised if the bp points to an allocated block.
 * 
 * Bytes 0-3 are hdr. Bytes 12-15 are ftr. Function does not touch these bytes.
 * Bytes 4-7: Function updates to hold addr of current head of the free blocks.
 * Bytes 8-11: Updates to NULL. [No more free blocks up the queue.]
 * 
 * Points root ptr to byte 4 of this block.
 * Updates bytes 4-7 of prev head to hold address of this block.
 * 
 * Terms used to refer to blocks in queue:
 ** up: travel up towards the head of the queue.
 ** down: travel down towrds the end of the queue.
 * 
 * Invariants:
 ** root->down == thisBlock
 ** thisBlock->down->up == thisBlock
 ** downblock->up->down == downBlock
 ** thisBlock->up == NULL
 *
 * Returns: nothing.
 */

static void insertFreeBlock(char * bp) {
    if(verbose) {printf("running insertFreeBlock with bp: %p\n", bp);}
    //printf("root points to: %p, bp points to %p, DN_BLK(bp): %x, UP_BLK(bp): %x\n", 
    //        root, bp, DN_BLK(bp), UP_BLK(bp));
    //heapcheck();
    // Move new block to the head of the queue.
    PUT(bp, (uintptr_t)root);// uintptr_t/unsigned_int/size_t?

    // There is nothing further up the queue.
    PUT(bp + WSIZE, 0);

    // Point prev head to new head [this block].
    if (root) { // if root->NULL, do nothing.
        PUT(root + WSIZE, (uintptr_t) bp); // uintptr_t/unsigned_int/size_t?
    }

    // Point root to new head.
    root = bp;
}

/*
 * spliceBlock: Given ptr to a block, function will remove it from the queue.
 * Used when coalescing freed blocks.
 *
 * Expects a valid ptr to a free block, which contains the following:
 * bp + bytes 0 to 3: address of the block down the queue from this block.
 * bp + bytes 4 to 7: address of the block up the queue from this block.
 * 
 * Returns nothing.
 */
static void spliceBlock(char * bp) {

    assert(bp != 0);
    
    char * dbp = DNBLKP(bp);
    char * ubp = UPBLKP(bp);
    
    // Splice
    if (dbp) {
        PUT(dbp + WSIZE, (size_t) GET(bp + WSIZE)); //dbp->up = bp->up
    } 
    if (ubp) {
        PUT(ubp, (size_t) GET(bp)); //ubp->down = bp->down
    }

    // Edge case: root points to this block.
    if (root == bp) {
        root = dbp; // root now points to next block down the queue.
    }
}

/*
 * removeBlock: Removes an allocated block from the free list.
 * Expects bp to point to start of block.
 * Expects 32 bit addresses.
 * Exects hdr in bp - WSIZE bytes.
 * Expects addr of block after this block in queue in bp + 0 bytes.
 * Expects addr of block before this block in queue in bp + WSIZE bytes.
 * Expects hdr this block and of successor block to contain size info.
 * Expects that no two adjacent [contiguous] blocks are ever free. This is 
 * important. The function must determine whether or not  the original block 
 * was split into an allocated block [which this function will remove from free
 * list] and a remaining free block. The function checks the next block hdr for
 * its allocated status. If that block was free, then it must not have existed
 * before the current block was allocated. The function will not do any error
 * checking and expects coalesce() to have dealt with such scenarios.
 * 
 * Returns nothing.
 */
static void removeBlock(void * bp) {

    // case 1: allocated block is the head of queue and block was split.
    // The block remaining after the split becomes head of the queue.
    // Edge case: The remaining block is the only block in queue.

    // case 2: allocated block is the head of queue and block wasn't split.
    // The next block down the queue becomes new head of queue.
    // Edge case: There is no next free block. Free list is empty.

    // case 3: allocated block is not the head, but block was split.
    // The block remaining after split takes posn in queue of original block.

    // case 4: allocated block is not the head and block wasn't split.
    // Next block down the queue moves up one posn in the free list.
    
    char * nbp = NEXT_BLKP(bp); 
    char * dbp = (char *) GET(bp); // ptr to downBlock.
    char * ubp = (char *) GET(bp + WSIZE); // ptr to upBlock.
    
    if (!GET_ALLOC(HDRP(nbp))) { //split block
    
        PUT(nbp, (size_t) GET(bp));
        PUT(nbp + WSIZE, (size_t) GET(bp+WSIZE));

        if (dbp) { PUT(dbp+WSIZE, (size_t) nbp); }

        if (ubp) { PUT(ubp, (size_t) nbp); }

        if (root == bp) { root = nbp; }
    }

    else {

        if (dbp) { PUT(dbp + WSIZE, (size_t) GET(bp + WSIZE)); }
        if (ubp) { PUT(ubp, (size_t) GET(bp)); }
        //if (root == bp) { root = (uintptr_t) GET(bp); }
        if (root == bp) { root = (char *) GET(bp); }
    }
}

/*
 * mmheapcheck - calls numerous consistency checking functions.
 */
//void mm_heapcheck(int lineno) {
void mm_heapcheck() {

    //printf("heapcheck called from %d\n", lineno);
    printBlocks();
    printFreeList();
    //checkHeapBlockInvariants();
    //checkListBlockInvariants();
    //checkAllFreeBlocks(); 
    //noAllocBlocks(); 

    
}


/*
 * checkHeapBlockInvariants - does consistency check on the heap.
 * 1. All headers & footers have identical block sizes & allocated bits.
 * 2. All blocks are properly aligned.
 */
static void checkHeapBlockInvariants() {
    if (verbose) {printf("Running checkHeapBlockInvariants.\n");}
    char * bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    
        // header and footer match.
        assert(GET_SIZE(HDRP(bp)) == GET_SIZE(FTRP(bp)));
        assert(GET_ALLOC(HDRP(bp)) == GET_ALLOC(FTRP(bp)));

        // All blocks are properly aligned.
        assert(((unsigned long) bp) % (DSIZE) == 0); // actual size for addr?
    }
}

/* 
 * checkListInvariants: does consistency checking:
 * Next/prev ptrs in consecutive free blocks are consistent.
 * Free list contains no allocated blocks.
 * All free blocks are in the free list.
 * No contiguous free blocks in memory (unless coalescing deferred).
 * No cycles in the list (unless using circular lists).
 * Segregated list contains only blocks that belong to the size class.
 */

/*
 * checkListBlockInvariants: does consistency checks on free blocks.
 * 1. All up/down ptrs in consecutive free blocks are consistent.
 */
static void checkListBlockInvariants() {
    if (verbose) {printf("begin checkListBlockinvariants %d\n", count++);}

    char * bp;
    for (bp = root; bp != NULL; bp = (char *) GET(bp)) {
        char * dbp = DNBLKP(bp); //ptr to downBlock.
        char * ubp = UPBLKP(bp); //ptr to upBlock.
    
        // Edge case: complete fresh heap or no more free blocks.
        if (!dbp && ! ubp) { 
            if (verbose) {printf("!dbp && !ubp\n");}
            return;
        }
        
        // Edge case: at the bottom of the queue - no more block after this.
        else if (!dbp) {
            if(verbose) {printf("bp->up->down == bp\n");}
            assert(DNBLKP(UPBLKP(bp)) == bp);
        }
        
        // Edge case: at the head of the queue - no more block ahead of this.
        else if (!ubp) {
            if(verbose) {printf("bp->down->up == bp\n");}
            assert(UPBLKP(DNBLKP(bp)) == bp);
        }
    
        // Normal case: all the block not at the head or bottom of the queue.
        else {
            if (verbose) {printf("bp->up->down == bp\n");}
            assert(DNBLKP(UPBLKP(bp)) == bp);
            if (verbose) {printf("bp->down->up == bp\n");}
            assert(UPBLKP(DNBLKP(bp)) == bp);
        }
        if (verbose) {printf("end checkListBlockinvariants %d\n", count++);}
    }
}

/*
 * myprintblock - prints the contents of the header and footer 
 * of all blocks in the contiguous list.
 */
static void printBlocks() {
    char * bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        printf("header: [size: %d, alloc: %d] footer: [size: %d, alloc: %d]\n",
            GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)),
            GET_SIZE(FTRP(bp)), GET_ALLOC(FTRP(bp)));
    }
}


static void printFreeList() {
    if (verbose) {printf("Running printFreeList\n");}

    char * bp;
    for (bp = root; bp != NULL; bp = (char *) GET(bp)) {
        printf("header: [size: %d, alloc: %d] footer: [size: %d, alloc: %d]\n",
            GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)),
            GET_SIZE(FTRP(bp)), GET_ALLOC(FTRP(bp)));
    
        printf("root points to: %p, bp points to %p, DN_BLK(bp): %x, "
                "UP_BLK(bp): %x\n", root, bp, DN_BLK(bp), UP_BLK(bp));
        
        
        printf("hdr: [size: %d, alloc: %d], DN_BLK(bp): %x, UP_BLK(bp): %x"
              "ftr: [size: %d, alloc: %d], root pts to: %p, bp pts to %p\n",
              GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), DN_BLK(bp), UP_BLK(bp),
              GET_SIZE(FTRP(bp)), GET_ALLOC(FTRP(bp)), root, bp);
    }
}


/*
 * checkAllocStatus: Free list contains no allocated blocks.
 */
static void noAllocBlocks() {
    
    if (verbose) {printf("Running checkAllocStatus\n");}

    char * bp;
    for (bp = root; bp != NULL; bp = (char *) GET(bp)) {
        assert(GET_ALLOC(HDRP(bp) == 0));
    }
}

/*
 * checkAllFreeBlocks: All free blocks are in the free list.
 * Each block in the queue must be in the heap.
 * Each free block in the heap must be in the queue.
 */
static void checkAllFreeBlocks() {
    if (verbose) {printf("Running checkAllFreeBlocks\n");}

    char * bp; // base ptr in the heap.
    char * fbp; // base ptr in the queue.
    int match;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        
        if (!GET_ALLOC(HDRP(bp))) { // free block
        
            printf("header: [size: %d, alloc: %d] footer: [size: %d, alloc: %d]\n",
            GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)),
            GET_SIZE(FTRP(bp)), GET_ALLOC(FTRP(bp)));
            
            /*
            // hrd, ftr & addresses match in heap and queue.
            match = 0;
            for (fbp = root; fbp != NULL; fbp = (char *) GET(fbp)) {
                
                if ((GET_ALLOC(HDRP(fbp)) == GET_ALLOC(HDRP(bp))) &&
                        (GET_ALLOC(FTRP(fbp)) == GET_ALLOC(FTRP(bp))) &&
                        (GET_SIZE(HDRP(fbp)) == GET_SIZE(HDRP(bp))) &&
                        (GET_SIZE(FTRP(fbp)) == GET_SIZE(FTRP(bp))) &&
                        (fbp == bp)) {
                    match = 1;
                }
            }
            assert(match);*/
        }
    }
    printFreeList();
    /*
    
    // every free block is in the heap.
    for (fbp = root; fbp != NULL; fbp = (char *) GET(fbp)) {
        match = 0;
        for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
                if ((GET_ALLOC(HDRP(fbp)) == GET_ALLOC(HDRP(bp))) &&
                        (GET_ALLOC(FTRP(fbp)) == GET_ALLOC(FTRP(bp))) &&
                        (GET_SIZE(HDRP(fbp)) == GET_SIZE(HDRP(bp))) &&
                        (GET_SIZE(FTRP(fbp)) == GET_SIZE(FTRP(bp))) &&
                        (fbp == bp)) {
                    match = 1;
                }
        }
        assert(match);
    }*/
}




































