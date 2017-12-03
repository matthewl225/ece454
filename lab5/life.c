#define _GNU_SOURCE
/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/
#define MAX_SIZE 10000

typedef struct gol_thread_args {
  char *writeboard;
  char *readboard;
  int startingRow;
  int endingRow;
  int ncols;
  int nrows;
  int gens_max;
  int threadid;
  pthread_barrier_t *gen_barrier;
} gol_thread_args_t;
  
#define NUM_THREADS 8

char gamelogic_LUT[18] = {
  0, // 0: zero neighbors, and I'm already dead => stay dead
  0, // 1: zero neighbors, and I'm alive => die
  0, // 2: one neighbor, and I'm dead => stay dead, maintain neighbors
  0, // 3: one neighbor, and I'm alive => die, maintain neighbors
  0, // 4: 2 neighbors, and I'm dead => stay dead
  1, // 5: 2 neighbors, and I'm alive => stay alive
  1, // 6: 3 neighbors, and I'm dead => become alive
  1, // 7: 3 neighbors, and I'm alive => stay alive
  0, // 8: 4 neighbors, and I'm dead => stay dead
  0, // 9: 4 neighbors, and I'm alive => die
  0, // 10: 5 neighbors, and I'm dead => stay dead
  0, // 11: 5 neighbors, and I'm alive => die
  0, // 12: 6 neighbors, and I'm dead => stay dead
  0, // 13: 6 neighbors, and I'm alive => die
  0, // 14: 7 neighbors, and I'm dead => stay dead
  0, // 15: 7 neighbors, and I'm alive => die
  0, // 16: 8 neighbors, and I'm dead => stay dead
  0, // 17: 8 neighbors, and I'm alive => die
};

void print_board(char *board, const int nrows, const int ncols) {
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      printf("%d\t", board[i*nrows + j] & 0x1);
    }
    printf("\n");
  }
}

void raw_print_board(char *board, const int nrows, const int ncols) {
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      printf("%d\t", board[i*nrows + j]);
    }
    printf("\n");
  }
}

void format_intermediary_board(char *board, const int nrows, const int ncols) {
  int row, col;
  const int nrows_minus_1 = nrows - 1;
  const int nrows_minus_2 = nrows - 2;
  const int ncols_minus_1 = ncols - 1;
  const int ncols_minus_2 = ncols - 2;
  int neighborCount = 0;

  for (row = 0; row < nrows; ++row) {
    for (col = 0; col < ncols; ++col) {
      if (row == 0) {
        // top
        if (col == 0) {
          // top-left corner
          neighborCount = (board[nrows_minus_1 * nrows + 0]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows]) << 1;
          neighborCount += (board[nrows + 1]) << 1;
          neighborCount += (board[nrows + ncols_minus_1]) << 1;
          neighborCount += (board[1]) << 1;
          neighborCount += (board[ncols_minus_1]) << 1;
          board[0 * nrows + 0] |= neighborCount;
        } else if (col == ncols_minus_1) {
          // top-right corner
          neighborCount = (board[nrows_minus_1 * nrows + ncols_minus_2]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 0]) << 1;
          neighborCount += (board[ncols_minus_2]) << 1;
          neighborCount += (board[0]) << 1;
          neighborCount += (board[nrows + ncols_minus_2]) << 1;
          neighborCount += (board[nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows]) << 1;
          board[0 * nrows + ncols_minus_1] |= neighborCount;

        } else {
          // top row, no corners
          neighborCount = (board[nrows_minus_1 * nrows + col - 1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col + 1]) << 1;
          neighborCount += (board[nrows + col - 1]) << 1;
          neighborCount += (board[nrows + col]) << 1;
          neighborCount += (board[nrows + col + 1]) << 1;
          neighborCount += (board[col - 1]) << 1;
          neighborCount += (board[col + 1]) << 1;
          board[col] |= neighborCount;
        }
      } else if (row == nrows_minus_1) {
        // bottom row
        if (col == 0) {
          // bottom left corner
          neighborCount = (board[nrows_minus_2 * nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 0]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 1]) << 1;
          neighborCount += (board[ncols_minus_1]) << 1;
          neighborCount += (board[0]) << 1;
          neighborCount += (board[1]) << 1;
          board[nrows_minus_1 * nrows + 0] |= neighborCount;
        } else if (col == ncols_minus_1) {
          // bottom right corner
          neighborCount = (board[nrows_minus_2 * nrows + ncols_minus_2]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + ncols_minus_1]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 0]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_2]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 0]) << 1;
          neighborCount += (board[ncols_minus_2]) << 1;
          neighborCount += (board[ncols_minus_1]) << 1;
          neighborCount += (board[0]) << 1;
          board[nrows_minus_1 * nrows + ncols_minus_1] |= neighborCount;
        } else {
          // bottom row, no corners
          neighborCount = (board[nrows_minus_2 * nrows + col - 1]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + col]) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + col + 1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col - 1]) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col + 1]) << 1;
          neighborCount += (board[col - 1]) << 1;
          neighborCount += (board[col]) << 1;
          neighborCount += (board[col + 1]) << 1;
          board[nrows_minus_1 * nrows + col] |= neighborCount;
        }
      } else if (col == ncols_minus_1) {
        // right most column, no corners
        neighborCount = (board[(row - 1) * nrows + ncols_minus_2]) << 1;
        neighborCount += (board[(row - 1) * nrows + ncols_minus_1]) << 1;
        neighborCount += (board[(row - 1) * nrows + 0]) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_2]) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_1]) << 1;
        neighborCount += (board[(row + 1) * nrows + 0]) << 1;
        neighborCount += (board[row * nrows + ncols_minus_2]) << 1;
        neighborCount += (board[row * nrows + 0]) << 1;
        board[row * nrows + ncols_minus_1] |= neighborCount;
      } else if (col == 0) {
        // left most column, no corners
        neighborCount = (board[(row - 1) * nrows + 0]) << 1;
        neighborCount += (board[(row - 1) * nrows + 1]) << 1;
        neighborCount += (board[(row - 1) * nrows + ncols_minus_1]) << 1;
        neighborCount += (board[(row + 1) * nrows + 0]) << 1;
        neighborCount += (board[(row + 1) * nrows + 1]) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_1]) << 1;
        neighborCount += (board[row * nrows + 1]) << 1;
        neighborCount += (board[row * nrows + ncols_minus_1]) << 1;
        board[row * nrows + 0] |= neighborCount;
      } else {
        // inner square
        neighborCount = (board[(row - 1) * nrows + col - 1]) << 1;
        neighborCount += (board[(row - 1) * nrows + col]) << 1;
        neighborCount += (board[(row - 1) * nrows + col + 1]) << 1;
        neighborCount += (board[(row + 1) * nrows + col - 1]) << 1;
        neighborCount += (board[(row + 1) * nrows + col]) << 1;
        neighborCount += (board[(row + 1) * nrows + col + 1]) << 1;
        neighborCount += (board[row * nrows + col - 1]) << 1;
        neighborCount += (board[row * nrows + col + 1]) << 1;
        board[row * nrows + col] |= neighborCount;
      }
    }
  }
}

