#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line

// Configuration parameters, uncomment them to turn them on
#define USE_ISWHITEARRAY

// TODO: can be removed once everything is done in-place
unsigned char *rendered_frame;

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
    total_clockwise_rotation = (total_clockwise_rotation % 4);
    // ecr is between -3 and +3
    // if ecr is 3 or -1, ccw
    // else if ecr != 0, cw
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
/*
 * @param 
 */
optimized_kv* collapse_sensor_values(struct kv *sensor_values, int sensor_values_count, int *new_sensor_value_count) {
    int new_count = 0;
    char *sensor_value_key;
    movement_type type;
    struct kv *sensor_value;
    optimized_kv *collapsed_sensor_values = (optimized_kv*)malloc(sizeof(optimized_kv) * sensor_values_count);
    int i = 0;
    while (i < sensor_values_count) {
        // Collapse translations
        int total_up_movement = 0;
        int total_right_movement = 0;
        while (i < sensor_values_count) {
            sensor_value = &sensor_values[i];
            sensor_value_key = sensor_value->key;
            type = sensor_value_key[0] + sensor_value_key[1];
            switch (type) {
                case W: total_up_movement += sensor_value->value; break;
                case S: total_up_movement -= sensor_value->value; break;
                case D: total_right_movement += sensor_value->value; break;
                case A: total_right_movement -= sensor_value->value; break;
                default: goto not_translation_1;
            }
            ++i;
            if (i % 25 == 0) {
                new_count = insert_translation_frames(collapsed_sensor_values, new_count, total_up_movement, total_right_movement);
                total_up_movement = 0;
                total_right_movement = 0;
                collapsed_sensor_values[new_count].type = FRAME_BREAK;
                collapsed_sensor_values[new_count].value = 0;
                ++new_count;
            }
        }
    not_translation_1:
        new_count = insert_translation_frames(collapsed_sensor_values, new_count, total_up_movement, total_right_movement);


        // Collapse rotations
        int total_clockwise_rotation = 0;
        while (i < sensor_values_count) {
            sensor_value = &sensor_values[i];
            sensor_value_key = sensor_value->key;
            type = sensor_value_key[0] + sensor_value_key[1];
            switch (type) {
                case CW: total_clockwise_rotation += sensor_value->value; break;
                case CCW: total_clockwise_rotation -= sensor_value->value; break;
                default: goto not_rotation_1;
            }
            ++i;
            if (i % 25 == 0) {
                new_count = insert_rotation_frames(collapsed_sensor_values, new_count, total_clockwise_rotation);
                total_clockwise_rotation = 0;
                collapsed_sensor_values[new_count].type = FRAME_BREAK;
                collapsed_sensor_values[new_count].value = 0;
                ++new_count;
            }
        }

    not_rotation_1:
        new_count = insert_rotation_frames(collapsed_sensor_values, new_count, total_clockwise_rotation);


        // Collapse mirroring
        bool is_X_mirrored = 0;
        bool is_Y_mirrored = 0;
        while (i < sensor_values_count) {
            sensor_value = &sensor_values[i];
            sensor_value_key = sensor_value->key;
            type = sensor_value_key[0] + sensor_value_key[1];
            switch (type) {
                case MX: is_X_mirrored = !is_X_mirrored; break;
                case MY: is_Y_mirrored = !is_Y_mirrored; break;
                default: goto not_mirroring_1;
            }
            ++i;
            if (i % 25 == 0) {
                new_count = insert_mirror_frames(collapsed_sensor_values, new_count, is_X_mirrored, is_Y_mirrored);
                is_X_mirrored = false;
                is_Y_mirrored = false;
                collapsed_sensor_values[new_count].type = FRAME_BREAK;
                collapsed_sensor_values[new_count].value = 0;
                ++new_count;
            }
        }
    not_mirroring_1:
        new_count = insert_mirror_frames(collapsed_sensor_values, new_count, is_X_mirrored, is_Y_mirrored);

    }

    int second_pass_count = 0;
    i = 0;
    optimized_kv *second_pass_collapsed_values = (optimized_kv *)malloc(sizeof(optimized_kv) * new_count);
    while (i < new_count) {
        int total_up_movement = 0;
        int total_right_movement = 0;
        while (i < new_count) {
            switch (collapsed_sensor_values[i].type) {
                case W: total_up_movement += collapsed_sensor_values[i].value; break;
                case A: total_right_movement -= collapsed_sensor_values[i].value; break;
                case S: total_up_movement -= collapsed_sensor_values[i].value; break;
                case D: total_right_movement += collapsed_sensor_values[i].value; break;
                default: goto not_translating_2;
            }
            ++i;
        }
    not_translating_2:
        second_pass_count = insert_translation_frames(second_pass_collapsed_values, second_pass_count, total_up_movement, total_right_movement);

        int total_clockwise_rotation = 0;
        while (i < new_count) {
            switch(collapsed_sensor_values[i].type) {
                case CW: total_clockwise_rotation += collapsed_sensor_values[i].value; break;
                case CCW: total_clockwise_rotation -= collapsed_sensor_values[i].value; break;
                default: goto not_rotating_2;
            }
            ++i;
        }
    not_rotating_2:
        second_pass_count = insert_rotation_frames(second_pass_collapsed_values, second_pass_count, total_clockwise_rotation);

        bool is_X_mirrored = false;
        bool is_Y_mirrored = false;
        while (i < new_count) {
            switch (collapsed_sensor_values[i].type) {
            case MX: is_X_mirrored = !is_X_mirrored; break;
            case MY: is_Y_mirrored = !is_Y_mirrored; break;
            default: goto not_mirrored_2;
            }
            ++i;
        }
    not_mirrored_2:
        second_pass_count = insert_mirror_frames(second_pass_collapsed_values, second_pass_count, is_X_mirrored, is_Y_mirrored);

        while (i < new_count && collapsed_sensor_values[i].type == FRAME_BREAK) {
            second_pass_collapsed_values[second_pass_count].type = FRAME_BREAK;
            second_pass_collapsed_values[second_pass_count].value = 0;
            ++second_pass_count;
            ++i;
        }
    }
    free(collapsed_sensor_values);


    *new_sensor_value_count = second_pass_count;
    return second_pass_collapsed_values;
}
/* END SENSOR COLLAPSING CODE */


