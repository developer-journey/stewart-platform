/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <stdio.h>
#include <math.h>
#include "matrix.h"

void printPoint(const Point *p) {
  printf("<%+0.2f, %+0.2f, %+0.2f>", p->x, p->y, p->z);
}

void printLine(const char *title, const Point *before, const Point *after) {
  printf("%s", title);
  printPoint(before);
  printf(" => ");
  printPoint(after);
  printf("\n");
}

int main() {
  Point x = {
    1, 0, 0
  };
  Point y = {
    0, 1, 0
  };
  Point z = {
    0, 0, 1
  };
  Point origin = {
    0, 0, 0
  };
  Point translation = {
    0, 0, 0
  };
  Point result;

  float matrix[9];
  int ticks[] = { 0, 45, 90, 135, 180, 225, 270, 315 };
  int i;

  for (i = 0; i < sizeof(ticks) / sizeof(ticks[0]); i++) {
    float degrees = ticks[i];
    rotation_matrix_set(matrix, degrees * M_PI / 180.0, 0, 0);
    printf("Transformed by %+.0fdeg YAW:\n", degrees);
    transform_point(&result, &x, &origin, &translation, matrix);
    printLine("X vector:", &x, &result);
    transform_point(&result, &y, &origin, &translation, matrix);
    printLine("Y vector:", &y, &result);
    transform_point(&result, &z, &origin, &translation, matrix);
    printLine("Z vector:", &z, &result);

    rotation_matrix_set(matrix, 0, degrees * M_PI / 180.0, 0);
    printf("Transformed by %+.0fdeg PITCH:\n", degrees);
    transform_point(&result, &x, &origin, &translation, matrix);
    printLine("X vector:", &x, &result);
    transform_point(&result, &y, &origin, &translation, matrix);
    printLine("Y vector:", &y, &result);
    transform_point(&result, &z, &origin, &translation, matrix);
    printLine("Z vector:", &z, &result);

    rotation_matrix_set(matrix, 0, 0, degrees * M_PI / 180.0);
    printf("Transformed by %+.0fdeg ROLL:\n", degrees);
    transform_point(&result, &x, &origin, &translation, matrix);
    printLine("X vector:", &x, &result);
    transform_point(&result, &y, &origin, &translation, matrix);
    printLine("Y vector:", &y, &result);
    transform_point(&result, &z, &origin, &translation, matrix);
    printLine("Z vector:", &z, &result);
  }

  return 0;
}
