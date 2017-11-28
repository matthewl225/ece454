/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include <string.h>

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/
#define MAX_SIZE 10000;

char gamelogic_LUT[18] = {
  0, // 0: zero neighbors, and I'm already dead => stay dead
  0, // 1: zero neighbors, and I'm alive => die
  2, // 2: one neighbor, and I'm dead => stay dead, maintain neighbors
  2, // 3: one neighbor, and I'm alive => die, maintain neighbors
  4, // 4: 2 neighbors, and I'm dead => stay dead
  5, // 5: 2 neighbors, and I'm alive => stay alive
  7, // 6: 3 neighbors, and I'm dead => become alive
  7, // 7: 3 neighbors, and I'm alive => stay alive
  8, // 8: 4 neighbors, and I'm dead => stay dead
  8, // 9: 4 neighbors, and I'm alive => die
  10, // 10: 5 neighbors, and I'm dead => stay dead
  10, // 11: 5 neighbors, and I'm alive => die
  12, // 12: 6 neighbors, and I'm dead => stay dead
  12, // 13: 6 neighbors, and I'm alive => die
  14, // 14: 7 neighbors, and I'm dead => stay dead
  14, // 15: 7 neighbors, and I'm alive => die
  16, // 16: 8 neighbors, and I'm dead => stay dead
  16, // 17: 8 neighbors, and I'm alive => die
};

void print_board(char *board, const int nrows, const int ncols) {
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      printf("%d\t", board[i*nrows + j]);
    }
    printf("\n");
  }
}

