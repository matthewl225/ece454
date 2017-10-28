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

// #define DEBUG
// #define PRINT_FREE_LISTS

#define WSIZE         sizeof(void *)            /* word size (bytes) */
#define OVERHEAD      WSIZE
#define OVERHEAD_4    OVERHEAD * 4;
#define DSIZE         (2 * WSIZE)            /* doubleword size (bytes) */
#define DSIZE_MINUS_1 DSIZE-1
#define CHUNKSIZE     (1<<8)      /* initial heap size (bytes) */

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

#define FREE_LIST_SIZE 30
void *heap_listp = NULL;
void *heap_epilogue_hdrp = NULL;
void *free_list[FREE_LIST_SIZE];

// smallest block size is a DWORD of data and a DWORD of header/footer
// we don't need the footer for this
typedef struct linked_list {
    size_t size_alloc;
    struct linked_list *next;
    struct linked_list *prev;
} linked_list_t;


/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
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
    case 3: result = 128; break;
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
        // TODO should set this to adjust to nearest bucket size, guaranteed multiple of 16
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
                if (size < 129) {
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
    /*
    if (size <= 16) {
        result = 0;
    } else if (size <= 32) {
        result = 1;
    } else if (size <= 48) {
        result = 2;
    } else if (size <= 80) {
        result = 3;
    } else if (size <= 128) {
        result = 4;
    } else if (size <= 208) {
        result = 5;
    } else if (size <= 336) {
        result = 6;
    } else if (size <= 544) {
        result = 7;
    } else if (size <= 880) {
        result = 8;
    } else if (size <= 1424) {
        result = 9;
    } else if (size <= 2304) {
        result = 10;
    } else if (size <= 3728) {
        result = 11;
    } else if (size <= 6032) {
        result = 12;
    } else if (size <= 9760) {
        result = 13;
    } else if (size <= 15792) {
        result = 14;
    } else if (size <= 25552) {
        result = 15;
    } else if (size <= 41344) {
        result = 16;
    } else if (size <= 66896) {
        result = 17;
    } else if (size <= 108240) {
        result = 18;
    } else if (size <= 175136) {
        result = 19;
    } else if (size <= 283376) {
        result = 20;
    } else if (size <= 458512) {
        result = 21;
    } else if (size <= 741888) {
        result = 22;
    } else if (size <= 1200400) {
        result = 23;
    } else if (size <= 1942288) {
        result = 24;
    } else if (size <= 3142688) {
        result = 25;
    } else if (size <= 5084976) {
        result = 26;
    } else if (size <= 8227664) {
        result = 27;
    } else if (size <= 13312640) {
        result = 28;
    } else if (size <= 21540304) {
        result = 29;
    } else {
        result = 30; // default, largest bucket
    }
    */
    #ifdef DEBUG
    printf("\tFound list index %ld\n", result);
    #endif
    return result;
}

int check_free_list_pointers() {
    for (int i = 0; i < FREE_LIST_SIZE; ++i) {
        linked_list_t *curr = free_list[i];
        linked_list_t *prev = NULL;
        while (curr != NULL) {
            if (curr->prev != prev) { return 0; }
            prev = curr;
            curr = curr->next;
        }
    }
    return 1;
}

void print_free_lists() {
    #ifdef PRINT_FREE_LISTS
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
    #endif
}

/**********************************************************
 * sorted_list_insert
 * insert the given block into the given list at the correct position
 **********************************************************/
// TODO this is our biggest performance killer right now
// we should probably replace this with a BST, or relax our sorting requirements
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
 * search the given list and remove bp
 **********************************************************/
// TODO make the nodes doubly linked, so a remove can occur in O(1)
void sorted_list_remove(size_t free_list_index, void *hdrp_bp)
{
    #ifdef DEBUG
    printf("\tRemoving %p from freelist %p\n", hdrp_bp, free_list[free_list_index]);
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
}

/**********************************************************
 * split_block
 * given a block pointer and a required size, split the block
 * into a block which will fit the required size well, and a
 * block containing the remainder bytes which is inserted into the free list
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
    print_free_lists();
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
        list_index = get_list_index(size);
        #ifdef DEBUG
        printf("\tNothing to coalesce, prev: %p curr: %p next: %p\n",prev_blkp, bp, next_blkp);
        #endif
        free_list[list_index] = sorted_list_insert(free_list[list_index], bp, size);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
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

    else if (!prev_alloc && next_alloc) { /* Case 3 */
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

    // TODO broken
    else {
        /* Case 4 */
        // 3 blocks need 6 tags, 1 block need 2 tags therefore add 4 tags to the block size
        #ifdef DEBUG
        printf("Coalescing left and right, combining %p, %p and %p\n", prev_blkp, bp, next_blkp);
        #endif
        temp_size = GET_SIZE(HDRP(prev_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(prev_blkp));

        temp_size = GET_SIZE(HDRP(next_blkp));
        list_index = get_list_index(temp_size);
        sorted_list_remove(list_index, HDRP(next_blkp));

        size += GET_SIZE(HDRP(prev_blkp)) + temp_size;
        PUT(HDRP(prev_blkp), PACK(size,0));
        PUT(FTRP(prev_blkp), PACK(size,0));
        list_index = get_list_index(size);
        free_list[list_index] = sorted_list_insert(free_list[list_index], prev_blkp, size);
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
    // TODO: set the free bit and coalesce
    // will have to place block into either wilderness or free list
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
    #ifdef DEBUG
    print_free_lists();
    #endif
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
    #ifdef DEBUG
    printf("Malloc'ing %ld bytes\n", size);
    #endif
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp = NULL;

    /* Ignore spurious requests */
    if (size == 0) {
        #ifdef DEBUG
        printf("\tReturning NULL\n");
        #endif
        return NULL;
    }

    size += (OVERHEAD << 1);
    size_t list_index = get_list_index(size);
    asize = get_bucket_size(list_index, size);
    #ifdef DEBUG
    printf("\tAdjusted to %ld bytes\n", asize);
    #endif
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
        extendsize = MAX(asize - free_heap_size, CHUNKSIZE); // we are always going to extend the heap by at least CHUNKSIZE bytes to reduce calls to sbrk
        bp = extend_heap(extendsize);
        // if extend fails, return null
        if (!bp) {
            #ifdef DEBUG
            printf("\tReturning NULL\n");
            #endif
            return NULL;
        }
    }
    bp = split_block(bp, asize);
    #ifdef DEBUG
    printf("\tFound bp %p, size %ld\n", bp, GET_SIZE(HDRP(bp)));
    #endif

    asize = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    #ifdef DEBUG
    printf("\tReturning %p\n", bp);
    #endif
    return bp;
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    #ifdef DEBUG
    printf("Realloc'ing %p to %ld bytes\n", ptr, size);
    #endif
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
    // 1. Ensure my free list pointers are all accurate and point forwards and backwards
    return check_free_list_pointers();
    // 2. If I'm marked free, there are no free blocks beside me (fully coalesced)
    // 3. if I'm marked free, I'm in the proper free list/wilderness
    // 4. if I'm marked allocated, the blocks to my left and right also say i'm allocated
}
