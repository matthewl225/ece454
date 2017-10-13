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
#define isWhiteAreaStride 50 // translates to 64 bytes, each pixel is 3 bytes
bool *isWhiteArea;
unsigned char *temp_square;
unsigned int numFullStridesX;
unsigned int numFullStridesY;
unsigned int middleSquareDimensions;


void translatePixelToWhiteSpaceArrayIndices(unsigned px_x, unsigned px_y, unsigned px_width, unsigned *ws_x_out, unsigned *ws_y_out) {
    // if px_x is inside the middle column, return the middle column
    const bool hasMiddleSquare = (middleSquareDimensions != 0);

    if (!hasMiddleSquare) {
        *ws_x_out = px_x / isWhiteAreaStride;
        *ws_y_out = px_y / isWhiteAreaStride;
    } else {
        const unsigned numFullStridesX_div_2 = numFullStridesX >> 1;
        const unsigned numFullStridesY_div_2 = numFullStridesY >> 1;
        int test = px_x - (numFullStridesX_div_2 * isWhiteAreaStride);
        if (test < middleSquareDimensions && test >= 0) {
            // inside the middle
            *ws_x_out = numFullStridesX_div_2;
        } else if (test < 0) {
            // left of the middle
            *ws_x_out = px_x / isWhiteAreaStride;
        } else if (test >= middleSquareDimensions) {
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
    // printf("Translating px (%d, %d) to (%d, %d)\n", px_x, px_y, *ws_x_out, *ws_y_out);

}

// returns the top left pixel of the given white space array square
void translateWhiteSpaceArrayIndicesToPixel(unsigned ws_x, unsigned ws_y, unsigned ws_width, unsigned *px_x_out, unsigned *px_y_out) {
    if (middleSquareDimensions != 0 && ws_x > (numFullStridesX >> 1)) {
        *px_x_out = (ws_x - 1) * isWhiteAreaStride + middleSquareDimensions;
    } else {
        *px_x_out = ws_x * isWhiteAreaStride;
    }

    if (middleSquareDimensions != 0 && ws_y > (numFullStridesY >> 1)) {
        *px_y_out = (ws_y - 1) * isWhiteAreaStride + middleSquareDimensions;
    } else {
        *px_y_out = ws_y * isWhiteAreaStride;
    }
}

bool checkWhiteAreaSquare(unsigned char *buffer_frame, unsigned buffer_pxwidth, unsigned whiteAreaCol, unsigned whiteAreaRow, unsigned pxWidth, unsigned pxHeight) {
    const unsigned buffer_pxwidth_3 = buffer_pxwidth * 3;
    
    unsigned tl_px_x, tl_px_y;
    translateWhiteSpaceArrayIndicesToPixel(whiteAreaCol, whiteAreaRow, numFullStridesX + (middleSquareDimensions != 0), &tl_px_x, &tl_px_y);
    // printf("Checking square cornered at (%d, %d), width: %d height: %d\n", tl_px_x, tl_px_y, pxWidth, pxHeight);
    unsigned base_px_index = tl_px_y * buffer_pxwidth_3 + tl_px_x * 3;
    unsigned pixel_index = base_px_index;
    int row, col;
    for (row = 0; row < pxHeight; ++row) {
        for (col = 0; col < pxWidth; ++col) {
            if (buffer_frame[pixel_index] != 255 ||
                buffer_frame[pixel_index+1] != 255 ||
                buffer_frame[pixel_index+2] != 255)
            {
                return false;
            }
            pixel_index += 3;
        }
        base_px_index += buffer_pxwidth_3;
        pixel_index = base_px_index;
    }
    return true;
}

void updateIsWhiteAreaLeftOffset(unsigned char *buffer_frame, unsigned buffer_width, unsigned left_offset) {
    // check how many strides to the left we have to check of every black square
    unsigned const wsArrayDimensions = numFullStridesX + (middleSquareDimensions != 0);
    unsigned const numFullStridesX_div_2 = numFullStridesX >> 1;
    int const middleIndex = middleSquareDimensions != 0 ? numFullStridesX_div_2 : -1;
    // iterate top->bottom, left -> right
    unsigned row_wsArrayDimensions = 0;
    unsigned orig_px_x, orig_px_y;
    long dst_px_x; // has to handle negatives properly
    unsigned dst_ws_row, dst_ws_col, dst_row_wsDimensions;
    long dim_width, dim_height;
    // int num_checked = 0;
    int row, col;
    for (row = 0; row < wsArrayDimensions; ++row) {
        for (col = 0; col < wsArrayDimensions; ++col) {
            if (isWhiteArea[row_wsArrayDimensions + col]) {
                continue;
            }
            dim_width = col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            dim_height = row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            isWhiteArea[row_wsArrayDimensions + col] = checkWhiteAreaSquare(buffer_frame, buffer_width, col, row, dim_width, dim_height);
            // num_checked++;

            translateWhiteSpaceArrayIndicesToPixel(col, row, wsArrayDimensions, &orig_px_x, &orig_px_y);
            dst_px_x = orig_px_x - left_offset;
            if (dst_px_x >= 0) {
                translatePixelToWhiteSpaceArrayIndices(dst_px_x, orig_px_y, buffer_width, &dst_ws_col, &dst_ws_row);
                dst_row_wsDimensions = dst_ws_row * wsArrayDimensions;
                if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                    dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                }
                // num_checked++;
                // don't have to check col again or to the right of col
                ++dst_ws_col;
                if (dst_ws_col < col) {
                    if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                        dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                        // height is the same
                        isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                    }
                    // num_checked++;
                }
            } // TODO this wont handle objects being moved "almost" off the screen with a large offset
        }
        row_wsArrayDimensions += wsArrayDimensions;
    }
    /*
    printf("Checked %d WS squares due to left movement\n", num_checked);
    for(int row = 0; row < wsArrayDimensions; ++row) {
        for (int col = 0; col < wsArrayDimensions; ++col) {
            printf("%d",isWhiteArea[row*wsArrayDimensions + col]);
        }
        printf("\n");
    }
    */
    // find a black square top left px and ws index, move left_offset to the left of that and calculate that ws index.
    // also have to check the black square itself
    // might have to check one square back to the right to be sure
    // => at most 3 squares for each black square
}

