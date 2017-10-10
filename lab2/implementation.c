#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line

// Configuration parameters, uncomment them to turn them on
#define USE_ISWHITEAREA
#define USE_MIRROROPTS
#define USE_INSTRUCTIONCONDENSER
#define USE_TRANSLATEOPTS

/* SENSOR COLLAPSING CODE */

/* to convert chars to movement_type:
 *     ord(char[1]) + ord(char[2])
 */
typedef enum {
    A = 65, // 'A' + '\0'
    D = 68, // 'D' + '\0'
    S = 83, // 'S' + '\0'
    W = 87, // 'W' + '\0'
    CCW = 134, // 'C' * 2
    CW = 154, // 'C' + 'W'
    MX = 165, // 'M' + 'X'
    MY = 166, // 'M' + 'Y'
    FRAME_BREAK = 0,
} movement_type;

#ifdef USE_INSTRUCTIONCONDENSER
typedef struct {
    movement_type type;
    int value;
} optimized_kv;

int insert_mirror_frames(optimized_kv *collapsed_sensor_values, int new_count, bool is_X_mirrored, bool is_Y_mirrored) {
    if (is_X_mirrored) {
        collapsed_sensor_values[new_count].type = MX;
        collapsed_sensor_values[new_count].value = 1;
        ++new_count;
    }

    if (is_Y_mirrored) {
        collapsed_sensor_values[new_count].type = MY;
        collapsed_sensor_values[new_count].value = 1;
        ++new_count;
    }
    return new_count;
}

int insert_rotation_frames(optimized_kv *collapsed_sensor_values, int new_count, int total_clockwise_rotation) {
    // tcr is between -3 and +3
    total_clockwise_rotation = (total_clockwise_rotation % 4);
    // if tcr is 3 or -1, ccw
    // else if tcr != 0, cw
    if (total_clockwise_rotation == 3 || total_clockwise_rotation == -1) { // CCW
        collapsed_sensor_values[new_count].type = CCW;
        collapsed_sensor_values[new_count].value = 1;
        ++new_count;
    } else if (total_clockwise_rotation != 0) { // CW
        collapsed_sensor_values[new_count].type = CW;
        collapsed_sensor_values[new_count].value = (4 + total_clockwise_rotation) % 4;
        /* 1 => 1
            * 2 => 2
            * -2 => 2
            * -3 => 1
            */
        ++new_count;
    }
    return new_count;
}

int insert_translation_frames(optimized_kv *collapsed_sensor_values, int new_count, int total_up_movement, int total_right_movement) {
    if (total_up_movement > 0) {
        collapsed_sensor_values[new_count].type = W;
        collapsed_sensor_values[new_count].value = total_up_movement;
        ++new_count;
    } else if (total_up_movement != 0) { // negative
        collapsed_sensor_values[new_count].type = S;
        collapsed_sensor_values[new_count].value = -total_up_movement;
        ++new_count;
    }

    if (total_right_movement > 0) {
        collapsed_sensor_values[new_count].type = D;
        collapsed_sensor_values[new_count].value = total_right_movement;
        ++new_count;
    } else if (total_right_movement != 0) { // negative
        collapsed_sensor_values[new_count].type = A;
        collapsed_sensor_values[new_count].value = -total_right_movement;
        ++new_count;
    }
    return new_count;
}

optimized_kv* collapse_sensor_values(struct kv *sensor_values, int sensor_values_count, int *new_sensor_value_count) {
    int new_count = 0;
    char *sensor_value_key;

    movement_type type;
    struct kv *sensor_value;
    optimized_kv *collapsed_sensor_values = (optimized_kv*)malloc(sizeof(optimized_kv) * sensor_values_count);
    int i = 0;
    int sensor_value_value;
    int total_up_movement = 0;
    int total_right_movement = 0;
    int total_clockwise_rotation = 0;
    bool is_X_mirrored = false;
    bool is_Y_mirrored = false;
    while (i < sensor_values_count) {
        sensor_value = &sensor_values[i];
        sensor_value_value = sensor_value->value;
        sensor_value_key = sensor_value->key;
        type = sensor_value_key[0] + sensor_value_key[1];
        switch (type) {
        case MX:
            is_X_mirrored = !is_X_mirrored;
            break;
        case MY:
            is_Y_mirrored = !is_Y_mirrored;
            break;
        case CCW:
            sensor_value_value *= -1;
            /* fallthrough */
        case CW:
            if (is_X_mirrored) sensor_value_value *= -1;
            if (is_Y_mirrored) sensor_value_value *= -1;
            total_clockwise_rotation = (total_clockwise_rotation + sensor_value_value) % 4;
            break;
        case S:
            sensor_value_value *= -1;
            /* fallthrough */
        case W:
            if (is_X_mirrored) {
                sensor_value_value *= -1;
            }
            switch (total_clockwise_rotation) {
            case 0:
                total_up_movement += sensor_value_value;
                break;
            case -3:
            case 1:
                total_right_movement -= sensor_value_value;
                break;
            case -2:
            case 2:
                total_up_movement -= sensor_value_value;
                break;
            case -1:
            case 3:
                total_right_movement += sensor_value_value;
                break;
            }
            break;
        case A:
            sensor_value_value *= -1;
            /* fallthrough */
        case D:
            if (is_Y_mirrored) {
                sensor_value_value *= -1;
            }
            switch (total_clockwise_rotation) {
            case 0:
                total_right_movement += sensor_value_value;
                break;
            case -3:
            case 1:
                total_up_movement += sensor_value_value;
                break;
            case -2:
            case 2:
                total_right_movement -= sensor_value_value;
                break;
            case -1:
            case 3:
                total_up_movement -= sensor_value_value;
                break;
            }
            break;
        }
        ++i;
        if (i % 25 == 0) {
            if (is_X_mirrored && is_Y_mirrored) {
                total_clockwise_rotation = (total_clockwise_rotation + 2) % 4; // TODO test if it is always faster to do CW,2 vs MX+MY
                is_X_mirrored = false;                                         // if not, uncomment the block below
                is_Y_mirrored = false;
            }
            /*
            if (is_X_mirrored && is_Y_mirrored && (total_clockwise_rotation == 2 || total_clockwise_rotation == -2)) {
                is_X_mirrored = false;
                is_Y_mirrored = false;
                total_clockwise_rotation = 0;
            }
            */
            new_count = insert_translation_frames(collapsed_sensor_values, new_count, total_up_movement, total_right_movement);
            new_count = insert_rotation_frames(collapsed_sensor_values, new_count, total_clockwise_rotation);
            new_count = insert_mirror_frames(collapsed_sensor_values, new_count, is_X_mirrored, is_Y_mirrored);
            total_clockwise_rotation = 0;
            total_up_movement = 0;
            total_right_movement = 0;
            is_X_mirrored = false;
            is_Y_mirrored = false;
            collapsed_sensor_values[new_count].type = FRAME_BREAK;
            collapsed_sensor_values[new_count++].value = 0;
        }
    }
    *new_sensor_value_count = new_count;
    return collapsed_sensor_values;
}

