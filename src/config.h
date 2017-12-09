/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __config_h__
#define __config_h__

#include "stewart.h"

/***************************************************************************
 *
 * The files in this project requires that servos be arranged
 * as documented in the Developer Journey [1] with the PWM channel 0
 * corresponding to servo 0:
 *
 *                               y+
 *                                ^
 *                                |
 *                            .-------.
 *                         2 /         \ 3
 *                          /           \
 *                       1 /             \ 4
 *                -x <--- <       o +z    >  ---> x+
 *                         \             /
 *                          \           /
 *                           \         /
 *                            `-------'
 *                              0 | 5
 *                                v
 *                               -y
 *
 * 1. https://01.org/developerjourney/recipe/stewart-platform
 *
 ***************************************************************************/

/* Trim amount in DEGREES to adjust the servo so that zero degrees is
 * indeed zero. See "trim" application and "stewart.cfg" to keep from needing
 * to modify this file  */
#define TRIM_0            0
#define TRIM_1            0
#define TRIM_2            0
#define TRIM_3            0
#define TRIM_4            0
#define TRIM_5            0

/* Min and Max PWM signals for Tower Pro SG-5010 */
#define SERVO_MIN_ANGLE       -90     /* For 180-degree servo */
#define SERVO_MAX_ANGLE       +90     /* For 180-degree servo */
#define PULSE_WIDTH_MAX_POS   2500    /* in us - ~2500 is "standard" */
#define PULSE_WIDTH_MIN_POS   500     /* in us - ~500 is "standard"  */
#define PULSE_WIDTH_FREQUENCY 100     /* in Hz - cycles per second   */
#define PULSE_WIDTH_ZERO_POS  1500    /* in ms - ~1500 is "standard" */

/***************************************************************************
 *
 * If you build to the dimensions specified in the Developer Journey,
 * then you shouldn't need to modify any of the values below.
 *
 ***************************************************************************/

/* Servo external physical limits (0 is horizontal) -- change these values
 * based on if your servos are binding or running into things when they move */
#define SERVO_MIN          -85
#define SERVO_MAX          +85

/* Servo arm length in inches from center to ball joint pivot */
#define SERVO_ARM_LENGTH   (float)(1.0f + 7.0f / 16.0f)

/* Control rod length in inches */
#define CONTROL_ROD_LENGTH (float)(4.0f + 1.0f / 16.0f)

/* With servos at 0 position, vertical distance from link top to servo axis
 * This is used as Z coordinate in the effector_pos */
#define PLATFORM_HEIGHT    (float)(3.0f + 3.0f / 16.0f)

/* Center of top platform to effector connector in inches */
#define EFFECTOR_RADIUS    (float)(3.0f + 6.0f / 8.0f)

/* Center of base platform to pivot of servo arm in inches */
#define BASE_RADIUS        (float)(4.0f + 1.0f / 8.0f)

/* Angle (THETA) between paired servo arm pivot points (BASE)
 * IN RADIANS */
#define THETA_BASE         DEG2RAD(63.0f)

/* Angle (THETA) between paired effector attachement points (EFFECTOR)
 * IN RADIANS */
#define THETA_EFFECTOR     DEG2RAD(32.4f)

/* Max angles to allow the platform to attempt to move within */
#define MAX_ROLL      (15.0f)
#define MAX_PITCH     (15.0f)
#define MAX_YAW       (30.0f)

/************************************************************************
 *
 * You should not need to change anything below this line unless
 * you build a layout of the Stewart platform different than is
 * documented in the Developer Journey.
 *
 * For example, if you change how the servos are rotated relative to the
 * effector positions, or use servos that rotate in the opposite direction
 * from the servos in the Developer Journey.
 *
 *************************************************************************/

/* Rotation IN RADIANS of the positive (left-hand rule) axis for the servo
 * relative to the platform Z axis */
#define SERVO_ORIENTATION_0 ( 6.0 * M_PI / 6.0)
#define SERVO_ORIENTATION_1 (-4.0 * M_PI / 6.0)
#define SERVO_ORIENTATION_2 ( 2.0 * M_PI / 6.0)
#define SERVO_ORIENTATION_3 ( 4.0 * M_PI / 6.0)
#define SERVO_ORIENTATION_4 (-2.0 * M_PI / 6.0)
#define SERVO_ORIENTATION_5 ( 0.0 * M_PI / 6.0)

/* Direction the servo rotates to move the servo arm in the positive
 * Z direction */
#define SERVO_DIRECTION_0   CLOCKWISE
#define SERVO_DIRECTION_1   COUNTER_CLOCKWISE
#define SERVO_DIRECTION_2   CLOCKWISE
#define SERVO_DIRECTION_3   COUNTER_CLOCKWISE
#define SERVO_DIRECTION_4   CLOCKWISE
#define SERVO_DIRECTION_5   COUNTER_CLOCKWISE

#define RADIANS_TO_PULSE_WIDTH(__rad) (PULSE_WIDTH_ZERO_POS +            \
        ((double)__rad) * (PULSE_WIDTH_MAX_POS - PULSE_WIDTH_MIN_POS) / \
        DEG2RAD(SERVO_MAX_ANGLE - SERVO_MIN_ANGLE))

#define PULSE_WIDTH_TO_RADIANS(__pulse) ((((double)__pulse) - PULSE_WIDTH_ZERO_POS) * \
        DEG2RAD(SERVO_MAX_ANGLE - SERVO_MIN_ANGLE) /                                  \
        (PULSE_WIDTH_MAX_POS - PULSE_WIDTH_MIN_POS))

void config_get(StewartConfig *c);
int config_write(StewartConfig *c);

#endif
