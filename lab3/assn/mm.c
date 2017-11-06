/*
 * This allocator uses the segregated free list strategy with
 * buckets of Fibonacci sizes, starting at 32 and 48 bytes. The last
 * bucket is a catch-all which contains all sizes which are too big to fit into
 * one of the smaller buckets.
 *
 * Allocated blocks contain an 8 byte header and an 8 byte footer,
 * and are 16 byte aligned. The contents of each allocated block are as follows:
 * bit 63          bit 1               bit 0
 *   _____________________________________
 *   | size (63 bits) | allocated? (1bit)|
 *   | Payload                           |
 *   | Payload                           |
 *   | Payload                           |
 *   | size (63 bits) | allocated? (1bit)|
 *   -------------------------------------
 *
 * Free blocks contain the same header and footer as in an allocated block, 
 * except part of the payload is used to implement doubly-linked free lists.
 * Therefore, the contents of each free block are as follows:
 *
 * bit 63          bit 1               bit 0
 *   _____________________________________
 *   | size (63 bits) | allocated? (1bit)|
 *   | Next free block pointer           |
 *   | Previous free block pointer       |
 *   | Unused (multiple rows)            |
 *   | size (63 bits) | allocated? (1bit)|
 *   -------------------------------------
 * Note that we use a helper type (linked_list_t) to simplify accesses to the
 * next and previous pointers.
 *
 * Allocations are fulfilled by traversing the free lists in order to find an
 * unallocated block which can fit the request. If the found block is too large
 * to reasonably return to the caller of mm_malloc, the allocator will split the
 * block into two blocks: The first block will be created such that it is large
 * enough to accommodate the requst and will be returned by mm_malloc, while the 
 * second block will contain the remainder from the original block and will be 
 * inserted into its appropriate free list. If no block can be found, the unallocated 
 * bytes at the end of the heap are expanded using a call to mem_sbrk and returned to 
 * the user.
 * Therefore the expected average runtime of mm_malloc is O(M), where M is the number 
 * of free block at the time of the call. Note that this big-O analysis ignores the 
 * overhead of the mem_sbrk call.
 *
 * This allocator uses immediate free block coalescing and employs 
 * a greedy strategy. Therefore there will be no adjacent free blocks in 
 * the heap at any given time. Since doubly-linked free lists are used throughout
 * the allocator, mm_free runs in constant O(1) time.
 *
 * This allocator provides realloc functionality in several steps:
 *    1. We check if the call to mm_realloc should be interpretted as a mm_free 
 *       or mm_malloc call. These functions are used to fulfill these requests.
 *    2. We check to see if the size of the block is already large enough to 
 *       accomodate the requested size. If this is the case, the original pointer
 *       returned and no further modifications are required.
 *    3. We check to see if the next block in the heap is allocated. If not, we check
 *       if this free block coalesced with the current block is large enough to accomodate 
 *       the request. If so, we coalesce the current block with the next block, possibly
 *       splitting this coalesced block to better fit the request.
 *    4. If enabled by the TRY_REALLOC_LEFT option, we check to see if the previous block
 *       is free and large enough to accomodate the request when coalesced with the current 
 *       block. If so, we coalesce these blocks, move the data from the original pointer to 
 *       its new payload pointer, and split the resulting block to ensure a good fit.
 *    5. If no blocks can be coalesced to accommodate the request, we check if extending the heap
 *       will accommodate the request and do so if possible.
 *    6. If the above steps do not apply, we malloc a new block, copy the old contents to this
 *       this block and free the old block.
 * Note that the allocator will try to provide more than the requested space if we have to
 * proceed past step #2 above. This is done to increase throughput as realloc'd blocks of memory
 * are likely to be realloced again.
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
 * Basic Constants, Macros, and Configuration Symbols
 * You are not required to use these macros but may find them helpful.
*************************************************************************/

// #define DEBUG
// #define PRINT_FREE_LISTS
// #define TRY_REALLOC_LEFT
// #define CHECK_HEAP_CONST

#define WSIZE         sizeof(void *)            /* word size (bytes) */
#define OVERHEAD      WSIZE
#define OVERHEAD_4    OVERHEAD * 4;
#define DSIZE         (2 * WSIZE)            /* doubleword size (bytes) */
#define DSIZE_MINUS_1 DSIZE-1
#define CHUNKSIZE     (1<<8)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) < (y)?(x) :(y))

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

