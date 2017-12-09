/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "stewart.h"
#include "matrix.h"

/***************************************************************************/

typedef struct {
    Point servo_axis_normal[6];
    Point servo_axis_pos[6];
    Point effector_pos[6];
} StewartGeometry;

struct _StewartPlatform {
    StewartConfig config;
    StewartGeometry geometry;
    struct timeval started;
};

void _print_point(const char *name, int index, const Point *point);
void _init_geometry(StewartPlatform *platform);
int _circle_sphere_intersect(const StewartPlatform *platform,
                             const Point *C, const float servo_arm_length, const Point *servo_arm,
                             const Point *S, const float effector_rod_length, float angle[2]);
void _transform_platform_effectors(const StewartPlatform *platform, const Point *origin,
                             const Transform *transform, Point target[], float *matrix);


/*
 *
 * External interface
 *
 */

StewartPlatform *stewart_platform_create(const StewartConfig *config) {
    StewartPlatform *platform = (StewartPlatform *)malloc(sizeof(*platform));
    if (!platform) {
        return NULL;
    }
    memset(platform, 0, sizeof(*platform));
    memcpy(&platform->config, config, sizeof(platform->config));
    _init_geometry(platform);
    gettimeofday(&platform->started, NULL);
    return platform;
}

struct timeval stewart_platform_get_elapsed(const StewartPlatform *platform) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec -= platform->started.tv_sec;
    if (tv.tv_usec < platform->started.tv_usec) {
        tv.tv_sec--;
        tv.tv_usec += 1000000;
    }
    tv.tv_usec -= platform->started.tv_usec;
    return tv;
}

void stewart_platform_delete(StewartPlatform *platform) {
    if (!platform) {
        return;
    }
    memset(platform, 0, sizeof(*platform));
    free(platform);
}

int stewart_get_solutions(const StewartPlatform *platform, const Point *origin,
                          const Transform *transform,
                          Solution solutions[6], float *matrix) {
    const StewartConfig *c = &platform->config;
    const StewartGeometry *g = &platform->geometry;
    int i;
    Point effectors[6];
    int constrained = 0;
    float angles[2];
    Solution *solution;

    _transform_platform_effectors(platform, origin, transform, effectors, matrix);

    /* Calculate all 6 target angles */
    for (i = 0; i < 6; i++) {
        int ret;

        if (c->debug) {
            _print_point("target", i, &effectors[i]);
        }

        /* Imagine a sphere with a radius the length of the effector rod, centered at
         * the effector position (on the platform top.) Now imagine a circle inscribed
         * by the servo arm, with it's center at the center of the servo.
         *
         * If you look down on the Stewart platform, the sphere around the effector
         * can be thought of as a circle. The circle for the servo arm is now just a single
         * line.
         *
         * If we find the two points on the circle where that line intersects, we will have
         * two points which represent the diameter of a sphere intersected by the plane
         * of the servo's arm.
         *
         */
        ret = _circle_sphere_intersect(platform,
            &g->servo_axis_pos[i], c->servo_arm_length, &g->servo_axis_normal[i],
            &effectors[i], c->control_rod_length, angles);

        solution = &solutions[i];

        switch (ret) {
        case 1:
            solution->angle = angles[0];
            solution->type = SOLUTION;
            break;

        case 2:
            if (fabs(angles[0]) < fabs(angles[1])) {
                solution->angle = angles[0];
            } else {
                solution->angle = angles[1];
            }
            solution->type = SOLUTION;
            break;

        default:
            solution->angle = angles[0];
            solution->type = IMPOSSIBLE;
            constrained++;
            break;
        }

        /* Internally, the solver uses radians.
         *
         * Callers to the Stewart platform solver use degrees.
         *
         * Convert the radians to degrees here. */
        solution->angle = c->servo_direction[i] * RAD2DEG(solution->angle);

        if (solution->angle + c->servo_trim[i] < c->servo_min) {
            if (solution->angle >= c->servo_min) {
                solution->type |= TRIM;
            }
            solution->actual = solution->angle + c->servo_trim[i];
            solution->angle = c->servo_min;
            solution->type |= LIMITED;
            constrained++;
        } else if (solution->angle + c->servo_trim[i] > c->servo_max) {
            if (solution->angle <= c->servo_max) {
                solution->type |= TRIM;
            }
            solution->actual = solution->angle + c->servo_trim[i];
            solution->angle = c->servo_max;
            solution->type |= LIMITED;
            constrained++;
        } else {
            solution->angle += c->servo_trim[i];
            solution->actual = solution->angle;
        }

        if (c->debug) {
            fprintf(stdout, "Servo %d: %.02f (%d)\n", i,
                    solution->angle, ret);
        }
    }

    return constrained;
}

