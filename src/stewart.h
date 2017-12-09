/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __stewart_h__
#define __stewart_h__

#include <math.h> /* M_PI */
#include "matrix.h"

/***************************************************************************
 *
 * See config.h for servo arrangement and variables that may need to be
 * adjusted based on physical dimensions of the platform being controlled.
 *
 ***************************************************************************/

typedef enum {
    CLOCKWISE = -1,
    COUNTER_CLOCKWISE = 1
} ServoDirection;

typedef struct {
    float servo_min;            /* Servo physical minimum limit in radians */
    float servo_max;            /* Servo physical maximum limit in radians */
    float servo_arm_length;     /* Length of servo arm in inches */
    float servo_orientation[6]; /* Rotation of the positive (left-hand rule)
                                 * each servo relative to the platform Y axis */
    ServoDirection servo_direction[6]; /* Direction the servo rotates to
                                 * move the servo arm in the +Z direction */
    float servo_trim[6];        /* Trim per servo in radians */
    float control_rod_length;   /* Length of control rod in inches */
    float platform_height;      /* Height in inches from servo axis to platform
                                 * mounting point for control rod */
    float effector_radius;      /* Distance from platform center to control rod
                                 * connector in inches */
    float base_radius;          /* Distance from base center to pivot point of
                                 * servo arm in inches. NOTE: This point is
                                 * physically farther than the edge of the servo */
    float theta_base;           /* Angle (THETA) between paired servo arm pivot
                                 * points on Base in radians */
    float theta_effector;       /* Angle (THETA) between paired effector attachment
                                 * points on Effector platform in radians */

    int debug;                  /* Set to 1 if you want verbose output while solving
                                 * the inverse kinematics */
} StewartConfig;

typedef struct _StewartPlatform StewartPlatform;

typedef enum {
    TRANSFORM_EUCLIDEAN = 0,
    TRANSFORM_AXIS_ANGLE = 1
} TransformType;

typedef struct {
    Point translate;
    Point rotate;
    float angle;
    TransformType type;
} Transform;

typedef enum {
    SOLUTION = 0,     /* Solution for this point was solved without errors */
    IMPOSSIBLE = 1,   /* No solution is physically possible; "closest"
                       * angle returned. */
    MASK = (SOLUTION | IMPOSSIBLE),
    LIMITED = 1 << 4, /* Solution for this point was limited by physical
                       * constraints */
    TRIM = 1 << 5,    /* TRIM contributed to exceeding physical
                       * constraints */
} SolutionType;

typedef struct {
    float angle;
    float actual;     /* Only set if (type & LIMITED) set */
    SolutionType type;
} Solution;

#ifndef DEG2RAD
#define DEG2RAD(__deg) (float)(M_PI * (float)((double)__deg) / 180.0f)
#endif

#ifndef RAD2DEG
#define RAD2DEG(__rad) (float)(180.0f * (float)((double)__rad) / M_PI)
#endif

StewartPlatform *stewart_platform_create(const StewartConfig *config);
void stewart_platform_dump(const StewartPlatform *platform);
int stewart_get_solutions(const StewartPlatform *platform, const Point *origin,
                          const Transform *transform, Solution solutions[6], float *matrix);
void stewart_platform_delete(StewartPlatform *platform);
struct timeval stewart_platform_get_elapsed(const StewartPlatform *platform);

#endif