#endif
/* END SENSOR COLLAPSING CODE */
/* White Pixel Optimization Structures */

/* Use this structure as follows
 *
 * if isWhiteArea[i,j] is true, then the square with corners at (isWhiteAreaStride * i, isWhiteAreaStride * j, isWhiteAreaStride * (i + 1) - 1, isWhiteAreaStride * (j + 1) - 1)
 * contains only white pixels and can be optimized as such
 */

#ifdef USE_ISWHITEAREA
#define isWhiteAreaStride 21 // translates to 64 bytes, each pixel is 3 bytes
bool *isWhiteArea;
unsigned char *temp_square;
unsigned int numFullStridesX;
unsigned int numFullStridesY;
unsigned int middleSquareDimensions;


// TODO check this
void translatePixelToWhiteSpaceArrayIndices(unsigned px_x, unsigned px_y, unsigned px_width, unsigned *ws_x_out, unsigned *ws_y_out) {
    // if px_x is inside the middle column, return the middle column
    const bool hasMiddleSquare = (middleSquareDimensions != 0);

    if (!hasMiddleSquare) {
        *ws_x_out = px_x / isWhiteAreaStride;
        *ws_y_out = px_y / isWhiteAreaStride;
    } else {
        const unsigned numFullStridesX_div_2 = numFullStridesX / 2;
        const unsigned numFullStridesY_div_2 = numFullStridesY / 2;
        int test = px_x - (numFullStridesX_div_2 * isWhiteAreaStride);
        if (test < middleSquareDimensions && test >= 0) {
            // inside the middle
            *ws_x_out = numFullStridesX_div_2;
        } else if (test < 0) {
            // left of the middle
            *ws_x_out = px_x / isWhiteAreaStride;
        } else if (test > middleSquareDimensions) {
            // right of the middle
            *ws_x_out = (numFullStridesX_div_2) + 1 + ((test - middleSquareDimensions) / isWhiteAreaStride);
        }

        test = px_y - (numFullStridesY_div_2 * isWhiteAreaStride);
        if (test < middleSquareDimensions && test >= 0) {
            // inside the middle
            *ws_y_out = numFullStridesY_div_2;
        } else if (test < 0) {
            // above the middle
            *ws_y_out = px_y / isWhiteAreaStride;
        } else if (test >= middleSquareDimensions) {
            // below the middle
            *ws_y_out = (numFullStridesY_div_2) + 1 + ((test - middleSquareDimensions) / isWhiteAreaStride);
        }
    }

}

// returns the top left pixel of the given white space array square
void translateWhiteSpaceArrayIndicesToPixel(unsigned ws_x, unsigned ws_y, unsigned *px_x_out, unsigned *px_y_out) {
    if (middleSquareDimensions != 0 && ws_x > numFullStridesX / 2) {
        *px_x_out = (ws_x - 1) * isWhiteAreaStride + middleSquareDimensions;
    } else {
        *px_x_out = ws_x * isWhiteAreaStride;
    }

    if (middleSquareDimensions != 0 && ws_y > numFullStridesY / 2) {
        *px_y_out = (ws_y - 1) * isWhiteAreaStride + middleSquareDimensions;
    } else {
        *px_y_out = ws_y * isWhiteAreaStride;
    }
}

bool checkWhiteAreaSquare(unsigned char *buffer_frame, unsigned buffer_pxwidth, unsigned whiteAreaCol, unsigned whiteAreaRow, unsigned pxWidth, unsigned pxHeight) {
    unsigned tl_px_x, tl_px_y;
    translateWhiteSpaceArrayIndicesToPixel(whiteAreaCol, whiteAreaRow, &tl_px_x, &tl_px_y);
    unsigned row_offset = tl_px_y * buffer_pxwidth * 3;
    unsigned col_offset = tl_px_x * 3;
    // printf("Checking square cornered at (%d, %d), width: %d height: %d\n", tl_px_x, tl_px_y, pxWidth, pxHeight);
    for (int row = 0; row < pxHeight; ++row) {
        for (int col = 0; col < pxWidth; ++col) {
            int pixel_index = row_offset + col_offset + row * buffer_pxwidth * 3 + col * 3;
            if (buffer_frame[pixel_index] != 255 ||
                buffer_frame[pixel_index + 1] != 255 ||
                buffer_frame[pixel_index + 2] != 255)
            {
                return false;
            }
        }
    }
    return true;
}

void populateIsWhiteArea(unsigned char *buffer_frame, unsigned width, unsigned height) {
    const bool hasMiddleSquare = (middleSquareDimensions != 0);
    unsigned int boolArrayWidth = numFullStridesX + hasMiddleSquare;
    unsigned int boolArrayHeight = numFullStridesY + hasMiddleSquare;

    // 500x500 image = 22 21x21 squares with a 38x38 middle square, 38x21 middle column and 21x38 middle row
    // |11 21-wide squares|38 wide square|11 21-wide squares|

    int whiteAreaRow, whiteAreaCol;
    // upper left corner
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
    }

    // bottom left corner
    for (whiteAreaRow = numFullStridesY / 2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
    }

    // top right corner
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX / 2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
    }

    // bottom right corner
    for (whiteAreaRow = numFullStridesY / 2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX / 2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
    }

    if (hasMiddleSquare) {
        // left middle row
        whiteAreaRow = numFullStridesY / 2;
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // right middle row
        for (whiteAreaCol = numFullStridesX / 2 + 1; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // top center row
        whiteAreaCol = numFullStridesX / 2;
        for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // bottom center row
        whiteAreaCol = numFullStridesX / 2;
        for (whiteAreaRow = numFullStridesY / 2 + 1; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // middle square
        whiteAreaRow = numFullStridesY / 2;
        isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
            checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, middleSquareDimensions);
        // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
    }

    // For debugging: print the white area array
    /*
    printf("\n\nWhiteSpaceArray (%d x %d), middle square is %d x %d:\n", boolArrayWidth, boolArrayHeight, middleSquareDimensions, middleSquareDimensions);
    for (int row = 0; row < boolArrayHeight; row++) {
        for (int col = 0; col < boolArrayWidth; col++) {
            printf("%d", isWhiteArea[row * boolArrayWidth + col]);
        }
        printf("\n");
    }
    */
}