void* game_of_life_subproblem(void *input_args)
{
  const gol_thread_args_t *args = (gol_thread_args_t*)input_args;

  const int startingRow = args->startingRow;
  const int startingRow_plus_1 = startingRow + 1;
  const int endingRow = args->endingRow;
  const int nrows = args->nrows;
  const int ncols = args->ncols;
  const int num_my_rows = endingRow - startingRow;
  const int gens_max = args->gens_max;
  const int myid = args->threadid;

  char *writeboard = args->writeboard;
  char *readboard = args->readboard;
  pthread_barrier_t *gen_barrier = args->gen_barrier;

  // pin this thread
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(myid, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);


  const int nrows_minus_1 = nrows - 1;
  const int nrows_minus_2 = nrows - 2;
  const int ncols_minus_1 = ncols - 1;

  int gen, row, col;
  char current_state, transition_state;
  char *tempboard = NULL;
  for (gen = 0; gen < gens_max; ++gen) {
    // printf("%d: Memset'ing %p + %d = %p\n", myid, writeboard, startingRow*ncols, writeboard + startingRow*ncols);
    memset(writeboard + startingRow*ncols, 0, num_my_rows*ncols);
    pthread_barrier_wait(gen_barrier); // make sure the whole writeboard is zero'd before continuing
    for (row = startingRow; row < endingRow; ++row) {
      for (col = 0; col < ncols; ++col) {
        current_state = readboard[row * nrows + col];
        #pragma GCC diagnostic ignored "-Wchar-subscripts" // its ok to use a char to index here
        transition_state = gamelogic_LUT[current_state];
        #pragma GCC diagnostic pop
        if (transition_state) {
          if (row == 0) {
            if (col == 0) {
              // top left corner
              writeboard[nrows_minus_1 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_1 * nrows + 0] += 2;
              writeboard[nrows_minus_1 * nrows + 1] += 2;
              writeboard[0 * nrows + ncols_minus_1] += 2;
              writeboard[0 * nrows + 1] += 2;
              writeboard[1 * nrows + ncols_minus_1] += 2;
              writeboard[1 * nrows + 0] += 2;
              writeboard[1 * nrows + 1] += 2;
            } else if (col == ncols_minus_1) {
              // top right corner
              writeboard[nrows_minus_1 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_1 * nrows + 0] += 2;
              writeboard[0 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[0 * nrows + 0] += 2;
              writeboard[1 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[1 * nrows + ncols_minus_1] += 2;
              writeboard[1 * nrows + 0] += 2;

            } else {
              // top row
              writeboard[nrows_minus_1 * nrows + col-1] += 2;
              writeboard[nrows_minus_1 * nrows + col] += 2;
              writeboard[nrows_minus_1 * nrows + col+1] += 2;
              writeboard[0 * nrows + col-1] += 2;
              writeboard[0 * nrows + col+1] += 2;
              writeboard[1 * nrows + col-1] += 2;
              writeboard[1 * nrows + col] += 2;
              writeboard[1 * nrows + col+1] += 2;
            }
          } else if (row == nrows_minus_1) {
            if (col == 0) {
              writeboard[nrows_minus_2 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_2 * nrows + 0] += 2;
              writeboard[nrows_minus_2 * nrows + 1] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_1 * nrows + 1] += 2;
              writeboard[0 * nrows + ncols_minus_1] += 2;
              writeboard[0 * nrows + 0] += 2;
              writeboard[0 * nrows + 1] += 2;
            } else if (col == ncols_minus_1) {
              writeboard[nrows_minus_2 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_2 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_2 * nrows + 0] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_1 * nrows + 0] += 2;
              writeboard[0 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[0 * nrows + ncols_minus_1] += 2;
              writeboard[0 * nrows + 0] += 2;
            } else {
              writeboard[nrows_minus_2 * nrows + col - 1] += 2;
              writeboard[nrows_minus_2 * nrows + col] += 2;
              writeboard[nrows_minus_2 * nrows + col + 1] += 2;
              writeboard[nrows_minus_1 * nrows + col - 1] += 2;
              writeboard[nrows_minus_1 * nrows + col + 1] += 2;
              writeboard[0 * nrows + col - 1] += 2;
              writeboard[0 * nrows + col] += 2;
              writeboard[0 * nrows + col + 1] += 2;
            }
          } else if (col == 0) {
            // left side, no corners
            writeboard[(row-1) * nrows + ncols_minus_1] += 2;
            writeboard[(row-1) * nrows + 0] += 2;
            writeboard[(row-1) * nrows + 1] += 2;
            writeboard[row * nrows + ncols_minus_1] += 2;
            writeboard[row * nrows + 1] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1] += 2;
            writeboard[(row+1) * nrows + 0] += 2;
            writeboard[(row+1) * nrows + 1] += 2;
          } else if (col == ncols_minus_1) {
            // right side, no corners
            writeboard[(row-1) * nrows + ncols_minus_1-1] += 2;
            writeboard[(row-1) * nrows + ncols_minus_1] += 2;
            writeboard[(row-1) * nrows + 0] += 2;
            writeboard[row * nrows + ncols_minus_1-1] += 2;
            writeboard[row * nrows + 0] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1-1] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1] += 2;
            writeboard[(row+1) * nrows + 0] += 2;
          } else {
            // inner
            writeboard[(row-1) * nrows + col-1] += 2;
            writeboard[(row-1) * nrows + col] += 2;
            writeboard[(row-1) * nrows + col+1] += 2;
            writeboard[row * nrows + col-1] += 2;
            writeboard[row * nrows + col+1] += 2;
            writeboard[(row+1) * nrows + col-1] += 2;
            writeboard[(row+1) * nrows + col] += 2;
            writeboard[(row+1) * nrows + col+1] += 2;
          }
          writeboard[row * nrows + col] |= 0x1;
        }
      }
      // everyone must complete their first two rows before continuing to avoid conflicting adds
      if (row == startingRow_plus_1) {
        pthread_barrier_wait(gen_barrier);
      }
    }
    tempboard = writeboard;
    writeboard = readboard;
    readboard = tempboard;
    pthread_barrier_wait(gen_barrier);
  }
  return NULL;
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* writeboard, 
        char* readboard,
        const int nrows,
        const int ncols,
        const int gens_max)
{
  // spawn threads here
  // first pass establishes the expected intermediary format
  pthread_t threads[NUM_THREADS-1];
  gol_thread_args_t args[NUM_THREADS];
  pthread_barrier_t gen_barrier;
  pthread_barrier_init(&gen_barrier, NULL, NUM_THREADS);
  format_intermediary_board(readboard, nrows, ncols);
  int i = 0;
  args[i].writeboard = writeboard;
  args[i].readboard = readboard;
  args[i].startingRow = i * nrows/NUM_THREADS;
  args[i].endingRow = (i+1) * nrows/NUM_THREADS;
  args[i].ncols = ncols;
  args[i].nrows = nrows;
  args[i].gens_max = gens_max;
  args[i].threadid = i;
  args[i].gen_barrier = &gen_barrier;
  for (i = 1; i < NUM_THREADS; ++i) {
    args[i].writeboard = writeboard;
    args[i].readboard = readboard;
    args[i].startingRow = i * nrows/NUM_THREADS;
    args[i].endingRow = (i+1) * nrows/NUM_THREADS;
    args[i].ncols = ncols;
    args[i].nrows = nrows;
    args[i].gens_max = gens_max;
    args[i].threadid = i;
    args[i].gen_barrier = &gen_barrier;
    pthread_create(&threads[i-1], NULL, game_of_life_subproblem, &args[i]);
  }
  game_of_life_subproblem(&args[0]); // use this thread as the first thread
  for (i = 1; i < NUM_THREADS; ++i) {
    pthread_join(threads[i-1], NULL);
  }
  char *outboard = gens_max % 2 ? writeboard : readboard;
  return outboard;
}
