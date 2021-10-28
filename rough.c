#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#include <stdint.h> // For uintptr_t

#define WSIZE 4
#define GET(p) (*(unsigned int *)(p)) 
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define DNBLKP(bp) (char *) GET(bp)
#define UPBLKP(bp) (char *) GET(bp + WSIZE)

static void spliceBlock(char * bp); 
static void invar(char * bp); 
int count = 0;

int main() {
    
    int size = 5;
    char * list[size];
    int i;
    for (i = 0; i < size; i++) {
        list[i] = malloc(8);
    }
    
    for (i = 0; i < size; i++) {
        
        char * bp = list[i];
        if (i == 0) {PUT(bp, 0);}
        else { PUT(bp, (size_t) list[i-1]);}
        if (i == (size -1)) {PUT(bp+WSIZE, 0);}
        else {PUT(bp+WSIZE, (size_t) list[i+1]);}
    }

/*
    for (i = 0; i < size; i++) {
        char * bp = list[i];
        printf("addr bp[%d][0]: %p, addr bp[%d][1]: %p\n", i, bp, i, bp+4);
        
        char * dbp = DNBLKP(bp);
        char * ubp = UPBLKP(bp);
        printf("DNBLKP(bp): %p, UPBLKP(bp): %p\n", DNBLKP(bp), UPBLKP(bp));
    }*/

    for (i = 0; i < size; i++) {
        char * bp = list[i];
        invar(bp);
    }
    
    printf("end main\n");
    return 0;
}

#define DEBUG 
static void invar(char * bp) {
    printf("begin invar %d\n", count++);
    
    //Next/prev ptrs in consecutive free blocks are consistent.
    
    char * dbp = DNBLKP(bp); //ptr to downBlock.
    char * ubp = UPBLKP(bp); //ptr to upBlock.
    
    // Edge case: at the bottom of the queue - no more block after this one.
    if (!dbp) {
        printf("bp->up->down == bp\n");
        assert(DNBLKP(UPBLKP(bp)) == bp);
        //printf("bp->up->up->down->down == bp\n");
        //assert(DNBLKP(DNBLKP(UPBLKP(UPBLKP(bp)))) == bp);
    }
        
    // Edge case: at the head of the queue - no more block ahead of this one.
    else if (!ubp) {
        printf("bp->down->up == bp\n");
        assert(UPBLKP(DNBLKP(bp)) == bp);
        /*if (dbp){
            printf("bp->down->down->up->up == bp\n");
            assert(UPBLKP(UPBLKP(DNBLKP(DNBLKP(bp)))) == bp);
        }*/
    }
    
    // Normal case: all the block not at the head or bottom of the queue.
    else {
        printf("bp->up->down == bp\n");
        assert(DNBLKP(UPBLKP(bp)) == bp);
        printf("bp->down->up == bp\n");
        assert(UPBLKP(DNBLKP(bp)) == bp);
    }
       /* 
        if (GET(ubp)) {
            printf("GET(ubp)%d\n", GET(ubp));
            printf("bp->up->up->down->down == bp\n");
            assert(DNBLKP(DNBLKP(UPBLKP(UPBLKP(bp)))) == bp);
        }

        if (GET(dbp)) {
            printf("bp->down->down->up->up == bp\n");
            assert(UPBLKP(UPBLKP(DNBLKP(DNBLKP(bp)))) == bp);
        }

        printf("bp->down->up->up->down == bp\n");
        assert(DNBLKP(UPBLKP(UPBLKP(DNBLKP(bp)))) == bp);
        printf("bp->up->down->down->up == bp\n");
        assert(UPBLKP(DNBLKP(DNBLKP(UPBLKP(bp)))) == bp);

    
        //printf("dbp->up->up->down->down == bp\n");
        //assert(DNBLKP(DNBLKP(UPBLKP(UPBLKP(dbp)))) == dbp);
        //printf("ubp->down->down->up->up == ubp\n");
        //assert(UPBLKP(UPBLKP(DNBLKP(DNBLKP(ubp)))) == ubp);
    }*/
    printf("end invar\n=====================\n\n");
}

int main_0() {
    
    char * dbp;
    char * bp;
    char * ubp;

    dbp = malloc(8);
    bp = malloc(8);
    ubp = malloc(8);
   
 
    printf("addr dbp[0]: %p, addr dbp[1]: %p\n", dbp, &dbp[4]);
    printf("addr bp[0]: %p, addr bp[1]: %p\n", bp, &bp[4]);
    printf("addr ubp[0]: %p, addr ubp[1]: %p\n", ubp, &ubp[4]);

    // Put addresses
    PUT(dbp, 0); //dbp->down = 0
    PUT(dbp+WSIZE, (size_t) bp); //dbp->up = bp
    
    PUT(bp, (size_t) dbp); //bp->down = dbp
    PUT(bp+WSIZE, (size_t) ubp); //bp->up = ubp

    PUT(ubp, (size_t) bp); //ubp->down = bp
    PUT(ubp+WSIZE, 0); //ubp->up = 0
    
    printf("dbp->down: %x, dbp->up: %x\n",GET(dbp), GET(dbp+WSIZE)) ;
    printf("bp->down: %x, bp->up: %x\n",GET(bp), GET(bp+WSIZE)) ;
    printf("ubp->down: %x, ubp->up: %x\n",GET(ubp), GET(ubp+WSIZE)) ;
    
    
    printf("DNBLKP(bp): %p, UPBLKP(bp): %p\n", DNBLKP(bp), UPBLKP(bp));

    invar(bp);
    printf("end main\n");
    return 0;
}


static void spliceBlock(char * bp) {

    char * ubp = (char *) GET(bp); //ptr to upBlock.
    //char * nbp = GET(bp); //ptr to upBlock.
    char * pbp = (char *) GET(bp+WSIZE); //ptr to upBlock.
    
    // Splice
    PUT(pbp, (size_t) GET(bp)); //pbp->up = ubp
    PUT(ubp+WSIZE, (size_t) GET(bp+WSIZE)); //ubp->down = pbp
    
    return;
}