#define FREE_LIST_SIZE 30
void *heap_listp = NULL;
void *heap_epilogue_hdrp = NULL;
void *free_list[FREE_LIST_SIZE];

/* Cast free_list entries to this struct for easier management */
typedef struct linked_list {
    size_t size_alloc;
    struct linked_list *next;
    struct linked_list *prev;
} linked_list_t;

// forward declaration of helper functions
void print_free_lists();
int check_free_list_pointers();
int check_coalesced_matching_frees();
int traverse_heap_total_free();
int check_proper_list();
int mm_check(void);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue. Also initialize all free lists to be empty.
 **********************************************************/
int mm_init(void)
{
    #ifdef DEBUG
    printf("************************MM INIT************************\n");
    #endif
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    heap_epilogue_hdrp = heap_listp + 3*WSIZE;

    PUT(heap_listp, 0);                         // alignment padding
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));   // prologue header
    #ifdef DEBUG
    printf("Prologue header is %p\n", heap_listp + WSIZE);
    #endif
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));   // prologue footer
    #ifdef DEBUG
    printf("Prologue footer is %p\n", heap_listp + 2*WSIZE);
    #endif
    PUT(heap_epilogue_hdrp, PACK(0, 1));    // epilogue header, size = number of bytes
    #ifdef DEBUG
    printf("Epilogue header is %p\n", heap_listp + 3*WSIZE);
    #endif
    heap_listp += DSIZE;
    #ifdef PRINT_FREE_LISTS
    print_free_lists();
    #endif
    // Set all free lists as empty
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        free_list[i] = NULL;
    }

    return 0;
}

/**********************************************************
 * get_bucket_size
 * For a given list index, return its max size
 **********************************************************/
size_t get_bucket_size(size_t list_index, size_t current_size) {
    size_t result;
    switch(list_index) {
    case 0: result = 32; break;
    case 1: result = 48; break;
    case 2: result = 80; break;
    case 3: result = 144; break;
    case 4: result = 208; break;
    case 5: result = 336; break;
    case 6: result = 544; break;
    case 7: result = 880; break;
    case 8: result = 1424; break;
    case 9: result = 2304; break;
    case 10: result = 3728; break;
    case 11: result = 6032; break;
    case 12: result = 9760; break;
    case 13: result = 15792; break;
    case 14: result = 25552; break;
    case 15: result = 41344; break;
    case 16: result = 66896; break;
    case 17: result = 108240; break;
    case 18: result = 175136; break;
    case 19: result = 283376; break;
    case 20: result = 458512; break;
    case 21: result = 741888; break;
    case 22: result = 1200400; break;
    case 23: result = 1942288; break;
    case 24: result = 3142688; break;
    case 25: result = 5084976; break;
    case 26: result = 8227664; break;
    case 27: result = 13312640; break;
    case 28: result = 21540304; break;
    default: result = current_size; break;
    }
    if (result > (current_size + (current_size >> 2))) { // result > current_size + current_size/4 ==> result > 1.25*current_size
        result = (current_size + DSIZE_MINUS_1) & ~(DSIZE_MINUS_1);
        //result = DSIZE * ((current_size + (DSIZE) + (DSIZE-1))/ DSIZE);
    }
    return result;
}

/**********************************************************
 * get_list_index
 * For a given size, return the smallest list index which can contain blocks of that size
 **********************************************************/