void createIsWhiteArea(unsigned char *buffer_frame, unsigned width, unsigned height) {
    numFullStridesX = (width / isWhiteAreaStride / 2 * 2); // divide into this many 21x21 pixel squares. Must be an even number
    numFullStridesY = (height / isWhiteAreaStride / 2 * 2);
    middleSquareDimensions = width % (isWhiteAreaStride * 2); // remainder of the above square division.
    isWhiteArea = calloc((numFullStridesX + 1) * (numFullStridesY + 1), sizeof(bool));
    unsigned max_square_width = isWhiteAreaStride < middleSquareDimensions ? middleSquareDimensions : isWhiteAreaStride;
    temp_square = (unsigned char*)malloc(max_square_width * max_square_width * sizeof(unsigned char)*3);
    populateIsWhiteArea(buffer_frame, width, height);
}
#endif
/* End White Pixel Optimization Structures */


/* Helper Functions */
void blankSquare(unsigned char *buffer_frame, unsigned buffer_width, unsigned pxx, unsigned pxy, unsigned blank_width, unsigned blank_height) {
    unsigned const blank_width_3 = blank_width * 3;
    unsigned const buffer_width_3 = buffer_width * 3;
    unsigned char *target = buffer_frame + buffer_width_3 * pxy + pxx * 3;
    for (int row = 0; row < blank_height; ++row) {
        // set row to 255,255,255...
        memset(target, 255, blank_width_3);
        // move down a row
        target += buffer_width_3;
    }
}

void moveRectInline(unsigned char* buffer_frame, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned dst_pxx, unsigned dst_pxy, unsigned cpy_width, unsigned cpy_height) {
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned cpy_width_3 = cpy_width * 3;

    unsigned char *src = buffer_frame + buffer_width_3 * src_pxy + src_pxx * 3;
    unsigned char *dst = buffer_frame + buffer_width_3 * dst_pxy + dst_pxx * 3;
    for (int i = 0; i < cpy_height; ++i) {
        // memmove row by row
        // use memmove because src and dst may overlap (e.g. translations)
        memmove(dst, src, cpy_width_3);
        src += buffer_width_3;
        dst += buffer_width_3;
    }
}

// this should only be called with relatively small (20x20) areas
// also src and dst should not overlap
void moveRectInlineRotate90CCW(unsigned char* buffer_frame, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned dst_pxx, unsigned dst_pxy, unsigned cpy_width, unsigned cpy_height) {
    const unsigned buffer_width_3 = buffer_width * 3;
    unsigned char *src_base = buffer_frame + buffer_width_3 * src_pxy + src_pxx * 3; // first row first column
    unsigned char *dst_base = buffer_frame + buffer_width_3 * (dst_pxy + cpy_width - 1) + dst_pxx * 3; // last row first column
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int src_row = 0; src_row < cpy_height; ++src_row) {
        for (int src_col = 0; src_col < cpy_width; ++src_col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= buffer_width_3;
            src += 3;
        }
        // move src down a row and to the first column
        src_base += buffer_width_3;
        src = src_base;
        // move dst to the bottom row and next column over
        dst_base += 3;
        dst = dst_base;
    }
}

void moveRectInlineRotate180(unsigned char* buffer_frame, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned dst_pxx, unsigned dst_pxy, unsigned cpy_width, unsigned cpy_height) {
    // printf("Rotating (%d, %d) to (%d, %d)\n", src_pxx, src_pxy, dst_pxx, dst_pxy);
    const unsigned buffer_width_3 = buffer_width * 3;
    unsigned char *src_base = buffer_frame + buffer_width_3 * src_pxy + src_pxx * 3; // first row first col
    unsigned char *dst_base = buffer_frame + buffer_width_3 * (dst_pxy + cpy_height - 1) + (dst_pxx + cpy_width - 1) * 3;// last row last col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int src_col = 0; src_col < cpy_height; ++src_col) {
        for (int src_row = 0; src_row < cpy_width; ++src_row) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= 3;
            src += 3;
        }
        // move src down a row and to the first column
        src_base += buffer_width_3;
        src = src_base;
        // move dst up a row and to the last column
        dst_base -= buffer_width_3;
        dst = dst_base;
    }
}

// assuming dst and src don't overlap
void moveRectInlineRotate90CW(unsigned char* buffer_frame, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned dst_pxx, unsigned dst_pxy, unsigned cpy_width, unsigned cpy_height) {
    // printf("Moving from (%d,%d) to (%d,%d) size: %d x %d with 90 deg CW rotation\n", src_pxx, src_pxy, dst_pxx, dst_pxy, cpy_width, cpy_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    unsigned char *src_base = buffer_frame + buffer_width_3 * src_pxy + (src_pxx + cpy_width - 1) * 3; // first row last col
    unsigned char *dst_base = buffer_frame + buffer_width_3 * (dst_pxy + cpy_width - 1) + (dst_pxx + cpy_height - 1) * 3;// last row last col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int src_row = 0; src_row < cpy_height; ++src_row) {
        for (int src_col = 0; src_col < cpy_width; ++src_col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= buffer_width_3;
            src -= 3;
        }
        // move src down a row and to the last column
        src_base += buffer_width_3;
        src = src_base;
        // move dst to the bottom row and next column to the left
        dst_base -= 3;
        dst = dst_base;
    }
}

// TODO: write moveRectInlineRotate*AndBlankSrc, then wont need blankSquare in processRotateCCW or processRotateCW, more cache efficient