void stewart_platform_dump(const StewartPlatform *platform) {
    const StewartGeometry *g = &platform->geometry;
    const StewartConfig *c = &platform->config;
    int i;

    fprintf(stdout, "StewartConfig *config = {\n");
    fprintf(stdout, "    servo_min = %.02f\n", c->servo_min);
    fprintf(stdout, "    servo_max = %.02f\n", c->servo_max);
    fprintf(stdout, "    servo_arm_length = %.02f\n", c->servo_arm_length);
    for (i = 0; i < 6; i++) {
        fprintf(stdout, "    servo_orientation[%d] = %.02f\n", i,
                c->servo_orientation[i] / M_PI);
    }
    for (i = 0; i < 6; i++) {
        fprintf(stdout, "    servo_direction[%d] = %s\n", i,
                (c->servo_direction[i] == CLOCKWISE) ?
                "CLOCKWISE" : "COUNTER_CLOCKWISE");
    }
    for (i = 0; i < 6; i++) {
        fprintf(stdout, "    servo_trim[%d] = %+6.02fdeg\n", i,
                c->servo_trim[i]);
    }

    fprintf(stdout, "    control_rod_length = %.02f\n", c->control_rod_length);
    fprintf(stdout, "    platform_height = %.02f\n", c->platform_height);
    fprintf(stdout, "    effector_radius = %.02f\n", c->effector_radius);
    fprintf(stdout, "    base_radius = %.02f\n", c->base_radius);
    fprintf(stdout, "    theta_base = %.02f /* in radians */\n", c->theta_base);
    fprintf(stdout, "    theta_effector = %.02f /* in radians */\n", c->theta_effector);

    fprintf(stdout, "}\n\nStewartGeometry *geometry = {\n");

    for (i = 0; i < 6; i++) {
        _print_point("    servo_axis_pos", i, &g->servo_axis_pos[i]);
    }
    for (i = 0; i < 6; i++) {
        _print_point("    servo_axis_normal", i, &g->servo_axis_normal[i]);
    }
    for (i = 0; i < 6; i++) {
        _print_point("    effector_pos", i, &g->effector_pos[i]);
    }

    fprintf(stdout, "}\n");
}

/************************************************************
 *
 * Internal helper functions
 *
 ************************************************************/