void updateIsWhiteAreaRightOffset(unsigned char *buffer_frame, unsigned buffer_width, unsigned right_offset) {
    // check how many strides to the left we have to check of every black square
    unsigned const wsArrayDimensions = numFullStridesX + (middleSquareDimensions != 0);
    unsigned const numFullStridesX_div_2 = numFullStridesX >> 1;
    int const middleIndex = middleSquareDimensions != 0 ? numFullStridesX_div_2 : -1;
    // iterate top->bottom, left -> right
    unsigned row_wsArrayDimensions = 0;
    unsigned orig_px_x, orig_px_y;
    long dst_px_x; // has to handle negatives properly
    unsigned dst_ws_row, dst_ws_col, dst_row_wsDimensions;
    long dim_width, dim_height;
    // int num_checked = 0;
    int row, col;
    for (row = 0; row < wsArrayDimensions; ++row) {
        for (col = wsArrayDimensions - 1; col >= 0; --col) {
            if (isWhiteArea[row_wsArrayDimensions + col]) {
                continue;
            }
            dim_width = col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            dim_height = row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            isWhiteArea[row_wsArrayDimensions + col] = checkWhiteAreaSquare(buffer_frame, buffer_width, col, row, dim_width, dim_height);
            // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 1\n", col, row, right_offset, col, row);
            // num_checked++;

            translateWhiteSpaceArrayIndicesToPixel(col, row, wsArrayDimensions, &orig_px_x, &orig_px_y);
            dst_px_x = orig_px_x + right_offset + dim_width - 1;
            if (dst_px_x < buffer_width) {
                translatePixelToWhiteSpaceArrayIndices(dst_px_x, orig_px_y, buffer_width, &dst_ws_col, &dst_ws_row);
                dst_row_wsDimensions = dst_ws_row * wsArrayDimensions;
                if (isWhiteArea[dst_ws_col + dst_row_wsDimensions]) {
                    dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                }
                // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 2\n", col, row, right_offset, dst_ws_col, dst_ws_row);
                // num_checked++;
                // don't have to check col again or to the left of col
                --dst_ws_col;
                if (dst_ws_col > col) {
                    // height is the same
                    if (isWhiteArea[dst_ws_col + dst_row_wsDimensions]) {
                        dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                        isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                    }
                    // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 3\n", col, row, right_offset, dst_ws_col, dst_ws_row);
                    // num_checked++;
                }
            } // TODO this wont handle objects being moved "almost" off the screen with a large offset
        }
        row_wsArrayDimensions += wsArrayDimensions;
    }
    /*
    printf("Checked %d WS squares due to right movement\n", num_checked);
    for(int row = 0; row < wsArrayDimensions; ++row) {
        for (int col = 0; col < wsArrayDimensions; ++col) {
            printf("%d",isWhiteArea[row*wsArrayDimensions + col]);
        }
        printf("\n");
    }
    */
    // check how many strides to the right we have to check of every black square
    // iterate top->bottom, right -> left
    // find a black square top RIGHT px and ws index, move right_offset to the right of that and calculate that ws index.
    // also have to check the black square itself
    // might have to check one square back to the left to be sure
    // => at most 3 squares for each black square

}

void updateIsWhiteAreaUpOffset(unsigned char *buffer_frame, unsigned buffer_width, unsigned up_offset) {
    // check how many strides to the left we have to check of every black square
    unsigned const wsArrayDimensions = numFullStridesX + (middleSquareDimensions != 0);
    unsigned const numFullStridesX_div_2 = numFullStridesX >> 1;
    int const middleIndex = middleSquareDimensions != 0 ? numFullStridesX_div_2 : -1;
    // iterate top->bottom, left -> right
    unsigned row_wsArrayDimensions = 0;
    unsigned orig_px_x, orig_px_y;
    long dst_px_y; // has to handle negatives properly
    unsigned dst_ws_row, dst_ws_col, dst_row_wsDimensions;
    long dim_width, dim_height;
    // int num_checked = 0;
    int row, col;
    for (row = 0; row < wsArrayDimensions; ++row) {
        for (col = 0; col < wsArrayDimensions; ++col) {
            if (isWhiteArea[row_wsArrayDimensions + col]) {
                continue;
            }
            dim_width = col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            dim_height = row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 1\n", col, row, up_offset, col, row);
            isWhiteArea[row_wsArrayDimensions + col] = checkWhiteAreaSquare(buffer_frame, buffer_width, col, row, dim_width, dim_height);
            // num_checked++;

            translateWhiteSpaceArrayIndicesToPixel(col, row, wsArrayDimensions, &orig_px_x, &orig_px_y);
            dst_px_y = orig_px_y - up_offset;
            if (dst_px_y >= 0) {
                translatePixelToWhiteSpaceArrayIndices(orig_px_x, dst_px_y, buffer_width, &dst_ws_col, &dst_ws_row);
                dst_row_wsDimensions = dst_ws_row * wsArrayDimensions;
                // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 2\n", col, row, up_offset, dst_ws_col, dst_ws_row);
                if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                    dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                }
                // num_checked++;
                // don't have to check row again or below row
                ++dst_ws_row;
                dst_row_wsDimensions += wsArrayDimensions;
                if (dst_ws_row < row) {
                    // width is the same
                    // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 3\n", col, row, up_offset, dst_ws_col, dst_ws_row);
                    if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                        dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                        isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                    }
                    // num_checked++;
                }
            } // TODO this wont handle objects being moved "almost" off the screen with a large offset
        }
        row_wsArrayDimensions += wsArrayDimensions;
    }
    /*
    for(int row = 0; row < wsArrayDimensions; ++row) {
        for (int col = 0; col < wsArrayDimensions; ++col) {
            printf("%d",isWhiteArea[row*wsArrayDimensions + col]);
        }
        printf("\n");
    }
    */
    // check how many strides up we have to check of every black square
    // iterate top->bottom, left -> right
    // find a black square top left px and ws index, move up_offset up of that and calculate that ws index.
    // also have to check the black square itself
    // might have to check one square back down to be sure
    // => at most 3 squares for each black square

}

