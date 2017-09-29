#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line

// Configuration parameters, uncomment them to turn them on
#define USE_ISWHITEARRAY

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
 * if isWhiteArea[i,j] is true, then the square with corners at (isWhiteAreaStrideX * i, isWhiteAreaStrideY * j, isWhiteAreaStrideX * (i + 1) - 1, isWhiteAreaStrideY * (j + 1) - 1)
 * contains only white pixels and can be optimized as such
 */

#ifdef USE_ISWHITEARRAY
#define isWhiteAreaStrideX 21 // translates to 64 bytes, each pixel is 3 bytes
#define isWhiteAreaStrideY 21 // 64 / 3
bool *isWhiteArea;
bool checkWhiteAreaSquare(unsigned char *buffer_frame, unsigned buffer_pxwidth, unsigned whiteAreaCol, unsigned whiteAreaRow, unsigned pxWidth, unsigned pxHeight) {
    unsigned row_offset = whiteAreaRow * isWhiteAreaStrideY * buffer_pxwidth * 3;
    unsigned col_offset = whiteAreaCol * isWhiteAreaStrideX * 3;
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
void createIsWhiteArea(unsigned char *buffer_frame, unsigned width, unsigned height) {
    unsigned int numStridesX = (width / isWhiteAreaStrideX) + 1; // divide into this many 21x21 pixel squares
    unsigned int numStridesY = (height / isWhiteAreaStrideY) + 1;

    isWhiteArea = calloc(numStridesX * numStridesY, sizeof(bool));
    int col_offset;
    int row_offset = 0;
    int whiteAreaRow, whiteAreaCol;
    for (whiteAreaRow = 0; whiteAreaRow < numStridesY - 1; ++whiteAreaRow) {
        col_offset = 0;
        for (whiteAreaCol = 0; whiteAreaCol < numStridesX - 1; ++whiteAreaCol) {
            // todo: optimize this
            isWhiteArea[whiteAreaRow * numStridesX + whiteAreaCol] =
                checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStrideX, isWhiteAreaStrideY);
        }
    }

    // do the bottom row of the array, which may not be a full stride in height. Doesn't include bottom right corner
    unsigned remainingStrideHeight = height % isWhiteAreaStrideY;
    unsigned remainingStrideWidth = width % isWhiteAreaStrideX;
    // whiteAreaRow is numStridesY - 1
    for (whiteAreaCol = 0; whiteAreaCol < numStridesX - 1; ++whiteAreaCol) {
        isWhiteArea[whiteAreaRow * numStridesX + whiteAreaCol] =
            checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, isWhiteAreaStrideX, remainingStrideHeight);
    }

    // do the right most column of the array, white may not be a full stride in width. Doesn't include bottom right corner
    // whiteAreaCol is numStridesX - 1
    for (whiteAreaRow = 0; whiteAreaRow < numStridesY - 1; ++whiteAreaRow) {
        isWhiteArea[whiteAreaRow * numStridesX + whiteAreaCol] =
            checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, remainingStrideWidth, isWhiteAreaStrideY);
    }

    // do the bottom right corner
    // whiteAreaRow = numStridesY - 1, whiteAreaCol = numStridesX - 1
    isWhiteArea[whiteAreaRow * numStridesX + whiteAreaCol] =
        checkWhiteAreaSquare(buffer_frame, width, whiteAreaCol, whiteAreaRow, remainingStrideWidth, remainingStrideHeight);

    /* For debugging: print the white area array
    for (int j = 0; j < numStridesY; j++) {
        for (int i = 0; i < numStridesX; i++) {
            printf("%d", isWhiteArea[j * numStridesX + i]);
        }
        printf("\n");
    }
    */

}
#endif


/* End White Pixel Optimization Structures */


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
    return processMirrorYReference(buffer_frame, width, height, _unused);
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