void _init_geometry(StewartPlatform *platform) {
    StewartGeometry *g = &platform->geometry;
    StewartConfig *c = &platform->config;
    float Rb = c->base_radius;
    float Re = c->effector_radius;
    float Te = c->theta_effector / 2.0;
    float Tb = c->theta_base / 2.0;
    int i;

    /* Calcualte the position of each servo rotation point in 3D space */
    g->servo_axis_pos[0].x = Rb * sin((-M_PI) + Tb);
    g->servo_axis_pos[1].x = Rb * sin((-M_PI / 3) - Tb);
    g->servo_axis_pos[2].x = Rb * sin((-M_PI / 3) + Tb);
    g->servo_axis_pos[3].x = Rb * sin(( M_PI / 3) - Tb);
    g->servo_axis_pos[4].x = Rb * sin(( M_PI / 3) + Tb);
    g->servo_axis_pos[5].x = Rb * sin(( M_PI) - Tb);

    g->servo_axis_pos[0].y = Rb * cos((-M_PI) + Tb);
    g->servo_axis_pos[1].y = Rb * cos((-M_PI / 3) - Tb);
    g->servo_axis_pos[2].y = Rb * cos((-M_PI / 3) + Tb);
    g->servo_axis_pos[3].y = Rb * cos(( M_PI / 3) - Tb);
    g->servo_axis_pos[4].y = Rb * cos(( M_PI / 3) + Tb);
    g->servo_axis_pos[5].y = Rb * cos(( M_PI) - Tb);

    g->servo_axis_pos[0].z =
    g->servo_axis_pos[1].z =
    g->servo_axis_pos[2].z =
    g->servo_axis_pos[3].z =
    g->servo_axis_pos[4].z =
    g->servo_axis_pos[5].z = 0;

    /* calculate the servo's axis normal--this is the vector the
     * servo arm is pointing at 0 degrees */
    for (i = 0; i < 6; i++) {
        g->servo_axis_normal[i].x = cos(c->servo_orientation[i]);
        g->servo_axis_normal[i].y = sin(c->servo_orientation[i]);
        g->servo_axis_normal[i].z = 0;
    }

    /* Calcualte the position of each effector (platform connection point) in 3D space
     *
     * NOTE: Don't set Y coordinate of effector_pos here or it would need to be
     * subtracted and re-added during the platform transformation operation. Instead
     * just add it during that call (_transform_platform_effectors)
     */

     g->effector_pos[0].x = Re * sin((-M_PI) + Te);
     g->effector_pos[1].x = Re * sin((-M_PI / 3) - Te);
     g->effector_pos[2].x = Re * sin((-M_PI / 3) + Te);
     g->effector_pos[3].x = Re * sin(( M_PI / 3) - Te);
     g->effector_pos[4].x = Re * sin(( M_PI / 3) + Te);
     g->effector_pos[5].x = Re * sin(( M_PI) - Te);

     g->effector_pos[0].y = Re * cos((-M_PI) + Te);
     g->effector_pos[1].y = Re * cos((-M_PI / 3) - Te);
     g->effector_pos[2].y = Re * cos((-M_PI / 3) + Te);
     g->effector_pos[3].y = Re * cos(( M_PI / 3) - Te);
     g->effector_pos[4].y = Re * cos(( M_PI / 3) + Te);
     g->effector_pos[5].y = Re * cos(( M_PI) - Te);

     g->effector_pos[0].z =
     g->effector_pos[1].z =
     g->effector_pos[2].z =
     g->effector_pos[3].z =
     g->effector_pos[4].z =
     g->effector_pos[5].z = 0;
}

typedef struct {
    Point A;
    Point B;
    Point mid;
} Intersection;

int _line_circle_intersect(const StewartPlatform *platform,
                           const Point *vector, const Point *C, float radius,
                           Intersection *intersection) {
    const StewartConfig *config = &platform->config;
    float a = vector->x * vector->x + vector->y * vector->y;
    float b = 2.0 * (vector->x * -C->x + vector->y * -C->y);
    float c = (C->x * C->x + C->y * C->y) - (radius * radius);
    float bb4ac = b * b - 4 * a * c;
    float s;

    if (a == 0) {
        if (config->debug) {
            fprintf(stdout, "Line has no direction!\n");
        }
        return -1;
    }

    if (bb4ac < 0) {
        if (config->debug) {
            fprintf(stdout, "bb4ac is negative: bb4ac=%f, b=%f, a=%f, c=%f!\n",
                    bb4ac, b, a, c);
        }
        return 0;
    }

    s = sqrt(bb4ac);

    float ua = (-b + s) / (2 * a);
    float ub = (-b - s) / (2 * a);
    float um = -b / (2 * a);

    intersection->A.x = vector->x * ua;
    intersection->A.y = vector->y * ua;
    intersection->B.x = vector->x * ub;
    intersection->B.y = vector->y * ub;
    intersection->mid.x = vector->x * um;
    intersection->mid.y = vector->y * um;

    intersection->A.z =
    intersection->B.z =
    intersection->mid.z = 0;

/*
fprintf(stderr, "Line: 0,0 to %+.2f,%+.2f :: Circle: %+.2f,%+.2f R: %.2f\n", vector->x, vector->z,
           C->x, C->z, radius);
    fprintf(stderr, "Sol: %+.04f and %+.04f (%+.04f mid)\n", ua, ub, um);
*/
    if (s == 0) {
        return 1;
    }

    return 2;
}