void updateIsWhiteAreaDownOffset(unsigned char *buffer_frame, unsigned buffer_width, unsigned down_offset) {
    // check how many strides to the left we have to check of every black square
    unsigned const wsArrayDimensions = numFullStridesX + (middleSquareDimensions != 0);
    unsigned const numFullStridesX_div_2 = numFullStridesX >> 1;
    int const middleIndex = middleSquareDimensions != 0 ? numFullStridesX_div_2 : -1;
    // iterate top->bottom, left -> right
    unsigned row_wsArrayDimensions = (wsArrayDimensions - 1) * wsArrayDimensions;
    unsigned orig_px_x, orig_px_y;
    long dst_px_y; // has to handle negatives properly
    unsigned dst_ws_row, dst_ws_col, dst_row_wsDimensions;
    long dim_width, dim_height;
    // int num_checked = 0;
    int row, col;
    for (row = wsArrayDimensions - 1; row >= 0; --row) {
        for (col = 0; col < wsArrayDimensions; ++col) {
            if (isWhiteArea[row_wsArrayDimensions + col]) {
                continue;
            }
            dim_width = col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            dim_height = row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
            // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 1\n", col, row, up_offset, col, row);
            isWhiteArea[row_wsArrayDimensions + col] = checkWhiteAreaSquare(buffer_frame, buffer_width, col, row, dim_width, dim_height);
            // num_checked++;

            translateWhiteSpaceArrayIndicesToPixel(col, row, wsArrayDimensions, &orig_px_x, &orig_px_y);
            dst_px_y = orig_px_y + dim_height - 1 + down_offset;
            if (dst_px_y <= buffer_width) {
                translatePixelToWhiteSpaceArrayIndices(orig_px_x, dst_px_y, buffer_width, &dst_ws_col, &dst_ws_row);
                dst_row_wsDimensions = dst_ws_row * wsArrayDimensions;
                // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 2\n", col, row, up_offset, dst_ws_col, dst_ws_row);
                if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                    dim_width = dst_ws_col == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                    isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                }
                // num_checked++;
                // don't have to check row again or above row
                --dst_ws_row;
                dst_row_wsDimensions += wsArrayDimensions;
                if (dst_ws_row > row) {
                    // width is the same
                    // printf("Moved (%d, %d) offset %d. Therefore checking (%d, %d) - 3\n", col, row, up_offset, dst_ws_col, dst_ws_row);
                    if (isWhiteArea[dst_row_wsDimensions + dst_ws_col]) {
                        dim_height = dst_ws_row == middleIndex ? middleSquareDimensions : isWhiteAreaStride;
                        isWhiteArea[dst_ws_col + dst_row_wsDimensions] = checkWhiteAreaSquare(buffer_frame, buffer_width, dst_ws_col, dst_ws_row, dim_width, dim_height);
                    }
                    // num_checked++;
                }
            } // TODO this wont handle objects being moved "almost" off the screen with a large offset
        }
        row_wsArrayDimensions -= wsArrayDimensions;
    }
    /*
    for(int row = 0; row < wsArrayDimensions; ++row) {
        for (int col = 0; col < wsArrayDimensions; ++col) {
            printf("%d",isWhiteArea[row*wsArrayDimensions + col]);
        }
        printf("\n");
    }
    */
    // check how many strides up we have to check of every black square
    // iterate bottom->top, left -> right
    // find a black square BOTTOM left px and ws index, move down_offset down of that and calculate that ws index.
    // also have to check the black square itself
    // might have to check one square back up to be sure
    // => at most 3 squares for each black square
}

void populateIsWhiteArea(unsigned char *buffer_frame, unsigned width, unsigned height) {
    // printf("Populating IsWhiteArea\n");
    const bool hasMiddleSquare = (middleSquareDimensions != 0);
    const unsigned int numFullStridesX_div_2 = numFullStridesX >> 1;
    const unsigned int numFullStridesY_div_2 = numFullStridesY >> 1;
    const unsigned int boolArrayWidth = numFullStridesX + hasMiddleSquare;
    const unsigned int boolArrayHeight = numFullStridesY + hasMiddleSquare;

    // 500x500 image = 22 21x21 squares with a 38x38 middle square, 38x21 middle column and 21x38 middle row
    // |11 21-wide squares|38 wide square|11 21-wide squares|

    int whiteAreaRow, whiteAreaCol;
    unsigned row_arrayWidth = 0;
    // upper left corner
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY_div_2; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX_div_2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
        row_arrayWidth += boolArrayWidth;
    }

    // bottom left corner
    row_arrayWidth = (numFullStridesY_div_2 + hasMiddleSquare) * boolArrayWidth;
    for (whiteAreaRow = numFullStridesY_div_2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX_div_2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
        row_arrayWidth += boolArrayWidth;
    }

    // top right corner
    row_arrayWidth = 0;
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY_div_2; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX_div_2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
        row_arrayWidth += boolArrayWidth;
    }

    // bottom right corner
    row_arrayWidth = (numFullStridesY_div_2 + hasMiddleSquare) * boolArrayWidth;
    for (whiteAreaRow = numFullStridesY_div_2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX_div_2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }
        row_arrayWidth += boolArrayWidth;
    }

    if (hasMiddleSquare) {
        // left middle row
        whiteAreaRow = numFullStridesY_div_2;
        row_arrayWidth = whiteAreaRow * boolArrayWidth;
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX_div_2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // right middle row
        for (whiteAreaCol = numFullStridesX_div_2 + 1; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
        }

        // top center row
        whiteAreaCol = numFullStridesX_div_2;
        row_arrayWidth = 0;
        for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY_div_2; ++whiteAreaRow) {
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
            row_arrayWidth += boolArrayWidth;
        }

        // middle square
        isWhiteArea[numFullStridesY_div_2 * boolArrayWidth + whiteAreaCol] =
            checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, middleSquareDimensions);
        // bottom center row
        whiteAreaCol = numFullStridesX_div_2;
        row_arrayWidth = (numFullStridesY_div_2 + 1) * boolArrayWidth;
        for (whiteAreaRow = numFullStridesY_div_2 + 1; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
            isWhiteArea[row_arrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
            // printf("(%d,%d) is %d\n", whiteAreaCol, whiteAreaRow, isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol]);
            row_arrayWidth += boolArrayWidth;
        }

        // middle square
        isWhiteArea[numFullStridesY_div_2 * boolArrayWidth + whiteAreaCol] =
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
    numFullStridesX = (((width / isWhiteAreaStride) >> 1) << 1); // divide into this many 21x21 pixel squares. Must be an even number
    numFullStridesY = (((height / isWhiteAreaStride) >> 1) << 1);
    middleSquareDimensions = width % (isWhiteAreaStride << 1); // remainder of the above square division.
    // printf("NumFullStridesX %d NumFullStridesY %d middleSquareDimensions %d \n", numFullStridesX, numFullStridesY, middleSquareDimensions);
    isWhiteArea = calloc((numFullStridesX + 1) * (numFullStridesY + 1), sizeof(bool));
    unsigned max_square_width = isWhiteAreaStride < middleSquareDimensions ? middleSquareDimensions : isWhiteAreaStride;
    temp_square = (unsigned char*)malloc(max_square_width * max_square_width * sizeof(unsigned char)*3);
    populateIsWhiteArea(buffer_frame, width, height);
}
#endif
/* End White Pixel Optimization Structures */