/* White Pixel Optimization Structures */

/* Use this structure as follows
 *
 * if isWhiteArea[i,j] is true, then the square with corners at (isWhiteAreaStride * i, isWhiteAreaStride * j, isWhiteAreaStride * (i + 1) - 1, isWhiteAreaStride * (j + 1) - 1)
 * contains only white pixels and can be optimized as such
 */

#ifdef USE_ISWHITEARRAY
#define isWhiteAreaStride 21 // translates to 64 bytes, each pixel is 3 bytes
bool *isWhiteArea;
unsigned int numFullStridesX;
unsigned int numFullStridesY;
unsigned int middleSquareDimensions;
bool checkWhiteAreaSquare(unsigned char *buffer_frame, unsigned buffer_pxwidth, unsigned whiteAreaCol, unsigned whiteAreaRow, unsigned pxWidth, unsigned pxHeight) {
    unsigned row_offset = whiteAreaRow * isWhiteAreaStride * buffer_pxwidth * 3;
    unsigned col_offset = whiteAreaCol * isWhiteAreaStride * 3;
    for (int row = 0; row < pxHeight; ++row) {
        for (int col = 0; col < pxWidth; ++col) {
            int pixel_index = row_offset + col_offset + row * buffer_pxwidth * 3 + col * 3;
            if (buffer_frame[pixel_index] != 255 ||
                buffer_frame[pixel_index + 1] != 255 ||
                buffer_frame[pixel_index + 2] != 255)
            {
                return true;
            }
        }
    }
    return false;
}

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
void translateWhiteSpaceArrayIndicesToPixel(unsigned ws_x, unsigned ws_y, unsigned ws_width, unsigned *px_x_out, unsigned *px_y_out) {
    if (middleSquareDimensions != 0) {
        *px_x_out = ws_x * isWhiteAreaStride + (ws_x >= numFullStridesX / 2 ? middleSquareDimensions - isWhiteAreaStride : 0);
        *px_y_out = ws_y * isWhiteAreaStride + (ws_y >= numFullStridesY / 2 ? middleSquareDimensions - isWhiteAreaStride : 0);
    } else {
        *px_x_out = ws_x * isWhiteAreaStride;
        *px_y_out = ws_y * isWhiteAreaStride;
    }
}

