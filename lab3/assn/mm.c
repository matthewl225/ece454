/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "DownloadMoreRam.com",
    /* First member's full name */
    "Connor Smith",
    /* First member's email address */
    "connor.smith@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Fan Guo",
    /* Second member's email address (leave blank if none) */
    "cfan.guo@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define OVERHEAD    WSIZE
#define OVERHEAD_4  OVERHEAD * 4;
#define OVERHEAD_2  OVERHEAD * 2
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

void* heap_listp = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                         // alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
    heap_listp += DSIZE;

    return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    char *prev_blkp = PREV_BLKP(bp);
    char *next_blkp = NEXT_BLKP(bp);

    size_t prev_alloc = GET_ALLOC(FTRP(prev_blkp));
    size_t next_alloc = GET_ALLOC(HDRP(next_blkp));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(next_blkp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        // two blocks need 4 tags, 1 block needs 2 tags therefore add 2 tags when coalescing
        size += GET_SIZE(HDRP(prev_blkp)) + OVERHEAD_2;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev_blkp), PACK(size, 0));
        return (prev_blkp);
    }

    else {            /* Case 4 */
        // 3 blocks need 6 tags, 1 block need 2 tags therefore add 4 tags to the block size
        size += GET_SIZE(HDRP(prev_blkp))  +
            GET_SIZE(FTRP(next_blkp))  + OVERHEAD_4;
        PUT(HDRP(prev_blkp), PACK(size,0));
        PUT(FTRP(prev_blkp), PACK(size,0));
        return (prev_blkp);
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    /** Original Implementation, TODO: delete?
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
    */

}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
// TODO probably not needed anymore
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    // TODO: set the free bit and coalesce
    // will have to place block into either wilderness or free list
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    // 1. asize = 16byte_round(size). find free list index for asize. better_fit_size = (1<<(list_index-1) | 1<<(list_index-2))
    // 2. if better_fit_size >= size
    //    2a. Check the "wilderness" area for a size bigger than better_fit_size. Wilderness should try to be sorted or in some kind of BST
    //    2b. if found, return that pointer and subtract from the wilderness block size. If it's a power of 2, add the remaining size to a regular index, otherwise put back in Wilderness
    //    2c. if not found, sbrk a better_fit_size block and return that.
    // 3. if better_fit_size < size
    //    3a. check the free list index to see if a block is there. If yes, return that block and remove from free list
    //    3b. if no, check the wilderness and split a block from there if possible.
    //    3c. if nothing in wilderness will work, sbrk a block size corresponding to this index and return that
    //
    // Note: For sbrk, check a global pointer for the block at the very end of the heap. sbrk enough such that this block can fulfill the request


    /** Fragmentation Analysis
     * 
     * Worst case is allocating 2^n+1 sizes. (e.g. 33 bytes, 65 bytes, 129 bytes)
     * in this case, we'd use a better_fit_size block of 2^n + 2^(n-1) rather than 2^(n+1) = 2*2^n blocks.
     * Fragmentation = (16 + 2^n + 2^(n-1) - 2^n - 1)/(2^n + 2^(n-1)) = (2^(n-1) + 15)/(3*2^(n-1)) which is 48% fragmentation for 33 bytes, and tends towards 33% as size increases
     * If we didn't use better_fit_size blocks, we'd have Fragmentation = (16 + 2^n - 1)/(2*2^n) which is 58% fragmentation for 33 bytes, and tends towards 50% as size increases
     * Therefore, we will keep our fragmentation below 33% for reasonable malloc/free use-cases
     */

    // Note: rounding to 16 byte alignment means that the caret addresses are 16 byte aligned
    // [prev size (8 bytes) and a free bit][my size (8 bytes) and a free bit][data           ]|[prev size(8 bytes) and a free bit][my size(8 bytes) and a free bit][data          ]
    // ^                                      ^                 ^                                     ^
    // note: data length will be in multiples of 16 bytes

    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    // if size == 0, free
    // if ptr == null, malloc
    // If my current max size is > new size, do nothing
    // if new size is within the next power of 2 down, split into a new free block (shrink this allocated block)
    // if new size is bigger than my current max
    //    look to the right. If free and big enough to accommodate expansion: expand into that block, increase my size just enough and reconfigure the free block to reflect my new size
    //    look to the left. If free and big enough to accommodate expansion: memmove into that block and split the current block such that its just large enough to contain the new size
    // if new size cannot be accommodated above, malloc a new block and free this block

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
    // Probably something like iterating over all blocks and ensuring that:
    // 1. If I'm marked free, there are no free blocks beside me (fully coalesced)
    // 2. if I'm marked free, I'm in the proper free list/wilderness
    // 3. if I'm marked allocated, the blocks to my left and right also say i'm allocated
  return 1;
}