size_t get_list_index(size_t size)
{
    size_t result;
    if (size < 15793) {
        if (size < 337) {
            if (size < 49) {
                if (size < 33) {result = 0;}
                else {result = 1;}
            } else {
                if (size < 145) {
                    if (size < 81) {result = 2;}
                    else {result = 3;}
                } else if (size < 209) {result = 4;}
                else {result = 6;}
            }
        } else {
            if (size < 2305) {
                if (size < 881) {
                    if (size < 545) {result = 6;}
                    else {result = 7;}
                } else if (size < 1425) {result = 8;}
                else {result = 9;}
            } else {
                if (size < 6033) {
                    if (size < 3729) {result = 10;}
                    else {result = 11;}
                } else if (size < 9761) {result = 12;}
                else {result = 13;}
            }
        }
    } else {
        if (size < 741889) {
            if (size < 108241) {
                if (size < 41345) {
                    if (size < 25553) {result = 14;}
                    else {result = 15;}
                } else if (size < 66897) {result = 16;}
                else {result = 17;}
            } else {
                if (size < 283377) {
                    if (size < 175137) {result = 18;}
                    else {result = 19;}
                } else if (size < 458513) {result = 20;}
                else {result = 21;}
            }
        } else {
            if (size < 5084977) {
                if (size < 1942289) {
                    if (size < 1200401) {result = 22;}
                    else {result = 23;}
                } else if (size < 3142689) {result = 24;}
                else {result = 25;}
            } else {
                if (size < 13312641) {
                    if (size < 8227665) {result = 26;}
                    else {result = 27;}
                } else if (size < 21540305) {result = 28;}
                else {result = 29;}
            }
        }
    }
    #ifdef DEBUG
    printf("\tFound list index %ld\n", result);
    #endif
    return result;
}



/**********************************************************
 * sorted_list_insert
 * insert the given block into the given list at the correct position
 * Note that the given bp must point to a payload.
 **********************************************************/
void *sorted_list_insert(void *free_list, void *bp, size_t size)
{
    #ifdef DEBUG
    printf("\tInserting bp %p, size %ld into freelist %p\n", bp, size, free_list);
    #endif
    linked_list_t *ll_bp = (linked_list_t*)HDRP(bp);
    // insert at the front if list is empty
    if (free_list == NULL) {
        ll_bp->next = NULL;
        ll_bp->prev = NULL;
        return ll_bp;
    }

    linked_list_t *current = (linked_list_t *)free_list;
    linked_list_t *prev = NULL;
    // look for two nodes where prev->size < size < next->size
    #ifdef DEBUG
    printf("\t1. Current = %p prev = %p ll_bp = %p\n", current, prev, ll_bp);
    #endif
    while (current != NULL && current->size_alloc < size) {
        prev = current;
        current = current->next;
        #ifdef DEBUG
        printf("\tCurrent = %p prev = %p\n", current, prev);
        #endif
    }
    #ifdef DEBUG
    printf("\t2. Current = %p prev = %p ll_bp = %p\n", current, prev, ll_bp);
    #endif
    ll_bp->next = current;
    ll_bp->prev = prev;
    if (current) current->prev = ll_bp; // back pointer
    if (!prev) {
        return ll_bp; // new head of the list
    }
    prev->next = ll_bp;
    return free_list;
}

/**********************************************************
 * sorted_list_insert
 * search the given list and remove the block which has a header
 * equal to the given hdrp_bp pointer
 **********************************************************/
void sorted_list_remove(size_t free_list_index, void *hdrp_bp)
{
    #ifdef DEBUG
    printf("\tRemoving %p from freelist %p\n", hdrp_bp, free_list[free_list_index]);
    #ifdef PRINT_FREE_LISTS
    printf("Before ");
    print_free_lists();
    #endif
    #endif

    linked_list_t *ll_bp = (linked_list_t *)hdrp_bp;
    linked_list_t *prev = ll_bp->prev;
    linked_list_t *next = ll_bp->next;

    if (next) {
        next->prev = prev; // bp is not the last node in the list
    }
    if (prev) {
        prev->next = next; // bp is not the first node in the list
    } else {
        free_list[free_list_index] = next; // bp was the first node
    }
    #ifdef PRINT_FREE_LISTS
    printf("After ");
    print_free_lists();
    #endif
}

/**********************************************************
 * split_block
 * given a block pointer and a required size, split the block
 * into a block which will fit the required size well, and a
 * block containing the remainder bytes which is inserted into the free list
 * The block which will fit the required size well is returned to the caller
 **********************************************************/