void populateIsWhiteArea(unsigned char *buffer_frame, unsigned width, unsigned height) {
    const bool hasMiddleSquare = (middleSquareDimensions != 0);
    unsigned int boolArrayWidth = numFullStridesX + (middleSquareDimensions != 0);
    unsigned int boolArrayHeight = numFullStridesY + (middleSquareDimensions != 0);

    // 500x500 image = 22 21x21 squares with a 38x38 middle square, 38x21 middle column and 21x38 middle row
    // |11 21-wide squares|38 wide square|11 21-wide squares|

    int whiteAreaRow, whiteAreaCol;
    // upper left corner
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
        }
    }

    // bottom left corner
    for (whiteAreaRow = numFullStridesY / 2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
        }
    }

    // top right corner
    for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX / 2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
        }
    }

    // bottom right corner
    for (whiteAreaRow = numFullStridesY / 2 + hasMiddleSquare; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
        for (whiteAreaCol = numFullStridesX / 2 + hasMiddleSquare; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, isWhiteAreaStride);
        }
    }

    if (hasMiddleSquare) {
        // left middle row
        whiteAreaRow = numFullStridesY / 2;
        for (whiteAreaCol = 0; whiteAreaCol < numFullStridesX / 2; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
        }

        // right middle row
        for (whiteAreaCol = numFullStridesX / 2 + 1; whiteAreaCol < boolArrayWidth; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStride, middleSquareDimensions);
        }

        // top center row
        whiteAreaCol = numFullStridesX / 2;
        for (whiteAreaRow = 0; whiteAreaRow < numFullStridesY / 2; ++whiteAreaRow) {
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
        }

        // bottom center row
        whiteAreaCol = numFullStridesX / 2;
        for (whiteAreaRow = numFullStridesY / 2 + 1; whiteAreaRow < boolArrayHeight; ++whiteAreaRow) {
            isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, isWhiteAreaStride);
        }

        // middle square
        whiteAreaCol = numFullStridesY / 2;
        isWhiteArea[whiteAreaRow * boolArrayWidth + whiteAreaCol] =
            checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, middleSquareDimensions, middleSquareDimensions);
    }

    // For debugging: print the white area array
    /*
    printf("\n\nWhiteSpaceArray:\n");
    for (int j = 0; j < boolArrayHeight; j++) {
        for (int i = 0; i < boolArrayWidth; i++) {
            printf("%d", isWhiteArea[j * boolArrayWidth + i]);
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
    populateIsWhiteArea(buffer_frame, width, height);
}
#endif

/* End White Pixel Optimization Structures */

/* SubSquare Manipulation functions */

