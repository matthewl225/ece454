#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line



/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
void processMoveUp(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height, int offset) {
    // store shifted pixels to temporary buffer

    // precomputed constants
    int const width_3 = width * 3;
    int const height_minus_offset = height - offset;

    // Loop variables
    int row, column, column_3, row_width_3;

    int position_rendered_frame_row_offset = 0;
    int position_original_frame_row_offset = offset * width_3;
    int position_rendered_frame, position_original_frame;
    for (row = 0; row < height_minus_offset; ++row) {
        column_3 = 0;
        for (column = 0; column < width; ++column) {
            // TODO can probably remove column_3
            position_rendered_frame = position_rendered_frame_row_offset + column_3;
            position_original_frame = position_original_frame_row_offset + column_3;
            rendered_frame[position_rendered_frame] = original_frame[position_original_frame];
            rendered_frame[position_rendered_frame + 1] = original_frame[position_original_frame + 1];
            rendered_frame[position_rendered_frame + 2] = original_frame[position_original_frame + 2];
            column_3 += 3;
        }
        position_rendered_frame_row_offset += width_3;
        position_original_frame_row_offset += width_3;
    }

    // fill left over pixels with white pixels
    row_width_3 = height_minus_offset * width_3;
    for (row = height_minus_offset; row < height; ++row) {
        column_3 = 0;
        for (column = 0; column < width; ++column) {
            int position_rendered_frame = row_width_3 + column_3;
            rendered_frame[position_rendered_frame] = 255;
            rendered_frame[position_rendered_frame + 1] = 255;
            rendered_frame[position_rendered_frame + 2] = 255;
            column_3 += 3;
        }
        row_width_3 += width_3;
    }

    // TODO: do this copy inline rather than using a temporary buffer
    // copy the temporary buffer back to original frame buffer
    original_frame = copyFrame(rendered_frame, original_frame, width, height);
    // return processMoveUpReference(original_frame, width, height, offset);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image left
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
void processMoveRight(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height, int offset) {
    // precomputed constants
    int const width_3 = width * 3;
    int const offset_3 = offset * 3;

    // Loop variables
    int row, column, column_3, row_width_3;
    int column_minus_offset_3 = 0;

    int position_rendered_frame_row_offset = 0;
    int position_original_frame_row_offset = offset * width_3;
    int position_rendered_frame, position_original_frame;
    // store shifted pixels to temporary buffer
    row_width_3 = 0;
    for (row = 0; row < height; row++) {
        column_3 = offset_3;
        column_minus_offset_3 = 0;
        for (column = offset; column < width; column++) {
            position_rendered_frame = position_rendered_frame_row_offset + column_3;
            position_original_frame = row_width_3 + column_minus_offset_3;
            rendered_frame[position_rendered_frame] = original_frame[position_original_frame];
            rendered_frame[position_rendered_frame + 1] = original_frame[position_original_frame + 1];
            rendered_frame[position_rendered_frame + 2] = original_frame[position_original_frame + 2];
            column_3 += 3;
            column_minus_offset_3 += 3;
        }
        row_width_3 += width_3;
    }

    // fill left over pixels with white pixels
    row_width_3 = 0;
    column_3 = 0;
    for (row = 0; row < height; row++) {
        for (column = 0; column < offset; column++) {
            position_rendered_frame = row_width_3 + column_3;
            rendered_frame[position_rendered_frame] = 255;
            rendered_frame[position_rendered_frame + 1] = 255;
            rendered_frame[position_rendered_frame + 2] = 255;
            column_3 += 3;
        }
        row_width_3 += width_3;
    }

    // copy the temporary buffer back to original frame buffer
    original_frame = copyFrame(rendered_frame, original_frame, width, height);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
void processMoveDown(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height, int offset) {
    // return processMoveDownReference(original_frame, width, height, offset);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image right
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
void processMoveLeft(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height, int offset) {
    // return processMoveLeftReference(original_frame, width, height, offset);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
void processRotateCW(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height,
                               int rotate_iteration) {
    // return processRotateCWReference(original_frame, width, height, rotate_iteration);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer counter clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
void processRotateCCW(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height,
                                int rotate_iteration) {
    // return processRotateCCWReference(original_frame, width, height, rotate_iteration);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
void processMirrorX(unsigned char *original_frame, unsigned char *rendered_frame, unsigned int width, unsigned int height, int _unused) {
    // return processMirrorXReference(original_frame, width, height, _unused);
}

/***********************************************************************************************************************
 * @param original_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
void processMirrorY(unsigned char *original_frame, unsigned char *rendered_frame, unsigned width, unsigned height, int _unused) {
    // return processMirrorYReference(original_frame, width, height, _unused);
}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          Do not forget to modify the team_name and team member information !!!
 **********************************************************************************************************************/
void print_team_info(){
    // Please modify this field with something interesting
    char team_name[] = "default-name";

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
    unsigned char *new_frame_buffer = allocateFrame(width, height);
    int processed_frames = 0;
    for (int sensorValueIdx = 0; sensorValueIdx < sensor_values_count; sensorValueIdx++) {
        // TODO: expand strcmp into direct comparisons on char values, remember \0 at end
//        printf("Processing sensor value #%d: %s, %d\n", sensorValueIdx, sensor_values[sensorValueIdx].key,
//               sensor_values[sensorValueIdx].value);
        if (!strcmp(sensor_values[sensorValueIdx].key, "W")) {
            processMoveUp(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "A")) {
            processMoveLeft(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "S")) {
            processMoveDown(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "D")) {
            processMoveRight(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CW")) {
            processRotateCW(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CCW")) {
            processRotateCCW(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MX")) {
            processMirrorX(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MY")) {
            processMirrorY(frame_buffer, new_frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        }
        processed_frames += 1;
        if (processed_frames % 25 == 0) {
            verifyFrame(new_frame_buffer, width, height, grading_mode);
        }
    }
    deallocateFrame(new_frame_buffer);
    return;
}
