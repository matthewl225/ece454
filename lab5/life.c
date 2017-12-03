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
 * Helper function and type definitions
 ****************************************************************************/
#define MAX_SIZE 10000
#define NUM_THREADS 8

// type used to pass arguments for each spawned thread
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

/**********************************************************
 * print_board
 * Print the given board without its neighbor counts
 **********************************************************/
void print_board(char *board, const int nrows, const int ncols) {
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      printf("%d\t", board[i*nrows + j] & 0x1);
    }
    printf("\n");
  }
}

/**********************************************************
 * print_board_with_counts
 * Print the given board along with its neighbor counts
 **********************************************************/
void print_board_with_counts(char *board, const int nrows, const int ncols) {
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      printf("%d\t", board[i*nrows + j]);
    }
    printf("\n");
  }
}

/**********************************************************
 * format_intermediary_board
 * Given a board which specifies whether a cell is alive or dead,
 * format that same board to contain whether the cell is alive/dead
 * and how many neighbors are alive
 **********************************************************/
void format_intermediary_board(char *board, const int nrows, const int ncols) {
  const int nrows_minus_1 = nrows - 1;
  const int ncols_minus_1 = ncols - 1;

  int row, col;
  int right_col, left_col;
  int above_row_offset = (nrows_minus_1 - 1) * ncols;
  int below_row_offset = 0;
  int row_offset = -ncols;
  int neighborCount = 0;

  for (row = 0; row < nrows; ++row) {
    above_row_offset += ncols;
    below_row_offset += ncols;
    row_offset += ncols;
    if (row == 1) {
      above_row_offset = 0;
    } else if (row == nrows_minus_1) {
      below_row_offset = 0;
    }
    left_col = ncols_minus_1 - 1;
    right_col = 0;
    for (col = 0; col < ncols; ++col) {
      ++left_col;
      ++right_col;
      if (col == 1) {
        left_col = 0;
      } else if (col == ncols_minus_1) {
        right_col = 0;
      }
      neighborCount = (board[above_row_offset + left_col] & 0x1) << 1;
      neighborCount += (board[above_row_offset + col] & 0x1) << 1;
      neighborCount += (board[above_row_offset + right_col] & 0x1) << 1;
      neighborCount += (board[below_row_offset + left_col] & 0x1) << 1;
      neighborCount += (board[below_row_offset + col] & 0x1) << 1;
      neighborCount += (board[below_row_offset + right_col] & 0x1) << 1;
      neighborCount += (board[row_offset + left_col] & 0x1) << 1;
      neighborCount += (board[row_offset + right_col] & 0x1) << 1;
      if (neighborCount) board[row_offset + col] |= neighborCount;
    }
  }
}

/**********************************************************
 * game_of_life_subproblem
 * Computes Conway's Game of Life based on input_args
 * input_args must specify the boards, along with the region which
 * this thread should be concerned with
 **********************************************************/
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
              writeboard[ncols_minus_1] += 2;
              writeboard[1] += 2;
              writeboard[nrows + ncols_minus_1] += 2;
              writeboard[nrows] += 2;
              writeboard[nrows + 1] += 2;
            } else if (col == ncols_minus_1) {
              // top right corner
              writeboard[nrows_minus_1 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_1 * nrows + 0] += 2;
              writeboard[ncols_minus_1 - 1] += 2;
              writeboard[0] += 2;
              writeboard[nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows + ncols_minus_1] += 2;
              writeboard[nrows] += 2;

            } else {
              // top row
              writeboard[nrows_minus_1 * nrows + col-1] += 2;
              writeboard[nrows_minus_1 * nrows + col] += 2;
              writeboard[nrows_minus_1 * nrows + col+1] += 2;
              writeboard[col-1] += 2;
              writeboard[col+1] += 2;
              writeboard[nrows + col-1] += 2;
              writeboard[nrows + col] += 2;
              writeboard[nrows + col+1] += 2;
            }
          } else if (row == nrows_minus_1) {
            if (col == 0) {
              writeboard[nrows_minus_2 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_2 * nrows + 0] += 2;
              writeboard[nrows_minus_2 * nrows + 1] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_1 * nrows + 1] += 2;
              writeboard[ncols_minus_1] += 2;
              writeboard[0] += 2;
              writeboard[1] += 2;
            } else if (col == ncols_minus_1) {
              writeboard[nrows_minus_2 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_2 * nrows + ncols_minus_1] += 2;
              writeboard[nrows_minus_2 * nrows + 0] += 2;
              writeboard[nrows_minus_1 * nrows + ncols_minus_1 - 1] += 2;
              writeboard[nrows_minus_1 * nrows + 0] += 2;
              writeboard[ncols_minus_1 - 1] += 2;
              writeboard[ncols_minus_1] += 2;
              writeboard[0] += 2;
            } else {
              writeboard[nrows_minus_2 * nrows + col - 1] += 2;
              writeboard[nrows_minus_2 * nrows + col] += 2;
              writeboard[nrows_minus_2 * nrows + col + 1] += 2;
              writeboard[nrows_minus_1 * nrows + col - 1] += 2;
              writeboard[nrows_minus_1 * nrows + col + 1] += 2;
              writeboard[col - 1] += 2;
              writeboard[col] += 2;
              writeboard[col + 1] += 2;
            }
          } else if (col == 0) {
            // left side, no corners
            writeboard[(row-1) * nrows + ncols_minus_1] += 2;
            writeboard[(row-1) * nrows] += 2;
            writeboard[(row-1) * nrows + 1] += 2;
            writeboard[row * nrows + ncols_minus_1] += 2;
            writeboard[row * nrows + 1] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1] += 2;
            writeboard[(row+1) * nrows] += 2;
            writeboard[(row+1) * nrows + 1] += 2;
          } else if (col == ncols_minus_1) {
            // right side, no corners
            writeboard[(row-1) * nrows + ncols_minus_1-1] += 2;
            writeboard[(row-1) * nrows + ncols_minus_1] += 2;
            writeboard[(row-1) * nrows] += 2;
            writeboard[row * nrows + ncols_minus_1-1] += 2;
            writeboard[row * nrows] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1-1] += 2;
            writeboard[(row+1) * nrows + ncols_minus_1] += 2;
            writeboard[(row+1) * nrows] += 2;
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
 * game_of_life
 * Main entry point. Spawn threads and assign a piece of the world to each of them
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
  // The first thread is this thread
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

  // any addition threads are created and started
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

  // start this thread's work
  game_of_life_subproblem(&args[0]);

  // Wait until all spawned threads are finished
  for (i = 1; i < NUM_THREADS; ++i) {
    pthread_join(threads[i-1], NULL);
  }

  // return the proper output board
  char *outboard = gens_max % 2 ? writeboard : readboard;
  return outboard;
}