void swapAndMirrorYSubsquares(unsigned char *buffer_frame, unsigned buffer_width, unsigned left_left_pxindex, unsigned left_top_pxindex, unsigned right_right_pxindex, unsigned right_top_pxindex, unsigned subsquare_width, unsigned subsquare_height) {
    const unsigned buffer_width_3 = buffer_width * 3;
    const unsigned left_left_pxindex_3 = left_left_pxindex * 3;
    const unsigned right_right_pxindex_3 = right_right_pxindex * 3;

    unsigned ll_index, rr_index;
    unsigned char temp;

    /*
    printf("\n\nOld Left Square:\n");
    for (int i = 0; i < subsquare_height; i++) {
        for (int j = 0; j < subsquare_width; j++){
            unsigned char r = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3];
            unsigned char g = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3 + 1];
            unsigned char b = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3 + 2];
            printf("(%d,%d,%d)", r, g, b);
        }
        printf("\n");
    }
    printf("Old Right Square:\n");
    unsigned int right_left_pxindex = right_right_pxindex - subsquare_width + 1;
    for (int i = 0; i < subsquare_height; i++) {
        for (int j = 0; j < subsquare_width; j++){
            unsigned char r = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3];
            unsigned char g = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3 + 1];
            unsigned char b = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3 + 2];
            printf("(%d,%d,%d)", r, g, b);
        }
        printf("\n");
    }
    */

    // printf("MirrorY swapping the square at tl(%d,%d) with the square tr(%d,%d)\n", left_left_pxindex, left_top_pxindex, right_right_pxindex, right_top_pxindex);
    for (int i = 0; i < subsquare_height; ++i) {
        // move to first element of next row down
        ll_index = left_left_pxindex_3 + (left_top_pxindex + i) * buffer_width_3;
        // move to last element of next row down
        rr_index = right_right_pxindex_3 + (right_top_pxindex + i) * buffer_width_3;
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
        // printf("----\n");
    }
    /*
    printf("\nNew Left Square:\n");
    for (int i = 0; i < subsquare_height; i++) {
        for (int j = 0; j < subsquare_width; j++){
            unsigned char r = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3];
            unsigned char g = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3 + 1];
            unsigned char b = buffer_frame[left_left_pxindex_3 + i*buffer_width_3 + j * 3 + 2];
            printf("(%d,%d,%d)", r, g, b);
        }
        printf("\n");
    }
    printf("New Right Square:\n");
    right_left_pxindex = right_right_pxindex - subsquare_width + 1;
    for (int i = 0; i < subsquare_height; i++) {
        for (int j = 0; j < subsquare_width; j++){
            unsigned char r = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3];
            unsigned char g = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3 + 1];
            unsigned char b = buffer_frame[right_left_pxindex * 3 + i*buffer_width_3 + j*3 + 2];
            printf("(%d,%d,%d)", r, g, b);
        }
        printf("\n");
    }
    */
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
    // store shifted pixels to temporary buffer
    for (int row = 0; row < (height - offset); row++) {
        for (int column = 0; column < width; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            int position_buffer_frame = (row + offset) * width * 3 + column * 3;
            rendered_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
            rendered_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
            rendered_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
        }
    }

    // fill left over pixels with white pixels
    for (int row = (height - offset); row < height; row++) {
        for (int column = 0; column < width; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            rendered_frame[position_rendered_frame] = 255;
            rendered_frame[position_rendered_frame + 1] = 255;
            rendered_frame[position_rendered_frame + 2] = 255;
        }
    }

    // copy the temporary buffer back to original frame buffer
    buffer_frame = copyFrame(rendered_frame, buffer_frame, width, height);

    // return a pointer to the updated image buffer
    return buffer_frame;
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
    // store shifted pixels to temporary buffer
    for (int row = 0; row < height; row++) {
        for (int column = offset; column < width; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            int position_buffer_frame = row * width * 3 + (column - offset) * 3;
            rendered_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
            rendered_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
            rendered_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
        }
    }

    // fill left over pixels with white pixels
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < offset; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            rendered_frame[position_rendered_frame] = 255;
            rendered_frame[position_rendered_frame + 1] = 255;
            rendered_frame[position_rendered_frame + 2] = 255;
        }
    }

    // copy the temporary buffer back to original frame buffer
    buffer_frame = copyFrame(rendered_frame, buffer_frame, width, height);

    // return a pointer to the updated image buffer
    return buffer_frame;

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
    // store shifted pixels to temporary buffer
    for (int row = offset; row < height; row++) {
        for (int column = 0; column < width; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            int position_buffer_frame = (row - offset) * width * 3 + column * 3;
            rendered_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
            rendered_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
            rendered_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
        }
    }

    // fill left over pixels with white pixels
    for (int row = 0; row < offset; row++) {
        for (int column = 0; column < width; column++) {
            int position_rendered_frame = row * width * 3 + column * 3;
            rendered_frame[position_rendered_frame] = 255;
            rendered_frame[position_rendered_frame + 1] = 255;
            rendered_frame[position_rendered_frame + 2] = 255;
        }
    }

    // copy the temporary buffer back to original frame buffer
    buffer_frame = copyFrame(rendered_frame, buffer_frame, width, height);

    // return a pointer to the updated image buffer
    return buffer_frame;
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
    return processMoveLeftReference(buffer_frame, width, height, offset);
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
    return processRotateCWReference(buffer_frame, width, height, rotate_iteration);
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
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
unsigned char *processMirrorX(unsigned char *buffer_frame, unsigned int width, unsigned int height, int _unused) {
    return processMirrorXReference(buffer_frame, width, height, _unused);
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
unsigned char *processMirrorY(unsigned char *buffer_frame, unsigned width, unsigned height, int _unused) {
    /*
    printf("original:\n");
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            int position_buffer_frame = row * height * 3 + column * 3;
            printf("(%d,%d,%d)", buffer_frame[position_buffer_frame], buffer_frame[position_buffer_frame + 1], buffer_frame[position_buffer_frame + 2]);
        }
        printf("\n");
    }
    */
    // TODO: only swap and mirror non-white squares.
    swapAndMirrorYSubsquares(buffer_frame, width, 0, 0, width-1, 0, width/2, height);
    /*
    printf("mirrored:\n");
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            int position_buffer_frame = row * height * 3 + column * 3;
            printf("(%d,%d,%d)", buffer_frame[position_buffer_frame], buffer_frame[position_buffer_frame + 1], buffer_frame[position_buffer_frame + 2]);
        }
        printf("\n");
    }
    */
    // TODO: once everything is done in-place, wont need this return anymore
    return buffer_frame;
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
    int processed_frames = 0;
    rendered_frame = allocateFrame(width, height);
    int collapsed_sensor_values_count = 0;
    optimized_kv *collapsed_sensor_values = collapse_sensor_values(sensor_values, sensor_values_count, &collapsed_sensor_values_count);
    /*
    printf("Original Sensor number: %d, New Sensor Count: %d\n", sensor_values_count, collapsed_sensor_values_count);
    for (int i = 0; i < collapsed_sensor_values_count; ++i) {
        printf("\tCommand: %d Value: %d\n", collapsed_sensor_values[i].type, collapsed_sensor_values[i].value);
    }
    */
    #ifdef USE_ISWHITEARRAY
    createIsWhiteArea(frame_buffer, width, height);
    #endif
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
    #ifdef USE_ISWHITEARRAY
    free(isWhiteArea);
    #endif
    free(collapsed_sensor_values);
    deallocateFrame(rendered_frame);
    return;
}
