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
    size_t temp_size, list_index;


    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        // remove next_blkp from free list
        temp_size = GET_SIZE(HDRP(next_blkp));
        list_index = get_list_index(temp_size);
        free_list[list_index] = sorted_list_remove(free_list[list_index], HDRP(next_blkp));

        size += temp_size + OVERHEAD_2;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], bp, size);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        // remove prev_blkp from free list
        temp_size = GET_SIZE(HDRP(prev_blkp));
        list_index = get_list_index(temp_size);
        free_list[list_index] = sorted_list_remove(free_list[list_index], HDRP(prev_blkp));

        // two blocks need 4 tags, 1 block needs 2 tags therefore add 2 tags when coalescing
        size += temp_size + OVERHEAD_2;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev_blkp), PACK(size, 0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], prev_blkp, size);
        return (prev_blkp);
    }

    else {            /* Case 4 */
        // 3 blocks need 6 tags, 1 block need 2 tags therefore add 4 tags to the block size
        size += GET_SIZE(HDRP(prev_blkp))  +
            GET_SIZE(FTRP(next_blkp)) + OVERHEAD_4;
        PUT(HDRP(prev_blkp), PACK(size,0));
        PUT(FTRP(prev_blkp), PACK(size,0));
        // TODO: remove next_blkp from free list
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


// smallest block size is a DWORD of data and a DWORD of header/footer
// we don't need the footer for this
typedef struct linked_list {
    size_t size_alloc;
    struct linked_list *next;
} linked_list_t;

/**********************************************************
 * sorted_list_insert
 * insert the given block into the given list at the correct position
 **********************************************************/
void *sorted_list_insert(void *free_list, void *bp, size_t size) {
    linked_list_t *ll_bp = (linked_list_t*)HDRP(bp);
    // insert at the front if list is empty
    if (free_list == NULL) {
        ll_bp->next = NULL;
        return ll_bp;
    }

    linked_list_t *current = (linked_list_t *)free_list;
    linked_list_t *prev = NULL;
    // look for two nodes where prev->size < size < next->size
    // In the case of size ties, we should insert in address order with lowest address first
    while (current != NULL && (current->size_alloc < size || (current->size_alloc == size && current < ll_bp))) {
        prev = current;
        current = current->next;
    }
    prev->next = ll_bp;
    ll_bp->next = current;
    return free_list;
}

void *sorted_list_remove(void *free_list, void *bp) {

    linked_list_t *current = (linked_list_t *)free_list;
    linked_list_t *prev = NULL;
    // remove from front of list
    if (current == bp) {
        return current->next;
    }

    // search for bp in the list
    while (current != bp) {
        prev = current;
        current = current->next;
    }
    // remove it by setting the previous node to point to the next node, skipping current
    prev->next = current->next;
    return free_list;
}

/**********************************************************
 * insert_into_free_list
 * given a free'd block pointer, insert it into its appropriate free list at the correct position
 **********************************************************/
void insert_into_free_list(void *bp) {
    if (bp == mem_heap_lo()) {
        // the last block never goes into a free list
        return;
    }

    size_t size = GET_SIZE(HDRP(bp));
    size_t list_index = get_list_index(size);
    free_list[list_index] = sorted_list_insert(free_list[list_index], bp, size);
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
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        // TODO should set this to adjust to nearest bucket size, guaranteed multiple of 16
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    }

    size_t list_index = get_list_index(asize);
    void *found_bp = NULL;
    for (; list_index < MAX_LIST_INDEX && found_bp == NULL; ++list_index) {
        found_bp = find_fit(free_list[list_index], asize);
    }

    if (!found_bp) {
        // extend heap and set found_bp to new block
        extendsize = MAX(asize, CHUNKSIZE);
        found_bp = extend_heap(extendsize/WSIZE)
        // if extend fails, return null
        if (!found_bp) {
            return NULL;
        }
    }

    PUT(HDRP(found_bp), PACK(asize, 1));
    PUT(FTRP(found_bp), PACK(asize, 1));
    return found_bp
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