// we know that dst and src don't overlap
void moveTempToBufferRotate90CW(unsigned char* buffer_frame, unsigned char *temp, unsigned buffer_width, unsigned dst_pxx, unsigned dst_pxy, unsigned temp_width, unsigned temp_height) {
    // printf("Moving from temp buffer to rect (%d, %d) size %d x %d with a 90 deg CW rotation\n", dst_pxx, dst_pxy, temp_width, temp_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned temp_width_3 = temp_width * 3;

    unsigned char *src_base = temp + temp_width_3 - 3; // top row, last col
    unsigned char *dst_base = buffer_frame  + buffer_width_3 * (dst_pxy + temp_width - 1) + (dst_pxx + temp_height - 1) * 3; // bottom row, last col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int row = 0; row < temp_height; ++row) {
        for (int col = 0; col < temp_width; ++col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= buffer_width_3; // move up 1 row
            src -= 3; // move left 1 column
        }
        // move src next row down, last col
        src_base += temp_width_3;
        src = src_base;
        // move dst next col left, last row
        dst_base -= 3;
        dst = dst_base;
    }

}
void moveTempToBufferRotate180(unsigned char* buffer_frame, unsigned char *temp, unsigned buffer_width, unsigned dst_pxx, unsigned dst_pxy, unsigned temp_width, unsigned temp_height) {
    // printf("Rot180 temp to buff location (%d, %d), %dx%d\n", dst_pxx, dst_pxy, temp_width, temp_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned temp_width_3 = temp_width * 3;

    unsigned char *src_base = temp + temp_width_3 - 3; // top row, last col
    unsigned char *dst_base = buffer_frame  + buffer_width_3 * (dst_pxy + temp_height - 1) + dst_pxx * 3; // bottom row, first col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int row = 0; row < temp_height; ++row) {
        for (int col = 0; col < temp_width; ++col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst += 3; // move right 1 column
            src -= 3; // move left 1 column
        }
        // move dst next row up, first column
        dst_base -= buffer_width_3;
        dst = dst_base;
        // move src next row down, last col
        src_base += temp_width_3;
        src = src_base;
    }
}

void moveTempToBufferRotate90CCW(unsigned char* buffer_frame, unsigned char *temp, unsigned buffer_width, unsigned dst_pxx, unsigned dst_pxy, unsigned temp_width, unsigned temp_height) {
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned temp_width_3 = temp_width * 3;

    unsigned char *src_base = temp; // top row, first col
    unsigned char *dst_base = buffer_frame  + buffer_width_3 * (dst_pxy + temp_width - 1) + dst_pxx * 3; // bottom row, first col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    for (int row = 0; row < temp_height; ++row) {
        for (int col = 0; col < temp_width; ++col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= buffer_width_3; // move up 1 row
            src += 3; // move right 1 column
        }
        // move dst col left, bottom row
        dst_base += 3;
        dst = dst_base;
        // move src next row down, first col
        src_base += temp_width_3;
        src = src_base;
    }
}

void moveRectToTemp(unsigned char* buffer_frame, unsigned char *temp, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned cpy_width, unsigned cpy_height) {
    // printf("Moving rect (%d, %d) size %d x %d to a temp buffer(%p)\n", src_pxx, src_pxy, cpy_width, cpy_height, temp);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned cpy_width_3 = cpy_width * 3;

    unsigned char *src = buffer_frame + buffer_width_3 * src_pxy + src_pxx * 3; // top left pixel of src
    unsigned char *dst = temp; // 0,0 index of temp
    for (int row = 0; row < cpy_height; ++row) {
        // memcpy row by row
        // we can safely memcpy because temp is assumed to be a separate array from buffer_frame
        memcpy(dst, src, cpy_width_3);
        src += buffer_width_3;
        dst += cpy_width_3;
    }
}

void moveTempToBuffer(unsigned char* buffer_frame, unsigned char *temp, unsigned buffer_width, unsigned dst_pxx, unsigned dst_pxy, unsigned temp_width, unsigned temp_height) {
    const unsigned temp_width_3 = temp_width * 3;
    const unsigned buffer_width_3 = buffer_width * 3;

    unsigned char *src = temp;
    unsigned char *dst = buffer_frame + buffer_width_3 * dst_pxy + dst_pxx * 3;
    for (int i = 0; i < temp_height; ++i) {
        // memcpy row by row
        // we can safely memcpy because temp is assumed to be a separate array from buffer_frame
        memcpy(dst, src, temp_width_3);
        dst += buffer_width_3;
        src += temp_width_3;
    }
}
/* End Helper Functions */

/* SubSquare Manipulation functions */
void swapAndMirrorXSubsquares(unsigned char *buffer_frame, unsigned buffer_width, unsigned top_left_pxindex, unsigned top_top_pxindex, unsigned bottom_left_pxindex, unsigned bottom_bottom_pxindex, unsigned subsquare_width, unsigned subsquare_height) {
    // printf("MX tl = (%d, %d) bl = (%d, %d), %dx%d\n", top_left_pxindex, top_top_pxindex, bottom_left_pxindex, bottom_bottom_pxindex, subsquare_width, subsquare_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned subsquare_width_3_sizeof_char = subsquare_width * 3 * sizeof(char);

    // TODO we can make this a purely stack variable once we know an upperbound on subsquare_width
    unsigned char *temp = malloc(subsquare_width_3_sizeof_char);
    unsigned char *buffer_tl = buffer_frame + top_top_pxindex * buffer_width_3 + top_left_pxindex * 3;
    unsigned char *buffer_bl = buffer_frame + bottom_bottom_pxindex * buffer_width_3 + bottom_left_pxindex * 3;
    for (int i = 0; i < subsquare_height; ++i) {
        memcpy(temp, buffer_tl, subsquare_width_3_sizeof_char);
        memcpy(buffer_tl, buffer_bl, subsquare_width_3_sizeof_char);
        memcpy(buffer_bl, temp, subsquare_width_3_sizeof_char);
        // move to first element of next row down
        buffer_tl += buffer_width_3;
        // move to last element of next row down
        buffer_bl -= buffer_width_3;
    }
    free(temp);
}

