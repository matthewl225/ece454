#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "memlib.h"
#include "malloc.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

name_t myname = {
     /* team name to be displayed on webpage */
     "SBORK MLEM LIB",
     /* Full name of first team member */
     "Connor Smith",
     /* Email address of first team member */
     "connor.smith@mail.utoronto.ca",
     /* Student Number of first team member */
     "1000421411"
     /* Full name of second team member */
     "Fan Guo",
     /* Email address of second team member */
     "cfan.guo@mail.utoronto.ca",
     /* Student Number of second team member */
     "1000626539"
};

/*************************************************************************
 * Basic Constants, Macros, and Configuration Symbols
 * You are not required to use these macros but may find them helpful.
*************************************************************************/

// #define DEBUG
// #define PRINT_FREE_LISTS

#define PTHREAD_MUTEX_SUCCESS 0
#define ONE_HUNDRED_MICROSECONDS_IN_NS 100000
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

/* Debugging Macros */
#ifdef DEBUG
    pthread_mutex_t printf_lock = PTHREAD_MUTEX_INITIALIZER;
    #define DEBUG_PRINTF(...) { \
        pthread_mutex_lock(&printf_lock); \
        printf("Thread %d: ", pthread_self()); \
        printf(__VA_ARGS__); \
        pthread_mutex_unlock(&printf_lock); \
    }
    #define DEBUG_ASSERT(x) assert(x)
    #ifdef PRINT_FREE_LISTS
        #define DEBUG_PRINT_FREE_LISTS() print_free_lists()
    #else
        #define DEBUG_PRINT_FREE_LISTS() print_free_lists()
    #endif
#else
    #define DEBUG_PRINTF(...)
    #define DEBUG_ASSERT(x)
    #define DEBUG_PRINT_FREE_LISTS()
#endif

#define FREE_LIST_SIZE 30
#define INITIAL_HEAP_SIZE 512
#define MIN_EXTEND_SIZE 64 // cache line size to avoid false sharing
void *heap_listp = NULL;
size_t global_heap_bytes = 0;
pthread_mutex_t global_heap_lock = PTHREAD_MUTEX_INITIALIZER;
__thread bool free_list_init = false;
__thread void *free_list[FREE_LIST_SIZE];

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
 * get_bucket_size
 * For a given list index, return its max size
 **********************************************************/