void unformat_intermediary_board(char *board, const int nrows, const int ncols) {
  for (int row = 0; row < nrows; ++row) {
    for (int col = 0; col < ncols; ++col) {
      board[row * nrows + col] &= 0x1; // only keep lowest bit
    }
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
          neighborCount = (board[nrows_minus_1 * nrows + 0] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows] & 0x1) << 1;
          neighborCount += (board[nrows + 1] & 0x1) << 1;
          neighborCount += (board[nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[1] & 0x1) << 1;
          neighborCount += (board[ncols_minus_1] & 0x1) << 1;
          board[0 * nrows + 0] |= neighborCount;
        } else if (col == ncols_minus_1) {
          // top-right corner
          neighborCount = (board[nrows_minus_1 * nrows + ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 0] & 0x1) << 1;
          neighborCount += (board[ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[0] & 0x1) << 1;
          neighborCount += (board[nrows + ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows] & 0x1) << 1;
          board[0 * nrows + ncols_minus_1] |= neighborCount;

        } else {
          // top row, no corners
          neighborCount = (board[nrows_minus_1 * nrows + col - 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col + 1] & 0x1) << 1;
          neighborCount += (board[nrows + col - 1] & 0x1) << 1;
          neighborCount += (board[nrows + col] & 0x1) << 1;
          neighborCount += (board[nrows + col + 1] & 0x1) << 1;
          neighborCount += (board[col - 1] & 0x1) << 1;
          neighborCount += (board[col + 1] & 0x1) << 1;
          board[col] |= neighborCount;
        }
      } else if (row == nrows_minus_1) {
        // bottom row
        if (col == 0) {
          // bottom left corner
          neighborCount = (board[nrows_minus_2 * nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 0] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 1] & 0x1) << 1;
          neighborCount += (board[ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[0] & 0x1) << 1;
          neighborCount += (board[1] & 0x1) << 1;
          board[nrows_minus_1 * nrows + 0] |= neighborCount;
        } else if (col == ncols_minus_1) {
          // bottom right corner
          neighborCount = (board[nrows_minus_2 * nrows + ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + 0] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + 0] & 0x1) << 1;
          neighborCount += (board[ncols_minus_2] & 0x1) << 1;
          neighborCount += (board[ncols_minus_1] & 0x1) << 1;
          neighborCount += (board[0] & 0x1) << 1;
          board[nrows_minus_1 * nrows + ncols_minus_1] |= neighborCount;
        } else {
          // bottom row, no corners
          neighborCount = (board[nrows_minus_2 * nrows + col - 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + col] & 0x1) << 1;
          neighborCount += (board[nrows_minus_2 * nrows + col + 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col - 1] & 0x1) << 1;
          neighborCount += (board[nrows_minus_1 * nrows + col + 1] & 0x1) << 1;
          neighborCount += (board[col - 1] & 0x1) << 1;
          neighborCount += (board[col] & 0x1) << 1;
          neighborCount += (board[col + 1] & 0x1) << 1;
          board[nrows_minus_1 * nrows + col] |= neighborCount;
        }
      } else if (col == ncols_minus_1) {
        // right most column, no corners
        neighborCount = (board[(row - 1) * nrows + ncols_minus_2] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + ncols_minus_1] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + 0] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_2] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_1] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + 0] & 0x1) << 1;
        neighborCount += (board[row * nrows + ncols_minus_2] & 0x1) << 1;
        neighborCount += (board[row * nrows + 0] & 0x1) << 1;
        board[row * nrows + ncols_minus_1] |= neighborCount;
      } else if (col == 0) {
        // left most column, no corners
        neighborCount = (board[(row - 1) * nrows + 0] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + 1] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + ncols_minus_1] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + 0] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + 1] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + ncols_minus_1] & 0x1) << 1;
        neighborCount += (board[row * nrows + 1] & 0x1) << 1;
        neighborCount += (board[row * nrows + ncols_minus_1] & 0x1) << 1;
        board[row * nrows + 0] |= neighborCount;
      } else {
        // inner square
        neighborCount = (board[(row - 1) * nrows + col - 1] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + col] & 0x1) << 1;
        neighborCount += (board[(row - 1) * nrows + col + 1] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + col - 1] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + col] & 0x1) << 1;
        neighborCount += (board[(row + 1) * nrows + col + 1] & 0x1) << 1;
        neighborCount += (board[row * nrows + col - 1] & 0x1) << 1;
        neighborCount += (board[row * nrows + col + 1] & 0x1) << 1;
        board[row * nrows + col] |= neighborCount;
      }
    }
  }
  // print_board(board, nrows, ncols);
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
// TODO write longs (4 chars) to reduce writebacks
char*
game_of_life (char* writeboard, 
        char* readboard,
        const int nrows,
        const int ncols,
        const int gens_max)
{
  const int nrows_minus_1 = nrows - 1;
  const int nrows_minus_2 = nrows - 2;
  const int ncols_minus_1 = ncols - 1;
  // spawn threads here
  // first pass establishes the expected intermediary format
  char *tempboard;
  int row, col, gen;
  char current_state, transition_state;
  format_intermediary_board(readboard, nrows, ncols);
  for (gen = 0; gen < gens_max; ++gen) {
    memset(writeboard, 0, nrows*ncols);
    for (row = 0; row < nrows; ++row) {
      for (col = 0; col < ncols; ++col) {
        current_state = readboard[row * nrows + col];
        #pragma GCC diagnostic ignored "-Wchar-subscripts" // its ok to use a char to index here
        transition_state = gamelogic_LUT[current_state];
        #pragma GCC diagnostic pop
        if (transition_state & 0x1) {
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
              // CHECKED

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
              // CHECKED
                
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
              // CHECKED

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
              // TODO: bottom row
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
          writeboard[row * nrows + col] |= 0x1; // set alive bit
        } else {
          writeboard[row * nrows + col] &= ~0x1; // clear alive bit
        }
      }
      // reset this row in readboard for later use
      // TODO should do this before the barrier for the whole block when multithreading
    }
    // swap read and write boards;
    // print_board(writeboard, nrows, ncols);
    tempboard = writeboard;
    writeboard = readboard;
    readboard = tempboard;
  }
  // last pass
  // readboard always holds the last computed state
  unformat_intermediary_board(readboard, nrows, ncols);
  return readboard;
}
