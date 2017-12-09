/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __matrix_h__
#define __matrix_h__

typedef struct {
    float x;
    float y;
    float z;
} Vector;

typedef Vector Point;

void rotation_matrix_set(float matrix[9], float yaw, float pitch, float roll);
void axis_angle_matrix_set(float matrix[9], float x, float y, float z, float angle);
void transform_point(Point *target, const Point *point, const Point *origin, const Point *translation, const float matrix[9]);

#endif