size_t get_bucket_size(size_t list_index, size_t current_size) {
    size_t result;
    switch(list_index) {
    // TODO make the min size 64 bytes
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
    // TODO make the min size 64 bytes
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
    DEBUG_PRINTF("\tFound list index %ld\n", result);
    return result;
}

void *global_heap_extend(size_t num_bytes) {
    pthread_mutex_lock(&global_heap_lock);
    // printf("Extending Heap by %ld, current p = %p\n", num_bytes, heap_listp);
    void *retval = NULL;
    void *bp = NULL;
    if (num_bytes < global_heap_bytes) {
        global_heap_bytes -= num_bytes;
        retval = heap_listp + OVERHEAD;
        heap_listp += num_bytes;
    } else if ((bp = mem_sbrk(num_bytes - global_heap_bytes)) != (void *)-1) { // TODO extend by at least MIN_SIZE every time
        global_heap_bytes = 0; // TODO: adjust this number to != 0 when increasing by MIN_SIZE
        retval = heap_listp + OVERHEAD;
        heap_listp += num_bytes;
    } else {
        DEBUG_PRINTF("ERROR: Could not sbrk!\n");
        return NULL;
    }
    // printf("Returning %p\n", retval + OVERHEAD);
    pthread_mutex_unlock(&global_heap_lock);
    PUT(HDRP(retval), PACK(num_bytes, 0));
    PUT(FTRP(retval), PACK(num_bytes, 0));
    return retval;
}

/**********************************************************
 * sorted_list_insert_unsafe
 * insert the given block into the given list at the correct position
 * Note that the given bp must point to a payload.
 **********************************************************/
void *sorted_list_insert_unsafe(void *free_list, void *bp, size_t size)
{
    DEBUG_PRINTF("\tInserting bp %p, size %ld into freelist %p\n", bp, size, free_list);
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
    DEBUG_PRINTF("\t1. Current = %p prev = %p ll_bp = %p\n", current, prev, ll_bp);
    while (current != NULL && current->size_alloc < size) {
        prev = current;
        current = current->next;
        DEBUG_PRINTF("\tCurrent = %p prev = %p\n", current, prev);
    }
    DEBUG_PRINTF("\t2. Current = %p prev = %p ll_bp = %p\n", current, prev, ll_bp);
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
 * sorted_list_insert_unsafe
 * search the given list and remove the block which has a header
 * equal to the given hdrp_bp pointer
 **********************************************************/
void sorted_list_remove_unsafe(size_t free_list_index, void *hdrp_bp)
{
    DEBUG_PRINTF("\tRemoving %p from freelist %p\n", hdrp_bp, free_list[free_list_index]);
    #ifdef PRINT_FREE_LISTS
    DEBUG_PRINTF("Before ");
    DEBUG_PRINT_FREE_LISTS();
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
    DEBUG_PRINTF("After ");
    DEBUG_PRINT_FREE_LISTS();
    #endif
}

// wait at most 100uS for a lock
struct timespec default_lock_timeout;
/**********************************************************
 * split_block_unsafe
 * given a block pointer and a required size, split the block
 * into a block which will fit the required size well, and a
 * block containing the remainder bytes which is inserted into the free list
 * The block which will fit the required size well is returned to the caller
 **********************************************************/
void *split_block_unsafe(void *bp, const size_t adjusted_req_size)
{
    void *hdrp_bp = HDRP(bp);
    char bp_is_allocated = GET_ALLOC(hdrp_bp);
    size_t current_size = GET_SIZE(hdrp_bp);
    size_t current_size_index = get_list_index(current_size);
    size_t remainder_size = current_size - adjusted_req_size;
    size_t remainder_size_index = get_list_index(remainder_size);

    DEBUG_PRINTF("\tIf we split this block into at least 1 block of %ld, we'd have a remainder of %ld(%ld)\n", adjusted_req_size, remainder_size, remainder_size_index);
    // well sized. Don't split blocks if we are just going to return
    // one huge block and insert one relatively tiny block into the free list
    // therefore, if the index of the remainder is less than half the current
    // size's index, don't bother splitting
    if (((remainder_size_index << 2) < current_size_index) || (remainder_size < 31)) {
        DEBUG_PRINTF("\t**Too Small, don't split\n");
        return bp;
    }

    // We will get a decent sized block from this split, so do it
    PUT(hdrp_bp, PACK(adjusted_req_size, bp_is_allocated));
    PUT(FTRP(bp), PACK(adjusted_req_size, bp_is_allocated));
    // create the new block and set it as free
    void *new_block = NEXT_BLKP(bp);
    PUT(HDRP(new_block), PACK(remainder_size, 0));
    PUT(FTRP(new_block), PACK(remainder_size, 0));
    free_list[remainder_size_index] = sorted_list_insert_unsafe(free_list[remainder_size_index], new_block, remainder_size);

    DEBUG_PRINTF("\tSplit block of size %ld(idx %ld) into two blocks of size %ld(idx %ld) and %ld(idx %ld)\n", current_size, current_size_index, adjusted_req_size, get_list_index(adjusted_req_size), remainder_size, remainder_size_index);
    DEBUG_PRINT_FREE_LISTS();

    return bp;
}

/**********************************************************
 * find_fit_unsafe
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 * Removes the returned block from the free list
 **********************************************************/
void *find_fit_unsafe(const size_t fl_index, size_t asize)
{
    DEBUG_PRINTF("\tLooking for asize %ld in free_list %p\n", asize, free_list[fl_index]);
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
 * my_malloc
 * Allocate a block of size bytes.
 * First, we search the free lists to see if an existing free block
 * can accommodate the request, splitting a larger block into smaller blocks if necessary
 * If no free list block satisfies the request, the heap is extended
 **********************************************************/
void *my_malloc(size_t size)
{
    DEBUG_PRINTF("Malloc'ing %ld bytes\n", size);
    size_t asize; /* adjusted block size */
    pthread_mutex_t *current_lock = NULL;
    char * bp = NULL;

    /* Ignore spurious requests */
    if (size == 0) {
        DEBUG_PRINTF("\tReturning NULL\n");
        return NULL;
    }

    // We need to allocate enough space for our header and footer
    size += (OVERHEAD << 1);
    size_t list_index = get_list_index(size);
    asize = get_bucket_size(list_index, size);
    DEBUG_PRINTF("\tAdjusted to %ld bytes\n", asize);
    // Search the free lists for a block which will fit the required size
    for (; list_index < FREE_LIST_SIZE && bp == NULL; ++list_index) {
        if (free_list[list_index]) {
            bp = find_fit_unsafe(list_index, asize);
        }
    }

    if (!bp) {
        bp = global_heap_extend(MAX(asize, MIN_EXTEND_SIZE));
        if (!bp) {
            DEBUG_PRINTF("\tReturning NULL\n");
            return NULL;
        }
    }

    asize = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = split_block_unsafe(bp, asize);

    DEBUG_PRINTF("\tFound bp %p, size %ld\n", bp, GET_SIZE(HDRP(bp)));
    DEBUG_ASSERT(mm_check() != 0);
    return bp;
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue. Also initialize all free lists to be empty.
 **********************************************************/
int mm_init(void)
{
    DEBUG_PRINTF("************************MM INIT************************\n");
    mem_init();
    if ((heap_listp = mem_sbrk(INITIAL_HEAP_SIZE)) == NULL) {
        return -1;
    }
    global_heap_bytes = INITIAL_HEAP_SIZE;
    return 0;
}

void * mm_malloc(size_t sz) {
    if (!free_list_init) {
        // Set all free lists as empty
        free_list_init = true;
        for (int i = 0; i < FREE_LIST_SIZE; ++i) {
            free_list[i] = NULL;
        }
    }
    return my_malloc(sz);
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp) {
    DEBUG_PRINTF("Freeing %p\n", bp);
    if(bp == NULL){
      return;
    }
    if (!free_list_init) {
        // Set all free lists as empty
        free_list_init = true;
        for (int i = 0; i < FREE_LIST_SIZE; ++i) {
            free_list[i] = NULL;
        }
    }
    // set the free bit and coalesce, inserting into the appropriate free list
    size_t size = GET_SIZE(HDRP(bp));
    size_t index = get_list_index(size);
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    free_list[index] = sorted_list_insert_unsafe(free_list[index], bp, size);
    // coalesce(bp); // TODO fix coalescing
    // TODO could release to a global free list to decrease fragmentation
    DEBUG_PRINT_FREE_LISTS();
    DEBUG_ASSERT(mm_check() != 0);
}
