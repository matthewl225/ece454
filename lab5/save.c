#include "save.h"
#include <assert.h>
#include <stdlib.h>

static void
save_dimensions (FILE* output, const int nrows, const int ncols)
{
  int err = 0;

  err = fprintf (output, "P1\n%d %d\n", nrows, ncols);
  if (err < 0)
    {
      fprintf (stderr, "*** Failed to write board dimensions ***\n");
      fclose (output);
      exit (EXIT_FAILURE);
    }
}

static void
save_board_values (FILE* output, const char board[], const int nrows, const int ncols)
{
  int err = 0;
  int i = 0;

  // Assumption: both nrows and ncols are powers of 2, therefore nrows*ncols must be a multiple of 4
  for (i = 0; i < nrows * ncols; i += 4) {
    /* ASCII '0' is not zero; do the conversion */
    err = fprintf (output,
                   "%c\n%c\n%c\n%c\n",
                   (board[i] & 0x1) + '0',
                   (board[i+1] & 0x1) + '0',
                   (board[i+2] & 0x1) + '0',
                   (board[i+3] & 0x1) + '0');
    if (err < 0) {
      fprintf (stderr, "*** Failed to write board item %d ***\n", i);
	  fclose (output);
	  exit (EXIT_FAILURE);
	}
  }
}


void
save_board (FILE* output, const char board[], const int nrows, const int ncols)
{
  save_dimensions (output, nrows, ncols);
  save_board_values (output, board, nrows, ncols);
}