void *split_block(void *bp, const size_t adjusted_req_size)
{
    void *hdrp_bp = HDRP(bp);
    char bp_is_allocated = GET_ALLOC(hdrp_bp);
    size_t current_size = GET_SIZE(hdrp_bp);
    size_t current_size_index = get_list_index(current_size);
    size_t remainder_size = current_size - adjusted_req_size;
    size_t remainder_size_index = get_list_index(remainder_size);

    #ifdef DEBUG
    printf("\tIf we split this block into at least 1 block of %ld, we'd have a remainder of %ld(%ld)\n", adjusted_req_size, remainder_size, remainder_size_index);
    #endif
    // well sized. Don't split blocks if we are just going to return
    // one huge block and insert one relatively tiny block into the free list
    // therefore, if the index of the remainder is less than half the current
    // size's index, don't bother splitting
    if (((remainder_size_index << 2) < current_size_index) || (remainder_size < 31)) {
        #ifdef DEBUG
        printf("\t**Too Small, don't split\n");
        #endif
        return bp;
    }

    // We will get a decent sized block from this split, so do it
    PUT(hdrp_bp, PACK(adjusted_req_size, bp_is_allocated));
    PUT(FTRP(bp), PACK(adjusted_req_size, bp_is_allocated));
    // create the new block and set it as free
    void *new_block = NEXT_BLKP(bp);
    PUT(HDRP(new_block), PACK(remainder_size, 0));
    PUT(FTRP(new_block), PACK(remainder_size, 0));
    free_list[remainder_size_index] = sorted_list_insert(free_list[remainder_size_index], new_block, remainder_size);

    #ifdef DEBUG
    printf("\tSplit block of size %ld(idx %ld) into two blocks of size %ld(idx %ld) and %ld(idx %ld)\n", current_size, current_size_index, adjusted_req_size, get_list_index(adjusted_req_size), remainder_size, remainder_size_index);
    #ifdef PRINT_FREE_LISTS
    print_free_lists();
    #endif
    #endif

    return bp;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 * The resulting coalesced block is inserted into the 
 * appropriate free list and is also returned to the caller
 **********************************************************/
void *coalesce(void *bp)
{
    char *prev_blkp = PREV_BLKP(bp);
    char *next_blkp = NEXT_BLKP(bp);

    size_t prev_alloc = GET_ALLOC(FTRP(prev_blkp));
    size_t next_alloc = GET_ALLOC(HDRP(next_blkp));
    size_t size = GET_SIZE(HDRP(bp));
    size_t temp_size, list_index;


    /* Case 1 */
    if (prev_alloc && next_alloc) {
        list_index = get_list_index(size);
        #ifdef DEBUG
        printf("\tNothing to coalesce, prev: %p curr: %p next: %p\n",prev_blkp, bp, next_blkp);
        #endif
        free_list[list_index] = sorted_list_insert(free_list[list_index], bp, size);
        return bp;
    }

    /* Case 2 */
    else if (prev_alloc && !next_alloc) {
        #ifdef DEBUG
        printf("Coalescing right, combining %p and %p\n", bp, next_blkp);
        #endif
        // remove next_blkp from free list
        temp_size = GET_SIZE(HDRP(next_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(next_blkp));

        size += temp_size;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], bp, size);
        return (bp);
    }

    /* Case 3 */
    else if (!prev_alloc && next_alloc) {
        // remove prev_blkp from free list
        #ifdef DEBUG
        printf("Coalescing left, combining %p and %p\n", prev_blkp, bp);
        #endif
        temp_size = GET_SIZE(HDRP(prev_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(prev_blkp));

        // two blocks need 4 tags, 1 block needs 2 tags therefore add 2 tags when coalescing
        size += temp_size;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev_blkp), PACK(size, 0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], prev_blkp, size);
        return (prev_blkp);
    }

    /* Case 4 */
    else {
        // 3 blocks need 6 tags, 1 block need 2 tags therefore add 4 tags to the block size
        #ifdef DEBUG
        printf("Coalescing left and right, combining %p, %p and %p\n", prev_blkp, bp, next_blkp);
        #endif

        // remove the previous block from its free list
        temp_size = GET_SIZE(HDRP(prev_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(prev_blkp));

        // remove the next block from its free list
        temp_size = GET_SIZE(HDRP(next_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(next_blkp));

        // combine the 3 blocks and add it into its free list
        size += GET_SIZE(HDRP(prev_blkp)) + temp_size;
        PUT(HDRP(prev_blkp), PACK(size,0));
        PUT(FTRP(prev_blkp), PACK(size,0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], prev_blkp, size);
        return (prev_blkp);
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t size_16)
{
    #ifdef DEBUG
    printf("\tExtending heap by %ld bytes\n", size_16);
    assert((size_16 % 16) == 0);
    #endif
    char *bp;

    /* Allocate an even number of words to maintain alignments */
    if ( (bp = mem_sbrk(size_16)) == (void *)-1 )
        return NULL;
    #ifdef DEBUG
    printf("\tsbrk'd bp is %p\n", bp);
    #endif


    /* Coalesce left if the previous block was free */
    char *prev_blkp = PREV_BLKP(bp);
    size_t prev_alloc = GET_ALLOC(HDRP(prev_blkp));
    #ifdef DEBUG
    printf("\tPrevious blkp is %p and ", prev_blkp);
    #endif
    if (!prev_alloc) {
        #ifdef DEBUG
        printf("is not allocated\n");
        #endif
        // remove the previous block from the free list
        size_t extra_size = GET_SIZE(HDRP(prev_blkp));
        size_t list_index = get_list_index(extra_size);
        sorted_list_remove(list_index, HDRP(prev_blkp));
        // coalesce left with heap and free'd block
        size_16 += extra_size;
        bp = prev_blkp;
    } else {
        #ifdef DEBUG
        printf("is allocated\n");
        #endif
    }
    /* Initialize free block header/footer */
    #ifdef DEBUG
    printf("\tNew freeblock size is %ld\n", size_16);
    #endif
    PUT(HDRP(bp), PACK(size_16, 0));
    PUT(FTRP(bp), PACK(size_16, 0));
    /* Initialize the epilogue header */
    heap_epilogue_hdrp = HDRP(NEXT_BLKP(bp));
    PUT(heap_epilogue_hdrp, PACK(0, 1));
    #ifdef DEBUG
    printf("\t**New epilogue header is %p\n", HDRP(NEXT_BLKP(bp)));
    #endif
    return bp;
}

/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 * Removes the returned block from the free list
 **********************************************************/
void *find_fit(const size_t fl_index, size_t asize)
{
    #ifdef DEBUG
    printf("\tLooking for asize %ld in free_list %p\n", asize, free_list[fl_index]);
    #endif
    // the free list is sorted by size then by memory address, therefore the first block that fits at least asize is the best fit
    linked_list_t *curr = (linked_list_t *)free_list[fl_index];
    linked_list_t *prev = NULL;
    while (curr != NULL && curr->size_alloc < asize) {
        prev = curr;
        curr = curr->next;
    }
    // returns null if not found / free_list empty
    if (curr != NULL) {
        if (prev == NULL) {
            free_list[fl_index] = curr->next;
        } else {
            prev->next = curr->next;
        }
        if (curr->next) curr->next->prev = prev;
        curr = (linked_list_t*)&curr->next; // cast to supress compiler warning
    }
    return curr;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    #ifdef DEBUG
    printf("Freeing %p\n", bp);
    #endif
    if(bp == NULL){
      return;
    }
    // set the free bit and coalesce, inserting into the appropriate free list
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
    #ifdef PRINT_FREE_LISTS
    print_free_lists();
    #endif
    #ifdef CHECK_HEAP_CONST
    assert(mm_check() != 0);
    #endif
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * First, we search the free lists to see if an existing free block
 * can accommodate the request, splitting a larger block into smaller blocks if necessary
 * If no free list block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    #ifdef DEBUG
    printf("Malloc'ing %ld bytes\n", size);
    #endif
    size_t asize; /* adjusted block size */
    char * bp = NULL;

    /* Ignore spurious requests */
    if (size == 0) {
        #ifdef DEBUG
        printf("\tReturning NULL\n");
        #endif
        return NULL;
    }

    // We need to allocate enough space for our header and footer
    size += (OVERHEAD << 1);
    size_t list_index = get_list_index(size);
    asize = get_bucket_size(list_index, size);
    #ifdef DEBUG
    printf("\tAdjusted to %ld bytes\n", asize);
    #endif
    // Search the free lists for a block which will fit the required size
    for (; list_index < FREE_LIST_SIZE && bp == NULL; ++list_index) {
        if (free_list[list_index])
            bp = find_fit(list_index, asize);
    }

    if (!bp) {
        // extend heap and set found_bp to new block
        size_t free_heap_size = 0;
        size_t heap_chunk_size_alloc = GET(heap_epilogue_hdrp - OVERHEAD);
        #ifdef DEBUG
        printf("\tHeap Chunk FTRP: %p\n", heap_epilogue_hdrp - OVERHEAD);
        #endif
        if (!(heap_chunk_size_alloc & 0x1)) {
            free_heap_size = heap_chunk_size_alloc & (~DSIZE - 1);
        }
        #ifdef DEBUG
        printf("\tCurrent free heap size is %ld\n", free_heap_size);
        #endif
        bp = extend_heap(MAX(asize - free_heap_size, CHUNKSIZE));// we are always going to extend the heap by at least CHUNKSIZE bytes to reduce calls to sbrk
        // if extend fails, return null
        if (!bp) {
            #ifdef DEBUG
            printf("\tReturning NULL\n");
            #endif
            return NULL;
        }
    }
    // split the block if the found block is too large for a reasonable return size
    bp = split_block(bp, asize);
    #ifdef DEBUG
    printf("\tFound bp %p, size %ld\n", bp, GET_SIZE(HDRP(bp)));
    #endif

    // Allocate and set the size of the block and return it to the caller
    asize = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    #ifdef DEBUG
    printf("\tReturning %p\n", bp);
    #endif
    #ifdef CHECK_HEAP_CONST
    assert(mm_check() != 0);
    #endif
    return bp;
}

/**********************************************************
 * mm_realloc
 * Given a pointer and a size, return a pointer to a block of memory which is at least size bytes
 * and contains the contents of the given pointer if valid.
 * This is achieved through several steps:
 *    1. We check if the call to mm_realloc should be interpretted as a mm_free (size = 0)
 *       or mm_malloc (ptr = NULL) call. These functions are used to fulfill these requests.
 *    2. We check to see if the size of the block is already large enough to 
 *       accomodate the requested size. If this is the case, the original pointer
 *       returned and no further modifications are required.
 *    3. We check to see if the next block in the heap is allocated. If not, we check
 *       if this free block coalesced with the current block is large enough to accomodate 
 *       the request. If so, we coalesce the current block with the next block, possibly
 *       splitting this coalesced block to better fit the request.
 *    4. If enabled by the TRY_REALLOC_LEFT option, we check to see if the previous block
 *       is free and large enough to accomodate the request when coalesced with the current 
 *       block. If so, we coalesce these blocks, move the data from the original pointer to 
 *       its new payload pointer, and split the resulting block to ensure a good fit.
 *    5. If no blocks can be coalesced to accommodate the request, we check if extending the heap
 *       will accommodate the request and do so if possible.
 *    6. If none of the above steps apply, mm_malloc a new block, move the old contents to 
 *       this new block and mm_free the old pointer.
 * Note that we try to provide more than enough space to accomodate the request, as blocks are likely
 * to be mm_realloc'd again after the first call to mm_realloc. This is done to improve throughput.
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    #ifdef DEBUG
    printf("Realloc'ing %p to %ld bytes\n", ptr, size);
    #endif

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
        mm_free(ptr);
        return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return (mm_malloc(size));
    }

    void *oldptr = ptr;
    void *newptr = NULL;
    size_t copySize = GET_SIZE(HDRP(oldptr));
    size = (size+OVERHEAD*2 + 0xF) & ~0xF; // multiple of 16

    if (size <= copySize) {
        #ifdef DEBUG
        printf("\tRequired size of %ld already fits in current size %ld\n", size+OVERHEAD*2, copySize);
        #endif
        // TODO: possibly shrink this allocated chunk, no tests do this so maybe lab4?
        return oldptr;
    }
    size_t extra_size_needed = size - copySize;
    void *old_hdrp = HDRP(oldptr);
    void *next_blkp = NEXT_BLKP(oldptr);
    size_t next_size_alloc = GET(HDRP(next_blkp));
    size_t right_size = next_size_alloc & ~(DSIZE_MINUS_1);
    #ifdef TRY_REALLOC_LEFT
    void *prev_blkp = PREV_BLKP(oldptr);
    size_t prev_size_alloc = GET(HDRP(prev_blkp));
    size_t left_size = prev_size_alloc & ~(DSIZE_MINUS_1);
    #endif
    #ifdef DEBUG
    printf("\tNext Block: %p : Size: %ld Allocated: ", next_blkp, next_size_alloc & ~0x1);
    if (GET_ALLOC(HDRP(next_blkp))) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    #ifdef TRY_REALLOC_LEFT
    printf("\tPrev Block: %p : Size: %ld Allocated: ", prev_blkp, prev_size_alloc & ~0x1);
    if (GET_ALLOC(HDRP(prev_blkp))) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    #endif
    #endif
    if (!(next_size_alloc & 0x1)) { // next block isn't allocated
        if (right_size >= extra_size_needed) {
            #ifdef DEBUG
            printf("\tExpanding right\n");
            #endif
            // expand into right block
            sorted_list_remove(get_list_index(right_size), HDRP(next_blkp));
            // PUT(FTRP(next_blkp), PACK(copySize + right_size, 0));
            PUT(old_hdrp, PACK(copySize + right_size, 1));
            // combine then split_block
            // no copy needed
            newptr = split_block(oldptr, size);
            PUT(FTRP(newptr), GET(HDRP(newptr)));
            #ifdef CHECK_HEAP_CONST
            assert(mm_check() != 0);
            #endif
            return newptr;
        } 
        #ifdef TRY_REALLOC_LEFT
        else if (!(prev_size_alloc & 0x1)) { // both right and left are free
            if (left_size >= extra_size_needed) { // just left is enough
                #ifdef DEBUG
                printf("\tExpanding left1\n");
                #endif
                // expand into left block
                sorted_list_remove(get_list_index(left_size), HDRP(prev_blkp));
                // memmove required
                // combine then split_block
                // return prev_blkp;
            } else if ((left_size + right_size) >= extra_size_needed) {
                #ifdef DEBUG
                printf("\tExpanding left and right\n");
                #endif
                // expand into both right and left block
                // combine then split_block
                // memmove required
                // return prev_blkp;
            }
        }
        #endif
    } 
    #ifdef TRY_REALLOC_LEFT
    else if (!(prev_size_alloc & 0x1)) { // prev block isn't allocated but next block is
        if (left_size >= extra_size_needed && extra_size_needed < 33) { // just left is enough, but we also need a decent amount of memory
            #ifdef DEBUG
            printf("\tExpanding left2 for %ld bytes\n", extra_size_needed);
            #endif
            // expand into left block
            sorted_list_remove(get_list_index(left_size), HDRP(prev_blkp));
            PUT(HDRP(prev_blkp), PACK(left_size + copySize, 1));
            memmove(prev_blkp, oldptr, copySize);
            newptr = split_block(prev_blkp, size);
            PUT(FTRP(newptr), GET(HDRP(newptr)));
            #ifdef DEBUG
            printf("\tReturning %p, size %ld and %ld\n", newptr, GET(HDRP(newptr)), GET(FTRP(newptr)));
            #endif
            #ifdef CHECK_HEAP_CONST
            assert(mm_check() != 0);
            #endif
            return newptr;
            // combine then split_block
            // memmove required
            // return prev_blkp;
        }
    }
    #endif

    if (HDRP(next_blkp) == heap_epilogue_hdrp) {
        if (extend_heap(extra_size_needed)) {
            // we can now expand right
            #ifdef DEBUG
            printf("\tExtended heap to accommodate realloc\n");
            #endif
            PUT(old_hdrp, PACK(size, 1));
            PUT(FTRP(oldptr), PACK(size, 1));
            return oldptr;
        }
    }

    
    // left and right dont provide enough space to accommodate request
    // malloc a bigger block and free this block
        
    copySize -= OVERHEAD; // don't need to copy the footer block

    newptr = mm_malloc(size + (size >> 3)); // allocate more than enough. Reallocing once implies it'll happen again
    if (newptr == NULL) {
        return NULL;
    }
    #ifdef DEBUG
    printf("\tCannot expand, malloc'd %p size %ld, free'd %p\n", newptr, GET_SIZE(HDRP(newptr)), oldptr);

    printf("\tCopying %p to %p, %ld bytes\n", oldptr, newptr, copySize);
    #endif
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistent.
 *********************************************************/
int mm_check(void){
    // Probably something like iterating over all blocks and ensuring that:
    // 1. Ensure my free list pointers are all accurate and point forwards and backwards
    // 2. If I'm marked free, there are no free blocks beside me (fully coalesced)
    // 3. if I'm marked free, I'm in the proper free list
    // 4. Ensure the number of blocks marked free in the heap is consistent with the number of blocks in the free list
    return (check_coalesced_matching_frees() && check_proper_list() && check_free_list_pointers());
}


/**********************************************************
 * print_free_lists
 * Print the contents of all free lists tracked by
 * the free_list pointer array
 *********************************************************/
void print_free_lists() {
    printf("Free Lists: \n");
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        printf("\t[%d] ", i);
        linked_list_t *curr = free_list[i];
        while (curr != NULL) {
            printf("%p(%ld) <-> ", curr, curr->size_alloc);
            curr = curr->next;
        }
        printf("NULL\n");
    }
}

/**********************************************************
 * check_free_list_pointers
 * Traverses all free lists and ensures that the next and 
 * pointers of all list elements are consistent with their
 * predecessors and successors
 * Returns 1 if consistent, 0 otherwise.
 *********************************************************/
int check_free_list_pointers() {
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        linked_list_t *curr = free_list[i];
        linked_list_t *prev = NULL;
        while (curr != NULL) {
            if (curr->prev != prev) { return 0; }
            if ((curr->size_alloc & 0x1) != 0) { return 0;}
            prev = curr;
            curr = curr->next;
        }
    }
    return 1;
}