int _circle_circle_intersect(const StewartPlatform *platform,
                             float Ra, const Point *B, float Rb,
                             Intersection *intersection) {
    const StewartConfig *c = &platform->config;
    float d = sqrt(B->x * B->x + B->y * B->y);
    float a, l;

    if (d > Ra + Rb) {
        if (c->debug) {
            fprintf(stdout, "Circle completely outside other circle\n");
        }
        return 0;
    }

    if (d < fabs(Ra - Rb)) {
        if (c->debug) {
            fprintf(stdout, "Circle contains other circle\n");
        }
        return 0;
    }

    a = (Ra * Ra - Rb * Rb + d * d) / (2 * d);
    l = sqrt(Ra * Ra - a * a);
    Point mid = {
        .x = a * B->x / d,
        .y = a * B->y / d
    };

    intersection->A.x = mid.x + l * B->y / d;
    intersection->A.y = mid.y - l * B->x / d;
    intersection->B.x = mid.x - l * B->y / d;
    intersection->B.y = mid.y + l * B->x / d;
    intersection->A.z = intersection->B.z = 0;

    if (d == Ra + Rb) {
        return 1;
    }

    return 2;
}

/* Given a 3D vector, project it to the ZY plane and return the
 * angle from the origin */
float projected_angle(const Point *servo_to_effector) {
    return atan2(servo_to_effector->z,
                 sqrt(servo_to_effector->x * servo_to_effector->x + servo_to_effector->y * servo_to_effector->y));
}

/* C  - center of circle (eg., servo_pos)
 * Cr - radius of circle (eg., servo_arm_length)
 * Cn - normal vector of plane circle is on (eg., servo_axis)
 * S  - center of sphere (eg., effector position)
 * Sr - radius of sphere (eg., effector rod length)
 * angle - Buffer of two floats to hold solutions
 *
 * Returns: 0 if no good solutions, angle[0] set to "best direction"
 *          1 or 2  -- Number of angles returned
 */
int _circle_sphere_intersect(
    const StewartPlatform *platform,
    const Point *servo_pos,
    const float servo_arm_length,
    const Point *servo_arm,
    const Point *effector_pos,
    const float effector_rod_length,
    float angle[2]) {
    const StewartConfig *c = &platform->config;
    Point servo_to_effector = {
        .x = effector_pos->x - servo_pos->x,
        .y = effector_pos->y - servo_pos->y,
        .z = effector_pos->z - servo_pos->z
    };
    float d = sqrt(servo_to_effector.x * servo_to_effector.x +
                   servo_to_effector.y * servo_to_effector.y +
                   servo_to_effector.z * servo_to_effector.z);
    Intersection intersection;
    int i;

//    fprintf(stderr, "Distance: %f\n", d);

    /* Check: Circle too far from Sphere */
    if (d > effector_rod_length + servo_arm_length) {
        if (c->debug) {
            fprintf(stdout, "Effector is too far from Base point for ROD!\n");
        }
        angle[0] = projected_angle(&servo_to_effector);
        return 0;
    }

    /* Check: Circle is exactly touching Sphere */
    if (d == effector_rod_length + servo_arm_length) {
        if (c->debug) {
            fprintf(stdout, "Effector is exact distance from Base point as ROD!\n");
        }
        angle[0] = projected_angle(&servo_to_effector);
        return 1;
    }

    /* Check: Circle is completely contained in Sphere */
    if (effector_rod_length - d > servo_arm_length) {
        if (c->debug) {
            fprintf(stdout, "Effector is too close to Base point for ROD!\n");
        }
        angle[0] = -projected_angle(&servo_to_effector);
        return 0;
    }

    /* Check: Circle is inside Sphere and exactly touching */
    if (effector_rod_length - d == servo_arm_length) {
        if (c->debug) {
            fprintf(stdout, "Effector is as close to Base as it can get "
                    "for ROD to still touch.\n");
        }
        angle[0] = -projected_angle(&servo_to_effector);
        return 1;
    }
//    _print_point("DEB servo_to_effector", -1, &servo_to_effector);

    /* There should be two points of intersection */
    if (_line_circle_intersect(platform, servo_arm, &servo_to_effector, effector_rod_length, &intersection) <= 0) {
        if (c->debug) {
            fprintf(stdout, "No line-circle intersections.\n");
        }
        angle[0] = projected_angle(&servo_to_effector);
        return 0;
    }

    Point AM = {
        .x = intersection.mid.x - intersection.A.x,
        .y = intersection.mid.y - intersection.A.y,
        .z = 0
    };

//    _print_point("DEB intersection", -1, &AM);

    float lengthAM = sqrt(AM.x * AM.x + AM.y * AM.y);
    float lengthCM = sqrt(intersection.mid.x * intersection.mid.x +
                          intersection.mid.y * intersection.mid.y);

    /* Projected coordinates to Servo plane */
    Point Sp = {
        .x = intersection.mid.x > 0 ? lengthCM : -lengthCM,
        .y = servo_to_effector.z,
        .z = 0
    };

//    _print_point("Sp", -1, &Sp);

    i = _circle_circle_intersect(platform, servo_arm_length, &Sp, lengthAM, &intersection);
    if (i <= 0) {
        if (c->debug) {
            fprintf(stdout, "No circle-circle intersections.\n");
        }
        angle[0] = -atan2(Sp.y, Sp.x);
        return 0;
    }

//    _print_point("Intersection A", -1, &intersection.A);
//    _print_point("Intersection B", -1, &intersection.B);

    angle[0] = atan2(intersection.A.y,
                    servo_arm->x > 0 ? intersection.A.x : -intersection.A.x);
    angle[1] = atan2(intersection.B.y,
                    servo_arm->x > 0 ? intersection.B.x : -intersection.B.x);

    return 2;
}