void swapAndMirrorYSubsquares(unsigned char *buffer_frame, unsigned buffer_width, unsigned left_left_pxindex, unsigned left_top_pxindex, unsigned right_right_pxindex, unsigned right_top_pxindex, unsigned subsquare_width, unsigned subsquare_height) {
    // printf("MY lt = (%d, %d) rt = (%d, %d), %dx%d\n", left_left_pxindex, left_top_pxindex, right_right_pxindex, right_top_pxindex, subsquare_width, subsquare_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned left_left_pxindex_3 = left_left_pxindex * 3;
    const unsigned right_right_pxindex_3 = right_right_pxindex * 3;

    unsigned ll_index, rr_index;
    unsigned char temp;

    // printf("MirrorY swapping the square at tl(%d,%d) with the square tr(%d,%d)\n", left_left_pxindex, left_top_pxindex, right_right_pxindex, right_top_pxindex);
    unsigned left_row_offset = left_top_pxindex * buffer_width_3;
    unsigned right_row_offset = right_top_pxindex * buffer_width_3;
    for (int i = 0; i < subsquare_height; ++i) {
        // move to first element of next row down
        ll_index = left_left_pxindex_3 + left_row_offset;
        // move to last element of next row down
        rr_index = right_right_pxindex_3 + right_row_offset;
        for (int j = 0; j < subsquare_width; ++j) {
            // swap red values of left and right, in mirrored x order
            // printf("\tSwapping red values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index];
            buffer_frame[ll_index++] = buffer_frame[rr_index];
            buffer_frame[rr_index++] = temp;
            // swap blue values of left and right, in mirrored x order
            // printf("\tSwapping blue values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index];
            buffer_frame[ll_index++] = buffer_frame[rr_index];
            buffer_frame[rr_index++] = temp;
            // swap green values of left and right, in mirrored x order
            // printf("\tSwapping green values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index];
            buffer_frame[ll_index++] = buffer_frame[rr_index];
            buffer_frame[rr_index] = temp;

            rr_index -= 5; // move to the red value of the previous pixel on the right side frame
        }
        left_row_offset += buffer_width_3;
        right_row_offset += buffer_width_3;
        // printf("----\n");
    }
}


