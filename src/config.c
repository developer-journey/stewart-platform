/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include "stewart.h"
#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int config_write(StewartConfig *c) {
    FILE *cfg = fopen("stewart.cfg", "w");
    int i;
    if (cfg == NULL) {
        fprintf(stderr, "ERROR: Unable to open 'stewart.cfg': %s\n", strerror(errno));
        return -1;
    }
    if (fprintf(cfg, "version=\"" VERSION "\"\n") < 0) {
        fprintf(stderr, "Error: Unable to write to 'stewart.cfg': %s\n", strerror(errno));
        fclose(cfg);
        return -1;
    }
    for (i = 0; i < 6; i++) {
        if (c->servo_trim[i] != 0.0) {
            if (fprintf(cfg, "trim[%d]=%f\n", i, c->servo_trim[i]) < 0) {
                fprintf(stderr, "Error: Unable to write to 'stewart.cfg': %s\n", strerror(errno));
                fclose(cfg);
                return -1;
            }
        }
    }
    if (fclose(cfg) == -1) {
        fprintf(stderr, "Error: Unable to close 'stewart.cfg': %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

void config_get(StewartConfig *c) {
    /*
     *
     * To change these values, modify config.h
     *
     */
    c->servo_trim[0] =        TRIM_0;
    c->servo_trim[1] =        TRIM_1;
    c->servo_trim[2] =        TRIM_2;
    c->servo_trim[3] =        TRIM_3;
    c->servo_trim[4] =        TRIM_4;
    c->servo_trim[5] =        TRIM_5;

    c->servo_min =            SERVO_MIN;
    c->servo_max =            SERVO_MAX;
    c->servo_arm_length =     SERVO_ARM_LENGTH;
    c->control_rod_length =   CONTROL_ROD_LENGTH;
    c->platform_height =      PLATFORM_HEIGHT;
    c->effector_radius =      EFFECTOR_RADIUS;
    c->base_radius =          BASE_RADIUS;
    c->theta_base =           THETA_BASE;
    c->theta_effector =       THETA_EFFECTOR;

    c->servo_orientation[0] = SERVO_ORIENTATION_0;
    c->servo_orientation[1] = SERVO_ORIENTATION_1;
    c->servo_orientation[2] = SERVO_ORIENTATION_2;
    c->servo_orientation[3] = SERVO_ORIENTATION_3;
    c->servo_orientation[4] = SERVO_ORIENTATION_4;
    c->servo_orientation[5] = SERVO_ORIENTATION_5;

    c->servo_direction[0] =   SERVO_DIRECTION_0;
    c->servo_direction[1] =   SERVO_DIRECTION_1;
    c->servo_direction[2] =   SERVO_DIRECTION_2;
    c->servo_direction[3] =   SERVO_DIRECTION_3;
    c->servo_direction[4] =   SERVO_DIRECTION_4;
    c->servo_direction[5] =   SERVO_DIRECTION_5;

    FILE *cfg = fopen("stewart.cfg", "r");
    if (cfg != NULL) {
        char *version = NULL;

        if (fscanf(cfg, "version = \"%m[^\"]\"\n", &version) != 1) {
            version = strdup(VERSION);
        }

        if (!strcmp(VERSION, version)) {
            char buf[1024];
            int i;
            float v;
            do {
                if (buf != fgets(buf, sizeof(buf), cfg)) {
                    break;
                }
    	        if (sscanf(buf, " trim [ %d ] = %f\n", &i, &v) != 2) {
                    break;
                }
                if (i < 0 || i > 5) {
                    fprintf(stderr, "Invalid trim index %d in stewart.cfg!\n",
                            i);
                    break;
                }
                if (c->debug) {
                    fprintf(stdout, "Using TRIM override for %d from stewart.cfg: %5.02fdeg\n", i, v);
                }
                c->servo_trim[i] = v;
            } while (1);
        } else {
            fprintf(stderr, "WARNING: stewart.cfg is invalid! Ignoring.\n");
        }

        fclose(cfg);
        free(version);
    }
}
