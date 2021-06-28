/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

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

/* Turn off heap checker */
//#define heapcheck(lineno) mm_heapcheck(lineno)
#define heapcheck(lineno)

/* Private global variables */
static char * heap_listp;
static unsigned int request_id;


/* Function prototypes */
static void * extend_heap(size_t words); 
static void * coalesce(void * bp); 
void *mm_malloc(size_t size); 
static void * find_fit(size_t asize); 
static void place(void * bp, size_t asize); 
void mm_free(void *ptr); 
void *mm_realloc(void *ptr, size_t size);
static void myheapcheck();
static void myprintblock(char * bp);
static void mycheckblock(char * bp);
void mm_heapcheck();

/*
 * myheapcheck - prints and checks for consistency each free/allocated block. 
 */
static void myheapcheck() {

    char * bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        myprintblock(bp);
        mycheckblock(bp);
    }
}

/*
 * mmheapcheck - prints and checks for consistency each free/allocated block. 
 */
void mm_heapcheck(int lineno) {

    printf("checkheap called from %d\n", lineno);
    
}

/*
 * myprintblock - prints the contents of the header and footer 
 * of block bp on a single output line.
 */
static void myprintblock(char * bp) {
    
    char alloc;
    if (GET_ALLOC(HDRP(bp))) {alloc = 'a';}
    else {alloc = 'f';}

    printf("%c: header: [%d:%c", alloc, GET_SIZE(HDRP(bp)), alloc);
    
    if (alloc == 'a') {
        printf(", %d, %d] ", GET(bp), GET(bp + (WSIZE)));
    }
    else {
        printf("] ");
    }

    printf("footer: [%d:%c]\n", GET_SIZE(FTRP(bp)), alloc);     
}

/*
 * mycheckblock - does consistency checking such as making sure that headers
 * and footers have identical block sizes and allocated bits, and that all
 * blocks are properly aligned.
 */
static void mycheckblock(char * bp) {

    // header and footer match.
    assert(GET_SIZE(HDRP(bp)) == GET_SIZE(FTRP(bp)));
    assert(GET_ALLOC(HDRP(bp)) == GET_ALLOC(FTRP(bp)));

    // All blocks are properly aligned.
    assert(((unsigned long) bp) % (DSIZE) == 0); // actual size for addr?
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    request_id = -1;
    
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                                 // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(2*(DSIZE), 1));    // Prologue header
    PUT(heap_listp + (2*WSIZE), request_id);            // request_id
    PUT(heap_listp + (3*WSIZE), 0);                     // payload_size
    PUT(heap_listp + (4*WSIZE), PACK(2*(DSIZE), 1));    // Prologue header
    PUT(heap_listp + (5*WSIZE), PACK(0,1));             // Epilogue header
    heap_listp += (2*WSIZE);

    //printf("before extend...\n");
    //myheapcheck();

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    //printf("after extend...\n");
    //myheapcheck();
    return 0;
}

static void * extend_heap(size_t words) {
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

    // Coalesce if the previous block was free.
    return coalesce(bp);
}


/*
 * coalesce: 
 */
static void * coalesce(void * bp) {

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // Prev block free?
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // Next block free?
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) { // case 1
        return bp;
    }

    else if (prev_alloc && !next_alloc) { // case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // Add free block's size.
        PUT(HDRP(bp), PACK(size, 0)); // Update block's header to size | 0.
        PUT(FTRP(bp), PACK(size, 0)); // Update block's footer to size | 0.
    }

    else if (!prev_alloc && next_alloc) { // case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize; // Adjusted block size.
    size_t extendsize; // Amount to extend heap if no fit.
    size_t payload_size; // Actual payload.
    char * bp;

    // Ignore spurious requests.
    if (size == 0) {return NULL;}

    // Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE){
        asize = 3*DSIZE; // 4 hdr, 4 ftr, 4 req_id, 4 size, min 8 malloc.
    }
    else {
        asize = 2*WSIZE + DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    payload_size = asize - 4*WSIZE; // payload_size

    // Search the free list for a fit.
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        PUT(bp, ++request_id);
        PUT(bp + (1*(WSIZE)), payload_size); 
        
        //printf("after malloc(%d)\n", size);
        //myheapcheck();
        heapcheck(__LINE__); 
        
        return (bp + (2*(WSIZE)));
    }

    // No fit found. Get more memory and place the block.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    PUT(bp, ++request_id);
    PUT(bp + (1*(WSIZE)), payload_size); 
    
    //printf("after malloc(%d)\n", size);
    //myheapcheck();
    heapcheck(__LINE__); 
    
    return (bp + (2*(WSIZE)));
}

static void * find_fit(size_t asize) {
    // First fit search
    void * bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
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

    if (ptr == 0) {return;}
    
    // Decrement to account for request_id & payload_size.
    ptr = (char *) ptr - 2*(WSIZE);
    //printf("after free(%d)\n", GET(ptr));
    
    size_t size = GET_SIZE(HDRP(ptr));
    
    if(heap_listp == 0) { // Why need this?
        mm_init();
    }

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    heapcheck(__LINE__); 
    //myheapcheck();
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
