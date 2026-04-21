// Basic implicit free list allocator for CS:APP malloc lab
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "memlib.h"
#include "mm.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define WSIZE       4             // Word and header/footer size (bytes)
#define DSIZE       8             // Double word size (bytes)
#define CHUNKSIZE  (1<<12)        // Extend heap by this amount (bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc)  ((size) | (alloc))

// Read and write a word at address p
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

// Read the size and allocated fields from address p
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *heap_listp = NULL;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

int mm_init(void) {
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                         // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2*WSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            // Case 1
        return bp;
    } else if (prev_alloc && !next_alloc) {    // Case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {    // Case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {                                   // Case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *find_fit(size_t asize) {
    // First-fit search
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; // No fit
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *nbp = NEXT_BLKP(bp);
        PUT(HDRP(nbp), PACK(csize-asize, 0));
        PUT(FTRP(nbp), PACK(csize-asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void *mm_malloc(size_t size) {
    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;

    if (size == 0)
        return NULL;

    // Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr) {
    if (ptr == NULL) return;
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr)) - DSIZE;
    size_t copySize = oldsize < size ? oldsize : size;

    // Try to expand in place if next block is free and enough space
    size_t needed;
    if (size <= DSIZE) needed = 2*DSIZE; else needed = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    size_t csize = GET_SIZE(HDRP(ptr));
    void *next = NEXT_BLKP(ptr);
    if (!GET_ALLOC(HDRP(next))) {
        size_t nextsize = GET_SIZE(HDRP(next));
        if (csize + nextsize >= needed) {
            // Merge with next
            PUT(HDRP(ptr), PACK(csize+nextsize, 1));
            PUT(FTRP(ptr), PACK(csize+nextsize, 1));
            // Now maybe split
            size_t total = csize + nextsize;
            if (total - needed >= 2*DSIZE) {
                PUT(HDRP(ptr), PACK(needed, 1));
                PUT(FTRP(ptr), PACK(needed, 1));
                void *nbp = NEXT_BLKP(ptr);
                PUT(HDRP(nbp), PACK(total-needed, 0));
                PUT(FTRP(nbp), PACK(total-needed, 0));
            }
            return ptr;
        }
    }

    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}

void *mm_calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *p = mm_malloc(bytes);
    if (p) memset(p, 0, bytes);
    return p;
}

// Simple heap checker: verify prologue/epilogue and no two consecutive free blocks
void mm_checkheap(void) {
    char *bp = heap_listp;
    // Prologue checks
    if (GET_SIZE(HDRP(bp)) != DSIZE || !GET_ALLOC(HDRP(bp))) {
        fprintf(stderr, "Bad prologue header\n");
        return;
    }
    size_t prev_free = 0;
    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        size_t size = GET_SIZE(HDRP(bp));
        size_t alloc = GET_ALLOC(HDRP(bp));
        if (size % ALIGNMENT) {
            fprintf(stderr, "Bad alignment on block\n");
        }
        if (!alloc && prev_free) {
            fprintf(stderr, "Consecutive free blocks not coalesced\n");
        }
        prev_free = !alloc;
    }
}

team_t team = {
    .teamname = "codex",
    .name1 = "Agent",
    .id1 = "agent",
    .name2 = "",
    .id2 = ""
};