void _transform_platform_effectors(const StewartPlatform *platform, const Point *origin,
                                   const Transform *transform, Point target[], float *_matrix) {
    const StewartConfig *c = &platform->config;
    const StewartGeometry *g = &platform->geometry;

    /* If matrix isn't being returned to caller, use one allocated on the stack */
    float __matrix[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    float *matrix = _matrix ? _matrix : __matrix;
    int i;

    switch (transform->type) {
        case TRANSFORM_EUCLIDEAN:
            if (c->debug) {
                fprintf(stdout,
                        "Setting transform EUCLIDEAN:\n"
                        "  rotate    = <yaw = %.02f, pitch = %.02f, roll = %.02f>\n"
                        "  translate = <x = %.02f, y = %.02f, z = %.02f>\n"
                        "  origin    = <x = %.02f, y = %.02f, z = %.02f>\n",
                        transform->rotate.z,
                        transform->rotate.y,
                        transform->rotate.x,
                        transform->translate.x, transform->translate.y, transform->translate.z,
                        origin->x, origin->y, origin->z);
            }
            rotation_matrix_set(matrix,
                                DEG2RAD(transform->rotate.z),
                                DEG2RAD(transform->rotate.y),
                                DEG2RAD(transform->rotate.x));
            break;

        case TRANSFORM_AXIS_ANGLE:
            if (c->debug) {
                fprintf(stdout,
                        "Setting transform AXIS-ANGLE:\n"
                        "  axis      = <%+5.02f, %+5.02f, %+5.02f>\n"
                        "  angle     = %+5.02f\n"
                        "  translate = <x = %.02f, y = %.02f, z = %.02f>\n"
                        "  origin    = <x = %.02f, y = %.02f, z = %.02f>\n",
                        transform->rotate.x,
                        transform->rotate.y,
                        transform->rotate.z,
                        transform->angle,
                        transform->translate.x, transform->translate.y, transform->translate.z,
                        origin->x, origin->y, origin->z);
            }
            axis_angle_matrix_set(matrix,
                                   DEG2RAD(transform->rotate.x),
                                   DEG2RAD(transform->rotate.y),
                                   DEG2RAD(transform->rotate.z),
                                   DEG2RAD(transform->angle));
            break;

        default:
            fprintf(stderr, "Invalid transform type: %d\n", transform->type);
            return;
    }

    /* Multiply each position by the rotation matrix */
    for(i = 0; i < 6; i++) {
        Point translation = transform->translate;

        /* Add the base platform height to the translation for Z */
        translation.z += c->platform_height;
        transform_point(&target[i], &g->effector_pos[i], origin, &translation, matrix);
    }
}

void _print_point(const char *name, int index, const Point *point) {
    if (index == -1) {
        fprintf(stdout, "%s: { x: %.02f, y: %.02f, z: %.02f }\n",
                name,
                point->x, point->y, point->z);
    } else {
        fprintf(stdout, "%s[%d]: { x: %.02f, y: %.02f, z: %.02f }\n",
                name, index,
                point->x, point->y, point->z);
    }
}