/*********************************************************
 * check_coalesced_matching_frees
 * Traverses all the free lists and checks the blocks
 * before and after the free block in the heap is not free
 * Also keeps count of the number of free blocks in the
 * lists and compares it to the number of blocks marked
 * as free in the hea p
 * Returns 1 if consistent, 0 otherwise
 ********************************************************/
int check_coalesced_matching_frees() {
    int total_free_list = 0;
    void *prev_blk, *next_blk;
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        linked_list_t *curr = free_list[i];
        while (curr != NULL) {
            // increment the number of free blocks in the list
            if (curr->size_alloc & 0x1) { total_free_list++; }
            // check previous block in heap
            prev_blk = PREV_BLKP(&(curr->next));
            if (!GET_ALLOC(HDRP(prev_blk))) {
                #ifdef DEBUG
                printf("Error: previous block is free and should have been coalesced\n");
                #endif
                return 0;
            }
            // check next block in heap
            next_blk = NEXT_BLKP(&(curr->next));
            if (!GET_ALLOC(HDRP(next_blk))) {
                #ifdef DEBUG
                printf("Error: next block is free and should have been coalesced\n");
                #endif
                return 0;
            }

            curr = curr->next;
        }
    }

    // check to see if the number of free blocks in list
    // is correctly marked in the heap as unallocated
    if (total_free_list != traverse_heap_total_free()) {
        #ifdef DEBUG
        printf("Error: number of free blocks in heap is different from number of blocks in free list\n");
        #endif
        return 0;
    }
    return 1;
}