/* Helper Functions */
void blankSquare(unsigned char *buffer_frame, unsigned buffer_width, unsigned pxx, unsigned pxy, unsigned blank_width, unsigned blank_height) {
    // printf("Blanking square (%d, %d) %dx%d\n", pxx, pxy, blank_width, blank_height);
    unsigned const blank_width_3 = blank_width * 3;
    unsigned const buffer_width_3 = buffer_width * 3;
    unsigned char *target = buffer_frame + buffer_width_3 * pxy + pxx * 3;
    int row;
    for (row = 0; row < blank_height; ++row) {
        // set row to 255,255,255...
        memset(target, 255, blank_width_3);
        // move down a row
        target += buffer_width_3;
    }
}

// this should only be called with relatively small (20x20) areas
// also src and dst should not overlap
void moveRectInlineRotate90CCW(unsigned char* buffer_frame, unsigned buffer_width, unsigned src_pxx, unsigned src_pxy, unsigned dst_pxx, unsigned dst_pxy, unsigned cpy_width, unsigned cpy_height) {
    // printf("Moving from (%d,%d) to (%d,%d) size: %d x %d with 90 deg CCW rotation\n", src_pxx, src_pxy, dst_pxx, dst_pxy, cpy_width, cpy_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    unsigned char *src_base = buffer_frame + buffer_width_3 * src_pxy + src_pxx * 3; // first row first column
    unsigned char *dst_base = buffer_frame + buffer_width_3 * (dst_pxy + cpy_width - 1) + dst_pxx * 3; // last row first column
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    int src_row, src_col;
    for (src_row = 0; src_row < cpy_height; ++src_row) {
        for (src_col = 0; src_col < cpy_width; ++src_col) {
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
    int src_row, src_col;
    for (src_col = 0; src_col < cpy_height; ++src_col) {
        for (src_row = 0; src_row < cpy_width; ++src_row) {
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
    int src_row, src_col;
    for (src_row = 0; src_row < cpy_height; ++src_row) {
        for (src_col = 0; src_col < cpy_width; ++src_col) {
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
    int row, col;
    for (row = 0; row < temp_height; ++row) {
        for (col = 0; col < temp_width; ++col) {
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
    int row, col;
    for (row = 0; row < temp_height; ++row) {
        for (col = 0; col < temp_width; ++col) {
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
    // printf("90CCW temp to buff location (%d, %d), %dx%d\n", dst_pxx, dst_pxy, temp_width, temp_height);
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned temp_width_3 = temp_width * 3;

    unsigned char *src_base = temp; // top row, first col
    unsigned char *dst_base = buffer_frame + buffer_width_3 * (dst_pxy + temp_width - 1) + dst_pxx * 3; // bottom row, first col
    unsigned char *src = src_base;
    unsigned char *dst = dst_base;
    int row, col;
    for (row = 0; row < temp_height; ++row) {
        for (col = 0; col < temp_width; ++col) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst -= buffer_width_3; // move up 1 row
            src += 3; // move right 1 column
        }
        // move dst col right, bottom row
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
    int row;
    for (row = 0; row < cpy_height; ++row) {
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
    int i;
    for (i = 0; i < temp_height; ++i) {
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
    int i;
    for (i = 0; i < subsquare_height; ++i) {
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
    int i, j;
    for (i = 0; i < subsquare_height; ++i) {
        // move to first element of next row down
        ll_index = left_left_pxindex_3 + left_row_offset;
        // move to last element of next row down
        rr_index = right_right_pxindex_3 + right_row_offset;
        for (j = 0; j < subsquare_width; ++j) {
            // swap red values of left and right, in mirrored x order
            // printf("\tSwapping red values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index];
            buffer_frame[ll_index] = buffer_frame[rr_index];
            buffer_frame[rr_index] = temp;
            // swap blue values of left and right, in mirrored x order
            // printf("\tSwapping blue values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index+1];
            buffer_frame[ll_index+1] = buffer_frame[rr_index+1];
            buffer_frame[rr_index+1] = temp;
            // swap green values of left and right, in mirrored x order
            // printf("\tSwapping green values located at %d(%d) and %d(%d)\n", ll_index, buffer_frame[ll_index], rr_index, buffer_frame[rr_index]);
            temp = buffer_frame[ll_index+2];
            buffer_frame[ll_index+2] = buffer_frame[rr_index+2];
            buffer_frame[rr_index+2] = temp;

            rr_index -= 3; // move to the red value of the previous pixel on the right side frame
            ll_index += 3; // move to the red value of the next pixel
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
    // printf("Up %d\n", offset);
    #ifndef USE_TRANSLATEOPTS
    unsigned char *ret = processMoveUpReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    updateIsWhiteAreaUpOffset(buffer_frame, width, offset);
    // populateIsWhiteArea(buffer_frame, width, height);
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
    updateIsWhiteAreaUpOffset(buffer_frame, width, offset);
    // populateIsWhiteArea(buffer_frame, width, height);
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
    unsigned wsArrayDimensions = numFullStridesX + (middleSquareDimensions != 0);
    #ifndef USE_TRANSLATEOPTS

    unsigned char *ret = processMoveRightReference(buffer_frame, width, height, offset);
    #ifdef USE_ISWHITEAREA
    updateIsWhiteAreaRightOffset(buffer_frame, width, offset);
    // populateIsWhiteArea(buffer_frame, width, height);
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
    updateIsWhiteAreaRightOffset(buffer_frame, width, offset);
    // populateIsWhiteArea(buffer_frame, width, height);
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
    updateIsWhiteAreaDownOffset(buffer_frame, width, offset);
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
    updateIsWhiteAreaDownOffset(buffer_frame, width, offset);
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
    updateIsWhiteAreaLeftOffset(buffer_frame, width, offset);
    #endif
    return ret;

    #else

    unsigned const wsDimensions = numFullStridesX + (middleSquareDimensions != 0);
    int const widthTriple = 3 * width;
    int const offsetTriple = 3 * offset; 
    int const shiftedTriple = widthTriple - offsetTriple; // the amount of pixels in RGB that actually get shifted
    unsigned char *rowBegin = buffer_frame;


    unsigned ws_index_base = wsDimensions * wsDimensions - 1;
    unsigned ws_index = ws_index_base;
    unsigned new_height = height;
    int ws_row, ws_col;
    unsigned unused, px_y;
    for (ws_row = wsDimensions - 1; ws_row >= 0; --ws_row) {
        for (ws_col = 0; ws_col < wsDimensions; ++ws_col) {
            if (!isWhiteArea[ws_index]) {
                // printf("Old Height: %d ", height);
                translateWhiteSpaceArrayIndicesToPixel(0, ws_row+1, wsDimensions, &unused, &new_height);
                // printf("New Height: %d\n", new_height);
                goto found_height;
            }
            --ws_index;
        }
        ws_index_base -= wsDimensions;
        ws_index = ws_index_base;
    }
found_height:
    ws_index = 0;
    ws_index_base = 0;
    for (ws_row = 0; ws_row < wsDimensions; ++ws_row) {
        for (ws_col = 0; ws_col < wsDimensions; ++ws_col) {
            if (!isWhiteArea[ws_index]) {
                translateWhiteSpaceArrayIndicesToPixel(0, ws_row, wsDimensions, &unused, &px_y);
                rowBegin = buffer_frame + px_y * widthTriple;
                long height_limit = new_height - px_y;
                // printf("Moving %d rows instead of %d rows\n", height_limit, height);
                for (ws_row = 0; ws_row < height_limit; ws_row++){
                    memmove(rowBegin, rowBegin + offsetTriple, shiftedTriple);
                    memset(rowBegin + shiftedTriple, 255, offsetTriple);
                    rowBegin += widthTriple;
                }
                #ifdef USE_ISWHITEAREA
                updateIsWhiteAreaLeftOffset(buffer_frame, width, offset);
                #endif
                return buffer_frame;
            }
            ++ws_index;
        }
        ws_index_base += wsDimensions;
        ws_index = ws_index_base;
    }
    

    /*
    for (int row = 0; row < height; row++){
      memmove(rowBegin, rowBegin + offsetTriple, shiftedTriple);
      memset(rowBegin + shiftedTriple, 255, offsetTriple);
      rowBegin += widthTriple;
    }

    //return a pointer to the updated image buffer
    #ifdef USE_ISWHITEAREA
    updateIsWhiteAreaLeftOffset(buffer_frame, width, offset);
    #endif
    return buffer_frame;
    */

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
    int const whiteSpaceArrayHeight_div_2 = whiteSpaceArrayHeight >> 1;
    int const whiteSpaceArrayWidth_div_2 = whiteSpaceArrayWidth >> 1;
    unsigned tl_px_x, tl_px_y;
    unsigned tr_px_x, tr_px_y;
    unsigned bl_px_x, bl_px_y;
    unsigned br_px_x, br_px_y;
    if (rotate_iteration == 1) {
        for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
            for (int col = 0; col < whiteSpaceArrayWidth_div_2; ++col) {
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
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && !TR && BR && !BL) writeRot90 BR into BL, blank BR
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && !BR && !BL) writeRot90 TR into BR, blank TR
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && !BR && !BL) writeRot90 TL into TR, blank TL
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && !TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, blank BR
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 BL into TL, blank BL, blank TR
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && !BR && BL) writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && BR && !BL) writeRot90 TL into TR, writeRot90 BR into BL, blank TL, blank BR
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && !BR && !BL) writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (TL && !TR && BR && BL) writeRot90 TL into TR, BL into TL, BR into BL, blank BR
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, tl_px_x, tl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // else if (!TL && TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bl_px_x, bl_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, br_px_x, br_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, tr_px_x, tr_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // if (TL && TR && BR && BL) write BL into temp buffer, writeRot90 BR into BL, TR into BR, TL into TR, temp into TL
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

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
            int col = whiteSpaceArrayWidth_div_2;
            unsigned top_px_x, top_px_y;
            unsigned right_px_x, right_px_y;
            unsigned bottom_px_x, bottom_px_y;
            unsigned left_px_x, left_px_y;
            for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
                int top_col_index = row * whiteSpaceArrayWidth + col;
                int left_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + row;
                int right_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
                int bottom_col_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + col;
                int condition_hash = (!isWhiteArea[top_col_index] << 3) +
                                     (!isWhiteArea[right_row_index] << 2) +
                                     (!isWhiteArea[bottom_col_index] << 1) +
                                     (!isWhiteArea[left_row_index]);
                // if (condition_hash != 0) printf("Rotating Middle Col WS square (%d, %d), hash: %d\n", col, row, condition_hash);
                switch (condition_hash) {
                    case 0b0000: continue; break;
                    case 0b0001: // left into top, blank left
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // bottom into left, blank bottom
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, blank right
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top into right, blank top
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // left into top, bottom into left, blank bottom
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, left into top, blank right blank left
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // bottom into left, right into bottom, blank right
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top in to right, left into top, blank left
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top in to right, bottom into left, blank top blank bottom
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, top into right, blank top
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // bottom into left, right into bottom, top into right, blank top
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // right into bottom, top into right, left into top, blank left
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // top into right, left into top, bottom into left, blank bottom
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, top_px_x, top_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    // left into top, bottom into left, right into bottom, blank right
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate90CW(buffer_frame, width, left_px_x, left_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate90CW(buffer_frame, width, bottom_px_x, bottom_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate90CW(buffer_frame, width, right_px_x, right_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    // right to temp, top to right, left to top, bottom to left, temp to bottom
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

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
            if (!isWhiteArea[whiteSpaceArrayWidth * (whiteSpaceArrayHeight_div_2) + (whiteSpaceArrayWidth_div_2)]) {
                translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth_div_2, whiteSpaceArrayHeight_div_2, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y); // reusing tl_px_* for middle square
                moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
                moveTempToBufferRotate90CW(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
            }
            // no whitespace array update required
        }
    } else { // CCW/CW 180 deg
        for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
            for (int col = 0; col < whiteSpaceArrayWidth_div_2; ++col) {
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
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, blank BR
                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into BL, blank TR
                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, blank TL
                    case 0b1000:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, BL into TR, blank BR, blank BL
                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into temp, BL into TR, temp into BL
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);
                        // TODO write inline swap 180
                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TR into BL, BR into TL, blank TR, blank BR
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, BL into TR, blank TL, blank BL
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into temp, TL into BR, temp into TL
                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, TR into BL, blank TL, blank TR
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, TR into BL, blank TR
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into BR, blank TL, TR into temp, BL into TR, temp into BL
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectInlineRotate180(buffer_frame, width, tl_px_x, tl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, BL into TR, blank BL
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, bl_px_x, bl_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // BR into TL, blank BR, BL into temp, TR into BL, temp into TR
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, tr_px_x, tr_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, br_px_x, br_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                        break;

                    // TL into temp, BR into TL, temp into BR, TR into temp, BL into TR, temp into BL
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

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
            int col = whiteSpaceArrayWidth_div_2;
            unsigned top_px_x, top_px_y;
            unsigned right_px_x, right_px_y;
            unsigned bottom_px_x, bottom_px_y;
            unsigned left_px_x, left_px_y;
            for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
                int top_col_index = row * whiteSpaceArrayWidth + col;
                int left_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + row;
                int right_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
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
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    case 0b0010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    case 0b1010:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        break;

                    case 0b0100:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                        
                    case 0b0001:
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0101:
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;

                    case 0b0011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1001:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1100:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1110:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1101:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectInlineRotate180(buffer_frame, width, top_px_x, top_px_y, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1011:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectToTemp(buffer_frame, temp_square, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectInlineRotate180(buffer_frame, width, left_px_x, left_px_y, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b0111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                        moveRectInlineRotate180(buffer_frame, width, bottom_px_x, bottom_px_y, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                        blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);

                        moveRectToTemp(buffer_frame, temp_square, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveRectInlineRotate180(buffer_frame, width, right_px_x, right_px_y, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                        moveTempToBufferRotate180(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                        break;
                    case 0b1111:
                        translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                        translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

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
            if (!isWhiteArea[whiteSpaceArrayWidth * (whiteSpaceArrayHeight_div_2) + (whiteSpaceArrayWidth_div_2)]) {
                translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth_div_2, whiteSpaceArrayHeight_div_2, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y); // reusing tl_px_* for middle square
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
    #ifndef USE_ISWHITEAREA
    return processRotateCCWReference(buffer_frame, width, height, rotate_iteration);
    #else
    // our condenser limits this to be a 90 deg CCW rotation
    int const whiteSpaceArrayWidth = numFullStridesX + (middleSquareDimensions != 0);
    int const whiteSpaceArrayHeight = whiteSpaceArrayWidth;
    int const whiteSpaceArrayHeight_div_2 = (whiteSpaceArrayHeight >> 1);
    int const whiteSpaceArrayWidth_div_2 = (whiteSpaceArrayWidth >> 1);
    unsigned tl_px_x, tl_px_y;
    unsigned tr_px_x, tr_px_y;
    unsigned bl_px_x, bl_px_y;
    unsigned br_px_x, br_px_y;
    for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
        for (int col = 0; col < whiteSpaceArrayWidth_div_2; ++col) {
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
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && !TR && BR && !BL) writeRot90 BR into BL, blank BR
                case 0b0010:
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && TR && !BR && !BL) writeRot90 TR into BR, blank TR
                case 0b0100:
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && !TR && !BR && !BL) writeRot90 TL into TR, blank TL
                case 0b1000:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && !TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, blank BR
                case 0b0011:
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 BL into TL, blank BL, blank TR
                case 0b0101:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                case 0b0110:
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && !TR && !BR && BL) writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                case 0b1001:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && !TR && BR && !BL) writeRot90 TL into TR, writeRot90 BR into BL, blank TL, blank BR
                case 0b1010:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && TR && !BR && !BL) writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                case 0b1100:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && TR && BR && !BL) writeRot90 BR into BL, writeRot90 TR into BR, writeRot90 TL into TR, blank TL
                case 0b1110:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && TR && !BR && BL) writeRot90 TR into BR, writeRot90 TL into TR, writeRot90 BL into TL, blank BL
                case 0b1101:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (TL && !TR && BR && BL) writeRot90 TL into TR, BL into TL, BR into BL, blank BR
                case 0b1011:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x, tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // else if (!TL && TR && BR && BL) writeRot90 BL into TL, writeRot90 BR into BL, writeRot90 TR into BR, blank TR
                case 0b0111:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, tr_px_x, tr_px_y, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y, br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride);
                    break;

                // if (TL && TR && BR && BL) write BL into temp buffer, writeRot90 BR into BL, TR into BR, TL into TR, temp into TL
                case 0b1111:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - row - 1, col, whiteSpaceArrayWidth, &tr_px_x, &tr_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - col - 1, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &br_px_x, &br_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, whiteSpaceArrayHeight - 1 - col, whiteSpaceArrayWidth, &bl_px_x, &bl_px_y);

                    moveRectToTemp(buffer_frame, temp_square, width, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride); // TR to temp
                    moveRectInlineRotate90CCW(buffer_frame, width, br_px_x, br_px_y, tr_px_x, tr_px_y, isWhiteAreaStride, isWhiteAreaStride); // BR to BL
                    moveRectInlineRotate90CCW(buffer_frame, width, bl_px_x, bl_px_y,  br_px_x, br_px_y, isWhiteAreaStride, isWhiteAreaStride); // BL to TL
                    moveRectInlineRotate90CCW(buffer_frame, width, tl_px_x,  tl_px_y, bl_px_x, bl_px_y, isWhiteAreaStride, isWhiteAreaStride); // TL to TR
                    moveTempToBufferRotate90CCW(buffer_frame, temp_square, width, tl_px_x, tl_px_y, isWhiteAreaStride, isWhiteAreaStride); // temp to BR
                    break;
            }
            // rotate white space array
            bool temp_bool = isWhiteArea[TL_index];
            isWhiteArea[TL_index] = isWhiteArea[TR_index];
            isWhiteArea[TR_index] = isWhiteArea[BR_index];
            isWhiteArea[BR_index] = isWhiteArea[BL_index];
            isWhiteArea[BL_index] = temp_bool;
        }
    }
    if (middleSquareDimensions != 0) {
        // middle columns and rows
        // printf("processing middle cols and rows\n");
        int col = whiteSpaceArrayWidth_div_2;
        unsigned top_px_x, top_px_y;
        unsigned right_px_x, right_px_y;
        unsigned bottom_px_x, bottom_px_y;
        unsigned left_px_x, left_px_y;
        for (int row = 0; row < whiteSpaceArrayHeight_div_2; ++row) {
            int top_col_index = row * whiteSpaceArrayWidth + col;
            int left_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + row;
            int right_row_index = (whiteSpaceArrayHeight_div_2) * whiteSpaceArrayWidth + (whiteSpaceArrayWidth - row - 1);
            int bottom_col_index = (whiteSpaceArrayHeight - 1 - row) * whiteSpaceArrayWidth + col;
            int condition_hash = (!isWhiteArea[top_col_index] << 3) +
                                    (!isWhiteArea[right_row_index] << 2) +
                                    (!isWhiteArea[bottom_col_index] << 1) +
                                    (!isWhiteArea[left_row_index]);
            // if (condition_hash != 0) printf("Rotating Middle Col WS square (%d, %d), hash: %d\n", col, row, condition_hash);
            switch (condition_hash) {
                case 0b0000: continue; break;
                case 0b0001: // left into bottom, blank left
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // bottom into right, blank bottom
                case 0b0010:
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // right into top, blank right
                case 0b0100:
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // top into left, blank top
                case 0b1000:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // bottom into right, left into bottom, blank left
                case 0b0011:
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // right into top, left into bottom, blank right blank left
                case 0b0101:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // right into top, bottom into right, blank bottom
                case 0b0110:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // left into bottom, top in to left, blank top
                case 0b1001:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // top in to left, bottom into right, blank top blank bottom
                case 0b1010:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // top into left, right into top, blank right
                case 0b1100:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // top into left, right into top, bottom into right, blank bottom
                case 0b1110:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, bottom_px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // left into bottom, top into left, right into top, blank right
                case 0b1101:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // bottom into right, left into bottom, top into left, blank top
                case 0b1011:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x, top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride);
                    blankSquare(buffer_frame, width, top_px_x, top_px_y, middleSquareDimensions, isWhiteAreaStride);
                    break;

                // right into top, bottom into right, left into bottom, blank left
                case 0b0111:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectInlineRotate90CCW(buffer_frame, width, right_px_x, right_px_y, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions);
                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride);
                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y, bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions);
                    blankSquare(buffer_frame, width, left_px_x, left_px_y, isWhiteAreaStride, middleSquareDimensions);
                    break;

                // TODO left off here
                // right to temp, bottom to right, left to bottom, top to left, temp to top
                case 0b1111:
                    translateWhiteSpaceArrayIndicesToPixel(col, row, whiteSpaceArrayWidth, &top_px_x, &top_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth - 1 - row, col, whiteSpaceArrayWidth, &right_px_x, &right_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(col, whiteSpaceArrayHeight - row - 1, whiteSpaceArrayWidth, &bottom_px_x, &bottom_px_y);
                    translateWhiteSpaceArrayIndicesToPixel(row, col, whiteSpaceArrayWidth, &left_px_x, &left_px_y);

                    moveRectToTemp(buffer_frame, temp_square, width, right_px_x, right_px_y, isWhiteAreaStride, middleSquareDimensions); // Right to temp
                    moveRectInlineRotate90CCW(buffer_frame, width, bottom_px_x, bottom_px_y, right_px_x, right_px_y, middleSquareDimensions, isWhiteAreaStride); // BR to Bottom
                    moveRectInlineRotate90CCW(buffer_frame, width, left_px_x, left_px_y,  bottom_px_x, bottom_px_y, isWhiteAreaStride, middleSquareDimensions); // Bottom to Top
                    moveRectInlineRotate90CCW(buffer_frame, width, top_px_x,  top_px_y, left_px_x, left_px_y, middleSquareDimensions, isWhiteAreaStride); // Top to Right
                    moveTempToBufferRotate90CCW(buffer_frame, temp_square, width, top_px_x, top_px_y, isWhiteAreaStride, middleSquareDimensions); // temp to BR
                    break;
            }                

            // Update whitespace array
            bool temp_bool = isWhiteArea[top_col_index];
            isWhiteArea[top_col_index] = isWhiteArea[right_row_index];
            isWhiteArea[right_row_index] = isWhiteArea[bottom_col_index];
            isWhiteArea[bottom_col_index] = isWhiteArea[left_row_index];
            isWhiteArea[left_row_index] = temp_bool;

            top_col_index += whiteSpaceArrayWidth; // down 1 row
            bottom_col_index -= whiteSpaceArrayWidth; // up 1 row
            --right_row_index; // left one column
            ++left_row_index; // right one column

        }
        if (!isWhiteArea[whiteSpaceArrayWidth * (whiteSpaceArrayHeight_div_2) + (whiteSpaceArrayWidth_div_2)]) {
            translateWhiteSpaceArrayIndicesToPixel(whiteSpaceArrayWidth_div_2, whiteSpaceArrayHeight_div_2, whiteSpaceArrayWidth, &tl_px_x, &tl_px_y); // reusing tl_px_* for middle square
            moveRectToTemp(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
            moveTempToBufferRotate90CCW(buffer_frame, temp_square, width, tl_px_x, tl_px_y, middleSquareDimensions, middleSquareDimensions);
        }
        // no whitespace array update required
    }
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
unsigned char *processMirrorX(unsigned char *buffer_frame, unsigned int width, unsigned int height, int _unused) {
    #ifndef USE_MIRROROPTS
    return processMirrorXReference(buffer_frame, width, height, _unused);
    #else
    #ifndef USE_ISWHITEAREA
    swapAndMirrorXSubsquares(buffer_frame, width, 0, 0, 0, height-1, width, height/2);
    // TODO: once everything is done in-place, wont need this return anymore
    return buffer_frame;
    #else
    int const whiteSpaceArrayWidth = numFullStridesX + (middleSquareDimensions != 0);
    int const whiteSpaceArrayHeight = whiteSpaceArrayWidth; // TODO we know its always square, we can remove this and just use Width
    int const numFullStridesY_div_2 = numFullStridesY >> 1;
    int const numFullStridesX_div_2 = numFullStridesX >> 1;

    int ws_row, ws_col;

    int ws_top_index_base = 0, ws_top_index = 0;
    int ws_bottom_index_base = whiteSpaceArrayWidth * (whiteSpaceArrayHeight - 1);
    int ws_bottom_index = ws_bottom_index_base;
    int temp_bool;

    int px_x = 0; // same for top and bottom
    int top_px_y = 0; // top left of this square
    int bottom_px_y = height - 1; // bottom left of this square
    // top and bottom left quadrants
    for (ws_row = 0; ws_row < numFullStridesY_div_2; ++ws_row) {
        for (ws_col = 0; ws_col < numFullStridesX_div_2; ++ws_col) {
            if (!isWhiteArea[ws_top_index] || !isWhiteArea[ws_bottom_index]) {
                swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, isWhiteAreaStride, isWhiteAreaStride);
                temp_bool = isWhiteArea[ws_top_index];
                isWhiteArea[ws_top_index] = isWhiteArea[ws_bottom_index];
                isWhiteArea[ws_bottom_index] = temp_bool;
            }
            ++ws_top_index;
            ++ws_bottom_index;
            px_x += isWhiteAreaStride; // right one col
        }

        ws_top_index_base += whiteSpaceArrayWidth;
        ws_top_index = ws_top_index_base;
        ws_bottom_index_base -= whiteSpaceArrayWidth;
        ws_bottom_index = ws_bottom_index_base;

        px_x = 0; // first col
        top_px_y += isWhiteAreaStride; // down a stride
        bottom_px_y -= isWhiteAreaStride; // up a stride
    }
    // top and bottom right quadrants
    ws_top_index_base = numFullStridesX_div_2 + (middleSquareDimensions != 0);
    ws_top_index = ws_top_index_base;
    ws_bottom_index_base = whiteSpaceArrayWidth * (whiteSpaceArrayHeight - 1) + ws_top_index_base;
    ws_bottom_index = ws_bottom_index_base;

    int px_x_base = numFullStridesX_div_2 * isWhiteAreaStride + middleSquareDimensions;
    px_x = px_x_base;
    top_px_y = 0;
    bottom_px_y = height - 1;
    for (ws_row = 0; ws_row < numFullStridesY_div_2; ++ws_row) {
        for (ws_col = numFullStridesX_div_2 + (middleSquareDimensions != 0); ws_col < whiteSpaceArrayWidth; ++ws_col) {
            if (!isWhiteArea[ws_top_index] || !isWhiteArea[ws_bottom_index]) {
                swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, isWhiteAreaStride, isWhiteAreaStride);
                temp_bool = isWhiteArea[ws_top_index];
                isWhiteArea[ws_top_index] = isWhiteArea[ws_bottom_index];
                isWhiteArea[ws_bottom_index] = temp_bool;
            }
            ++ws_top_index;
            ++ws_bottom_index;
            px_x += isWhiteAreaStride; // right one col
        }

        ws_top_index_base += whiteSpaceArrayWidth; // down a row, first col on right
        ws_top_index = ws_top_index_base;
        ws_bottom_index_base -= whiteSpaceArrayWidth; // up a row, first col on right
        ws_bottom_index = ws_bottom_index_base;

        px_x = px_x_base; // first col on right side
        top_px_y += isWhiteAreaStride; // down a stride
        bottom_px_y -= isWhiteAreaStride; // up a stride
    }
    if (middleSquareDimensions != 0) {
        // middle column special case
        ws_col = numFullStridesX_div_2;
        top_px_y = 0;
        bottom_px_y = height - 1;
        px_x = ws_col * isWhiteAreaStride;
        ws_top_index = numFullStridesX_div_2;
        ws_bottom_index = whiteSpaceArrayWidth * (whiteSpaceArrayHeight - 1) + numFullStridesX_div_2;
        for (ws_row = 0; ws_row < numFullStridesY_div_2; ++ws_row) {
            if (!isWhiteArea[ws_top_index] || !isWhiteArea[ws_bottom_index]) {
                swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, middleSquareDimensions, isWhiteAreaStride);
                temp_bool = isWhiteArea[ws_top_index];
                isWhiteArea[ws_top_index] = isWhiteArea[ws_bottom_index];
                isWhiteArea[ws_bottom_index] = temp_bool;
            }
            ws_top_index += whiteSpaceArrayWidth;
            ws_bottom_index -= whiteSpaceArrayWidth;
            top_px_y += isWhiteAreaStride;
            bottom_px_y -= isWhiteAreaStride;
        }
        // TODO: middle row special case, split row in half to do it inline
        top_px_y = isWhiteAreaStride * numFullStridesY_div_2;
        bottom_px_y = top_px_y + middleSquareDimensions - 1;
        px_x = 0;
        ws_top_index = numFullStridesY_div_2 * whiteSpaceArrayWidth;
        unsigned middleColHeight_div_2 = middleSquareDimensions >> 1;
        for (ws_col = 0; ws_col < numFullStridesX_div_2; ++ws_col) {
            if (!isWhiteArea[ws_top_index]){
                swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, isWhiteAreaStride, middleColHeight_div_2);
            }
            ws_top_index++;
            px_x += isWhiteAreaStride;
            // no swaps required
        }
        // TODO: middle square special case, split in half along x axis to do it inline
        if (!isWhiteArea[ws_top_index]) {
            swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, middleSquareDimensions, middleColHeight_div_2);
            // no whitespace update required
        }
        ws_top_index++;
        px_x += middleSquareDimensions;
        for (ws_col = numFullStridesX_div_2 + 1; ws_col < whiteSpaceArrayWidth; ++ws_col) {
            if (!isWhiteArea[ws_top_index]){
                swapAndMirrorXSubsquares(buffer_frame, width, px_x, top_px_y, px_x, bottom_px_y, isWhiteAreaStride, middleColHeight_div_2);
            }
            ws_top_index++;
            px_x += isWhiteAreaStride;
            // no swaps required
        }
    }
    return buffer_frame;
    #endif
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
        #ifndef USE_ISWHITEAREA
        // TODO: only swap and mirror non-white squares.
        swapAndMirrorYSubsquares(buffer_frame, width, 0, 0, width-1, 0, width/2, height);
        // TODO: once everything is done in-place, wont need this return anymore
        return buffer_frame;
        #else
        swapAndMirrorYSubsquares(buffer_frame, width, 0, 0, width-1, 0, width/2, height);
        populateIsWhiteArea(buffer_frame, width, height);
        return buffer_frame;
        #endif
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
