/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <math.h>
#include "matrix.h"

void rotation_matrix_set(float matrix[9], float yaw, float pitch, float roll) {
    float cosY = cos(yaw);
    float cosP = cos(pitch);
    float cosR = cos(roll);

    float sinY = sin(yaw);
    float sinP = sin(pitch);
    float sinR = sin(roll);

    /* Rotate in Yaw, Pitch, Roll order */
    matrix[0] =  cosP * cosY;
    matrix[3] = -cosP * sinY;
    matrix[6] =  sinP;

    matrix[1] =  cosR * sinY + sinR * cosY * sinP;
    matrix[4] =  cosR * cosY - sinR * sinY * sinP;
    matrix[7] = -sinR * cosP;

    matrix[2] = sinR * sinY - cosR * cosY * sinP;
    matrix[5] = sinR * cosY + cosR * sinY * sinP;
    matrix[8] = cosR * cosP;
}

void axis_angle_matrix_set(float matrix[9], float x, float y, float z, float angle) {
    float mag = sqrt(x * x + y * y + z * z);
    float cosA = cos(angle);
    float sinA = sin(angle);
    float t = 1.0 - cosA;
    float tx, ty;

    if (mag == 0.0) {
        int i;
        for (i = 0; i < 9; i++) {
            matrix[i] = 0;
        }
        return;
    }

    x = x / mag;
    y = y / mag;
    z = z / mag;

    tx = t * x;
    ty = t * y;

    matrix[0] = tx * x + cosA;
    matrix[3] = tx * y - sinA * z;
    matrix[6] = tx * z + sinA * y;
    matrix[1] = tx * y + sinA * z;
    matrix[4] = ty * y + cosA;
    matrix[7] = ty * z - sinA * x;
    matrix[2] = tx * z - sinA * y;
    matrix[5] = ty * z + sinA * x;
    matrix[8] = t * z * z + cosA;
}

void transform_point(
  Point *target,
  const Point *point,
  const Point *origin,
  const Point *translation,
  const float matrix[9]) {

  target->x = translation->x +
    matrix[0] * (point->x - origin->x) +
    matrix[3] * (point->y - origin->y) +
    matrix[6] * (point->z - origin->z) +
    origin->x;

  target->y = translation->y +
    matrix[1] * (point->x - origin->x) +
    matrix[4] * (point->y - origin->y) +
    matrix[7] * (point->z - origin->z) +
    origin->y;

  target->z = translation->z +
    matrix[2] * (point->x - origin->x) +
    matrix[5] * (point->y - origin->y) +
    matrix[8] * (point->z - origin->z) +
    origin->z;
}