/*********************************************************
 * check_proper_list
 * Traverses all the free lists and checks the blocks
 * have the correct free list index
 * Returns 1 if correct, 0 otherwise
 ********************************************************/
int check_proper_list() {
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        linked_list_t *curr = free_list[i];
        while (curr != NULL) {
            if (get_list_index(curr->size_alloc) != i) {
                #ifdef DEBUG
                printf("Error: free block is not in correct list index\n");
                #endif
                return 0;
            }
            curr = curr->next;
        }
    }
    return 1;
}

/*********************************************************
 * traverse_heap_total_free
 * go thru the full heap
 * for each block, check if header is equal to footer
 * return 0 if any header isn't equal to the footer
 * for free blocks, increment total_free_heap by 1
 * returns the total number of free blocks in the heap
 ********************************************************/
int traverse_heap_total_free() {
    void *p = heap_listp;
    int total_free_heap = 0;
    while(p != NULL) {
        if (GET(HDRP(p)) != GET(FTRP(p))) {
        #ifdef DEBUG
        printf("Error: information in header is different from information in footer\n");
        #endif
        return 0;
        }
        if (!GET_ALLOC(FTRP(p))) { total_free_heap++; }
        p = (void *)NEXT_BLKP(p);
    }
    return total_free_heap;
}