/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveUp(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
    #ifndef USE_TRANSLATEOPTS
    unsigned char *ret = processMoveUpReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    return ret;

    #else

    // use memmove to copy over the image
    int const widthPixels = 3 * width;
    int const difference = widthPixels * offset;
    memmove(buffer_frame, buffer_frame + difference, widthPixels * height - difference);

    // fill left over pixels with white pixels
    memset(buffer_frame + (height - offset) * widthPixels, 255, difference);

    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    // return a pointer to the updated image buffer
    return buffer_frame;

    #endif
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image left
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveRight(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
    #ifndef USE_TRANSLATEOPTS

    unsigned char *ret = processMoveRightReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    return ret;

    #else

    int const widthTriple = 3 * width;
    int const offsetTriple = 3 * offset;
    int const shiftedTriple = widthTriple - offsetTriple; // the amount of pixels in RGB that actually get shifted
    unsigned char *rowBegin = buffer_frame;

    for (int row = 0; row < height; row++){
      memmove(rowBegin + offsetTriple, rowBegin, shiftedTriple);
      memset(rowBegin, 255, offsetTriple);
      rowBegin += widthTriple;
    }
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    // return a pointer to the updated image buffer
    return buffer_frame;
    #endif

}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveDown(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
    #ifndef USE_TRANSLATEOPTS
    unsigned char *ret = processMoveDownReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    return ret;

    #else
    // use memmove to copy over the image
    int const widthPixels = 3 * width;
    int const difference = widthPixels * offset;
    memmove (buffer_frame + difference, buffer_frame, widthPixels * height - difference);

    // fill left over pixels with white pixels
    memset (buffer_frame, 255, difference);

    // return a pointer to the updated image buffer
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    return buffer_frame;

    #endif
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image right
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveLeft(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
    #ifndef USE_TRANSLATEOPTS
    unsigned char *ret = processMoveLeftReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    populateIsWhiteArea(buffer_frame, width, height);
    #endif
    return ret;

    #else

    // only do basic translation opts
    int const widthTriple = 3 * width;
    int const offsetTriple = 3 * offset;
    int const shiftedTriple = widthTriple - offsetTriple; // the amount of pixels in RGB that actually get shifted
    unsigned char *rowBegin = buffer_frame;

    #ifndef USE_ISWHITEAREA
    for (int row = 0; row < height; row++){
      memmove(rowBegin, rowBegin + offsetTriple, shiftedTriple);
      memset(rowBegin + shiftedTriple, 255, offsetTriple);
      rowBegin += widthTriple;
    }
    #else

    // whitespace optimization
    unsigned wsLength = 0; // length of adjacent fullStride subsquares
    unsigned wsCurrentRow; // row * width of the bool array
    unsigned wsCurrentIdx; // row_square + column
    unsigned const wsTotalSquares = middleSquareDimensions ? numFullStridesX + 1 : numFullStridesX;
    unsigned const wsFullStridesHalf = numFullStridesX / 2; // FFF bitwise this
    unsigned wsSrc = wsTotalSquares; // location of first subsquare in row, set to impossible value as marker
    unsigned wsMiddleX;

    unsigned pxXSrc; // actually used multiple times but supposed to be src top left corner
    unsigned pxYSrc;
    unsigned pxLengthTriple; // total length of bytes to change
    unsigned pxHeight; // number of rows to iterate over from subsquare
    unsigned char *pxSrc = buffer_frame;
    unsigned wsRow, wsCol, pxRow;


    // go through each row
    for (wsRow = 0; wsRow < wsTotalSquares; wsRow++){
      wsCurrentRow = wsRow * wsTotalSquares;

      // for each row of subsquares, find nonwhite squares and translate those
      for (wsCol = 0; wsCol < wsTotalSquares; wsCol++){
        wsCurrentIdx = wsCurrentRow + wsCol;

	// first nonwhite
	if (!isWhiteArea[wsCurrentIdx] && wsSrc == wsTotalSquares){
	  wsSrc = wsCurrentIdx;
	  pxHeight = middleSquareDimensions && (wsCol == wsFullStridesHalf) ? middleSquareDimensions : isWhiteAreaStride; // consider if normal or middle row
	} // white at the end of nonwhites or end of row lel
	else if (wsSrc != wsTotalSquares && (isWhiteArea[wsCurrentIdx] || wsCurrentIdx == wsTotalSquares - 1)){
	  wsLength = wsCurrentIdx - wsSrc;
	  if (middleSquareDimensions && wsCurrentIdx > wsFullStridesHalf && wsSrc <= wsFullStridesHalf) { // only decrement the wsLength if the blocks span a middle block
	    wsLength--;
	    wsMiddleX = 1;
	  } else {
	    wsMiddleX = 0;
	  }

	  // get the beginning of the entire row in bytes
	  translateWhiteSpaceArrayIndicesToPixel(0, wsRow, &pxXSrc, &pxYSrc);
	  rowBegin = buffer_frame + 3 * (widthTriple * pxYSrc + pxXSrc);

	  // get the beginning of the subsquares
	  translateWhiteSpaceArrayIndicesToPixel(wsSrc, wsRow, &pxXSrc, &pxYSrc);
	  pxSrc = buffer_frame + 3 * (widthTriple * pxYSrc + pxXSrc);

	  pxLengthTriple = 3 * (wsMiddleX * middleSquareDimensions + wsLength * isWhiteAreaStride);
          if (pxSrc - offsetTriple < rowBegin) {
	   pxLengthTriple -= offsetTriple;
	   pxSrc += offsetTriple;
	  }

	  // for each row of stride
          for (pxRow = 0; pxRow < pxHeight; pxRow++){
	    memmove(pxSrc - offsetTriple, pxSrc, pxLengthTriple);
	    pxSrc += widthTriple;
	    printf("here\n");
	  }

	  // reset this value as a marker
	  wsSrc = wsTotalSquares;
	}
      }
    }
    //return a pointer to the updated image buffer
    populateIsWhiteArea(buffer_frame, width, height);
    #endif

    return buffer_frame;

    #endif
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
unsigned char *processRotateCW(unsigned char *buffer_frame, unsigned width, unsigned height,
                               int rotate_iteration) {
    // printf("RotateCW called with %d %d %d\n", width, height, rotate_iteration);
    #ifndef USE_ISWHITEAREA
    return processRotateCWReference(buffer_frame, width, height, rotate_iteration);
    #endif
    // our condenser will limit our CW rotation to 90 deg or 180 deg
    #ifdef USE_ISWHITEAREA
    int const whiteSpaceArrayWidth = numFullStridesX + (middleSquareDimensions != 0);
    int const whiteSpaceArrayHeight = whiteSpaceArrayWidth;
    unsigned tl_px_x, tl_px_y;
    unsigned tr_px_x, tr_px_y;
    unsigned bl_px_x, bl_px_y;
    unsigned br_px_x, br_px_y;
    if (rotate_iteration == 1) {
        for (int row = 0; row < whiteSpaceArrayHeight / 2; ++row) {
            for (int col = 0; col < whiteSpaceArrayWidth / 2; ++col) {
                // printf("Rotating ws array (%d, %d) and its associated squares\n", col, row);
                int TL_index = row * whiteSpaceArrayWidth + col;
                int BL_index = (whiteSpaceArrayHeight - 1 - col) * whiteSpaceArrayWidth + row;
                int BR_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - col - 1);
                int TR_index = col * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
                int condition_hash = (!isWhiteArea[TL_index] << 3) +
                                     (!isWhiteArea[TR_index] << 2) +
                                     (!isWhiteArea[BR_index] << 1) +
                                     (!isWhiteArea[BL_index]);
                // if (condition_hash != 0) printf("Rotating WS square (%d, %d), hash: %d\n", col, row, condition_hash);
                switch (condition_hash) {
                    // if (!TL && !BL && !BR && !TR) don't need to rotate blank squares
                    case 0b0000: continue; break;
                    // else if (!TL && !TR && !BR && BL) writeRot90 BL into TL, blank BL
                    case 0b0001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && !TR && BR && !BL) writeRot90 BR into BL, blank BR
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && !BR && !BL) writeRot90 TR into BR, blank TR
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && !BR && !BL) writeRot90 TL into TR, blank TL
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && !TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, blank BR
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 BL into TL, blank BL, blank TR
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && !BR && BL) writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && BR && !BL) writeRot90 TL into TR, writeRot90 BR into BL, blank TL, blank BR
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && !BR && !BL) writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && BR && BL) writeRot90 TL into TR, BL into TL, BR into BL, blank BR
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // if (TL && TR && BR && BL) write BL into temp buffer, writeRot90 BR into BL, TR into BR, TL into TR, temp into TL
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride); // TR to temp
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x,  tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride); // TL to TR
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y,  tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride); // BL to TL
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride); // BR to BL
                        moveTempToBufferRotate90CW(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride); // temp to BR
                        break;
                }
                // rotate white space array
                bool temp_bool = isWhiteArea[TL_index];
                isWhiteArea[TL_index] = isWhiteArea[BL_index];
                isWhiteArea[BL_index] = isWhiteArea[BR_index];
                isWhiteArea[BR_index] = isWhiteArea[TR_index];
                isWhiteArea[TR_index] = temp_bool;
            }
        }
        if (middleSquareDimensions != 0) {
            // middle columns and rows
            int col = whiteSpaceArrayWidth / 2;
            unsigned top_px_x, top_px_y;
            unsigned right_px_x, right_px_y;
            unsigned bottom_px_x, bottom_px_y;
            unsigned left_px_x, left_px_y;
            for (int row = 0; row < whiteSpaceArrayHeight / 2; ++row) {
                int top_col_index = row * whiteSpaceArrayWidth + col;
                int left_row_index = (whiteSpaceArrayHeight / 2) * whiteSpaceArrayWidth + row;
                int right_row_index = (whiteSpaceArrayHeight / 2) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
                int bottom_col_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + col;
                int condition_hash = (!isWhiteArea[top_col_index] << 3) +
                                     (!isWhiteArea[right_row_index] << 2) +
                                     (!isWhiteArea[bottom_col_index] << 1) +
                                     (!isWhiteArea[left_row_index]);
                // if (condition_hash != 0) printf("Rotating Middle Col WS square (%d, %d), hash: %d\n", col, row, condition_hash);
                switch (condition_hash) {
                    case 0b0000: continue; break;
                    case 0b0001: // left into top, blank left
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // bottom into left, blank bottom
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, blank right
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top into right, blank top
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // left into top, bottom into left, blank bottom
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, left into top, blank right blank left
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // bottom into left, right into bottom, blank right
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top in to right, left into top, blank left
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top in to right, bottom into left, blank top blank bottom
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, top into right, blank top
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // bottom into left, right into bottom, top into right, blank top
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, top into right, left into top, blank left
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top into right, left into top, bottom into left, blank bottom
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // left into top, bottom into left, right into bottom, blank right
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // right to temp, top to right, left to top, bottom to left, temp to bottom
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions); // Right to temp
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x,  top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride); // Top to Right
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y,  top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions); // Bottom to Top
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride); // BR to Bottom
                        moveTempToBufferRotate90CW(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions); // temp to BR
                        break;
                }

                // Update whitespace array
                bool temp_bool = isWhiteArea[top_col_index];
                isWhiteArea[top_col_index] = isWhiteArea[left_row_index];
                isWhiteArea[left_row_index] = isWhiteArea[bottom_col_index];
                isWhiteArea[bottom_col_index] = isWhiteArea[right_row_index];
                isWhiteArea[right_row_index] = temp_bool;

                top_col_index += whiteSpaceArrayWidth; // down 1 row
                bottom_col_index -= whiteSpaceArrayWidth; // up 1 row
                --right_row_index; // left one column
                ++left_row_index; // right one column

            }
            if (!isWhiteArea[whiteSpaceArrayWidth * (whiteSpaceArrayHeight / 2) + (whiteSpaceArrayWidth / 2)]) {
                translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth / 2, whiteSpaceArrayHeight / 2, &tl_px_x, &tl_px_y); // reusing tl_px_* for middle square
                moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
                moveTempToBufferRotate90CW(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
            }
            // no whitespace array update required
        }
    } else { // CCW/CW 180 deg
        for (int row = 0; row < whiteSpaceArrayHeight / 2; ++row) {
            for (int col = 0; col < whiteSpaceArrayWidth / 2; ++col) {
                // printf("Rotating ws array (%d, %d) and its associated squares\n", col, row);
                int TL_index = row * whiteSpaceArrayWidth + col;
                int BL_index = (whiteSpaceArrayHeight - 1 - col) * whiteSpaceArrayWidth + row;
                int BR_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - col - 1);
                int TR_index = col * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
                int condition_hash = (!isWhiteArea[TL_index] << 3) +
                                     (!isWhiteArea[TR_index] << 2) +
                                     (!isWhiteArea[BR_index] << 1) +
                                     (!isWhiteArea[BL_index]);
                // if (condition_hash != 0) printf("Rotating WS square (%d, %d), hash: %d\n", col, row, condition_hash);
                switch (condition_hash) {
                    case 0b0000: continue; break;

                    // BL into TR, blank BL
                    case 0b0001:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, blank BR
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into BL, blank TR
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, blank TL
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, BL into TR, blank BR, blank BL
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into temp, BL into TR, temp into BL
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);
                        // TODO write inline swap 180
                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into BL, BR into TL, blank TR, blank BR
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, BL into TR, blank TL, blank BL
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into temp, TL into BR, temp into TL
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, TR into BL, blank TL, blank TR
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, TR into BL, blank TR
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, blank TL, TR into temp, BL into TR, temp into BL
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, BL into TR, blank BL
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, blank BR, BL into temp, TR into BL, temp into TR
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, TR into temp, BL into TR, temp into BL
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;
                }
                // rotate white space array
                bool temp_bool = isWhiteArea[TL_index];
                isWhiteArea[TL_index] = isWhiteArea[BR_index];
                isWhiteArea[BR_index] = temp_bool;
                temp_bool = isWhiteArea[BL_index];
                isWhiteArea[BL_index] = isWhiteArea[TR_index];
                isWhiteArea[TR_index] = temp_bool;
            }
        }
        if (middleSquareDimensions != 0) {
            // middle columns and rows
            int col = whiteSpaceArrayWidth / 2;
            unsigned top_px_x, top_px_y;
            unsigned right_px_x, right_px_y;
            unsigned bottom_px_x, bottom_px_y;
            unsigned left_px_x, left_px_y;
            for (int row = 0; row < whiteSpaceArrayHeight / 2; ++row) {
                int top_col_index = row * whiteSpaceArrayWidth + col;
                int left_row_index = (whiteSpaceArrayHeight / 2) * whiteSpaceArrayWidth + row;
                int right_row_index = (whiteSpaceArrayHeight / 2) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
                int bottom_col_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + col;
                int condition_hash = (!isWhiteArea[top_col_index] << 3) +
                                     (!isWhiteArea[right_row_index] << 2) +
                                     (!isWhiteArea[bottom_col_index] << 1) +
                                     (!isWhiteArea[left_row_index]);
                // if (condition_hash != 0) printf("Rotating Middle Col WS square (%d, %d), hash: %d\n", col, row, condition_hash);
                switch (condition_hash) {
                    // do nothing
                    case 0b0000: continue; break;

                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // TODO fix me
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    case 0b0001:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // TODO fix me
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                }

                // Update whitespace array
                bool temp_bool = isWhiteArea[top_col_index];
                isWhiteArea[top_col_index] = isWhiteArea[bottom_col_index];
                isWhiteArea[bottom_col_index] = temp_bool;
                temp_bool = isWhiteArea[left_row_index];
                isWhiteArea[left_row_index] = isWhiteArea[right_row_index];
                isWhiteArea[right_row_index] = temp_bool;

                top_col_index += whiteSpaceArrayWidth; // down 1 row
                bottom_col_index -= whiteSpaceArrayWidth; // up 1 row
                --right_row_index; // left one column
                ++left_row_index; // right one column

            }
            if (!isWhiteArea[whiteSpaceArrayWidth * (whiteSpaceArrayHeight / 2) + (whiteSpaceArrayWidth / 2)]) {
                translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth / 2, whiteSpaceArrayHeight / 2, &tl_px_x, &tl_px_y); // reusing tl_px_* for middle square
                moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
                moveTempToBufferRotate180(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
            }
            // no whitespace array update required
        }
    }
    return buffer_frame;
    #endif
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer counter clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
unsigned char *processRotateCCW(unsigned char *buffer_frame, unsigned width, unsigned height,
                                int rotate_iteration) {
    return processRotateCCWReference(buffer_frame, width, height, rotate_iteration);
    // our condenser limits this to be a 90 deg CCW rotation
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
unsigned char *processMirrorX(unsigned char *buffer_frame, unsigned int width, unsigned int height, int _unused) {
    #ifndef USE_MIRROROPTS
    return processMirrorXReference(buffer_frame, width, height, _unused);
    #else
    swapAndMirrorXSubsquares(buffer_frame, width, 0, 0, 0, height-1, width, height/2);
    // TODO: once everything is done in-place, wont need this return anymore
    return buffer_frame;
    #endif
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
unsigned char *processMirrorY(unsigned char *buffer_frame, unsigned width, unsigned height, int _unused) {
    #ifndef USE_MIRROROPTS
    return processMirrorYReference(buffer_frame, width, height, _unused);
    #else
    // TODO: only swap and mirror non-white squares.
    swapAndMirrorYSubsquares(buffer_frame, width, 0, 0, width-1, 0, width/2, height);
    // TODO: once everything is done in-place, wont need this return anymore
    return buffer_frame;
    #endif
}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          Do not forget to modify the team_name and team member information !!!
 **********************************************************************************************************************/
void print_team_info(){
    // Please modify this field with something interesting
    char team_name[] = "Cache_Me_Ousside";

    // Please fill in your information
    char student1_first_name[] = "Connor";
    char student1_last_name[] = "Smith";
    char student1_student_number[] = "1000421411";

    // Please fill in your partner's information
    // If yon't have partner, do not modify this
    char student2_first_name[] = "Fan";
    char student2_last_name[] = "Guo";
    char student2_student_number[] = "1000626539";

    // Printing out team information
    printf("*******************************************************************************************************\n");
    printf("Team Information:\n");
    printf("\tteam_name: %s\n", team_name);
    printf("\tstudent1_first_name: %s\n", student1_first_name);
    printf("\tstudent1_last_name: %s\n", student1_last_name);
    printf("\tstudent1_student_number: %s\n", student1_student_number);
    printf("\tstudent2_first_name: %s\n", student2_first_name);
    printf("\tstudent2_last_name: %s\n", student2_last_name);
    printf("\tstudent2_student_number: %s\n", student2_student_number);
}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          You can modify anything else in this file
 ***********************************************************************************************************************
 * @param sensor_values - structure stores parsed key value pairs of program instructions
 * @param sensor_values_count - number of valid sensor values parsed from sensor log file or commandline console
 * @param frame_buffer - pointer pointing to a buffer storing the imported  24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param grading_mode - turns off verification and turn on instrumentation
 ***********************************************************************************************************************
 *
 **********************************************************************************************************************/
void implementation_driver(struct kv *sensor_values, int sensor_values_count, unsigned char *frame_buffer,
                           unsigned int width, unsigned int height, bool grading_mode) {
    #ifdef USE_INSTRUCTIONCONDENSER
    int collapsed_sensor_values_count = 0;
    // this is the naive one. The better condensers are collapse_sensor_values and collapse_sensor_values2
    optimized_kv *collapsed_sensor_values = collapse_sensor_values(sensor_values, sensor_values_count, &collapsed_sensor_values_count);
    /*
    printf("Original Sensor number: %d, New Sensor Count: %d\n", sensor_values_count, collapsed_sensor_values_count - sensor_values_count / 25); // don't count the FRAME_BREAKS, as they are unavoidable
    for (int i = 0; i < collapsed_sensor_values_count; ++i) {
        printf("\tCommand: %d Value: %d\n", collapsed_sensor_values[i].type, collapsed_sensor_values[i].value);
    }
    */
    #endif
    #ifdef USE_ISWHITEAREA
    createIsWhiteArea(frame_buffer, width, height);
    #endif
    #ifdef USE_INSTRUCTIONCONDENSER
    for (int i = 0; i < collapsed_sensor_values_count; ++i) {
        switch(collapsed_sensor_values[i].type) {
            case   W: frame_buffer = processMoveUp(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case   A: frame_buffer = processMoveLeft(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case   S: frame_buffer = processMoveDown(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case   D: frame_buffer = processMoveRight(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case  CW: frame_buffer = processRotateCW(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case CCW: frame_buffer = processRotateCCW(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case  MX: frame_buffer = processMirrorX(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case  MY: frame_buffer = processMirrorY(frame_buffer, width, height, collapsed_sensor_values[i].value); break;
            case FRAME_BREAK: verifyFrame(frame_buffer, width, height, grading_mode); break;
        }
    }
    #else
    struct kv *sensor_value;
    movement_type type;
    for (int i = 0; i < sensor_values_count;) {
        sensor_value = &sensor_values[i];
        type = sensor_value->key[0] + sensor_value->key[1];
        switch(type) {
            case   W: frame_buffer = processMoveUp(frame_buffer, width, height, sensor_value->value); break;
            case   A: frame_buffer = processMoveLeft(frame_buffer, width, height, sensor_value->value); break;
            case   S: frame_buffer = processMoveDown(frame_buffer, width, height, sensor_value->value); break;
            case   D: frame_buffer = processMoveRight(frame_buffer, width, height, sensor_value->value); break;
            case  CW: frame_buffer = processRotateCW(frame_buffer, width, height, sensor_value->value); break;
            case CCW: frame_buffer = processRotateCCW(frame_buffer, width, height, sensor_value->value); break;
            case  MX: frame_buffer = processMirrorX(frame_buffer, width, height, sensor_value->value); break;
            case  MY: frame_buffer = processMirrorY(frame_buffer, width, height, sensor_value->value); break;
        }
        ++i;
        if (i % 25 == 0) {
            verifyFrame(frame_buffer, width, height, grading_mode);
        }
    }
    #endif
    #ifdef USE_ISWHITEAREA
    free(isWhiteArea);
    free(temp_square);
    #endif
    #ifdef USE_INSTRUCTIONCONDENSER
    free(collapsed_sensor_values);
    #endif
    return;
}
