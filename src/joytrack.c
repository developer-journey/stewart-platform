/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <linux/joystick.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"

typedef struct {
    float x;
    float y;
} Axis;

typedef struct {
    Axis left;
    Axis right;
    Axis left_back;  /* x only */
    Axis right_back; /* x only */
} Joystick;

static int debug = 0;

int joystick_parse(const struct js_event *event, Joystick *joystick) {
    if (event->type & JS_EVENT_BUTTON) {
        switch (event->number) {
        case 14: /* (X) - Reset transform */
            memset(joystick, 0, sizeof(*joystick));
            return 1;

        case 10: /* L1 */
        case 8: /* L2 */
            if (event->value == 0) {
                joystick->left_back.x = 0;
                return 1;
            }
            break;

        case 11: /* R1 */
        case 9: /* R2 */
            if (event->value == 0) {
                joystick->right_back.x = 0;
                return 1;
            }
            break;

        default:
            if (debug) {
                fprintf(stderr, "Button %d = %s\n",
                        event->number, event->value ? "ON" : "OFF");
            }
            break;
        }
    }

    if (event->type & JS_EVENT_AXIS) {
        float alpha = 1.0 * event->value / 32767.0;

        switch (event->number) {
        case 0: /* Rotate - Roll */
            joystick->left.y = alpha;
            return 1;

        case 1: /* Rotate - Pitch */
            joystick->left.x = alpha;
            return 1;

        case 2: /* Translate - X */
            joystick->right.x = alpha;
            return 1;

        case 3: /* Translate - Y */
            joystick->right.y = alpha;
            return 1;

        case 13: /* R2 */
            alpha += 1.0;
            alpha /= 2.0;
            joystick->right_back.x = alpha;
            return 1;

        case 15: /* R1 */
            alpha += 1.0;
            alpha /= 2.0;
            joystick->right_back.x = -alpha;
            return 1;

        case 12: /* L2 */
            alpha += 1.0;
            alpha /= 2.0;
            joystick->left_back.x = alpha;
            return 1;

        case 14: /* L1 */
            alpha += 1.0;
            alpha /= 2.0;
            joystick->left_back.x = -alpha;
            return 1;

        default:
            if (debug) {
                fprintf(stdout, "Axis %d [IGNORED]: %8d\n", event->number, event->value);
            }
            break;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int err = 0;
    char *device;
    struct js_event event;
    Joystick joystick;
    fd_set read_fd;
    int fd = -1;
    int axis, buttons;
    char name[PATH_MAX];

    memset(&joystick, 0, sizeof(joystick));

    /* Parse command line arguments... */
    if (argc < 2) {
        fprintf(stderr,
                "usage: joytrack device-id | transform\n");
        fprintf(stderr,
                "This program will output Axis-Angle data in the format:\n"
                " |X| |Y| |Z| Angle(deg)\n");
        return 1;
    }

    device = argv[1];

    /* Attempt to open the Joystick device */
    fd = open(device, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Unable to open joystick: %s\n", device);
        fprintf(stderr, "Error %d: %s\n", errno, strerror(errno));
        goto terminate;
    }

    ioctl(fd, JSIOCGAXES, &axis);
    ioctl(fd, JSIOCGBUTTONS, &buttons);
    ioctl(fd, JSIOCGNAME(sizeof(name)), &name);

    fcntl(fd, F_SETFL,  O_NONBLOCK);

    if (debug) {
        fprintf(stderr, "Joystick: %s [%d buttons, %d axis]\n", name, axis, buttons);
    }

    /*
     * Loop on select on Joystick input
     * Read all Joystick input, translating values into Axis Angle
     */
    FD_ZERO(&read_fd);
    FD_SET(fd, &read_fd);

    while (select(fd + 1, &read_fd, NULL, NULL, NULL) > 0) {
        int update = 0;

        if (!FD_ISSET(fd, &read_fd)) {
            fprintf(stderr, "Select returned with no data!\n");
            break;
        }

        while (read(fd, &event, sizeof(event)) > 0) {
            update |= joystick_parse(&event, &joystick);
        }

        if (errno != EAGAIN) {
            break;
        }

        if (!update) {
            continue;
        }

        /* Translate Joystick input into an Axis Angle
         *
         * Set the Angle to be based on the magnitude of the joystick
         * axis move, clamped to 1.0, and multiply by (MAX_ROLL+MAX_PITCH)/2
         *
         * NOTE: 5.1deg hits impossible solutions in full X/Y directed
         * moves, however it allows greater control in lower angled
         * directions.
         */
        float angle_length = sqrt(joystick.left.x * joystick.left.x +
                                  joystick.left.y * joystick.left.y);
        if (angle_length > 0) {
            float clamped = angle_length > 1.0 ? 1 : angle_length;
            fprintf(stdout, "%f %f %f %f\n",
                    joystick.left.x / angle_length,
                    0.0f,
                    joystick.left.y / angle_length,
                    clamped * (MAX_ROLL + MAX_PITCH) / 2.0f);
        } else {
            fprintf(stdout, "0 0 1 0\n");
        }

        fflush(stdout);
    }

    fprintf(stderr, "Error reading from Joystick!\n"
            "Error %d: %s\n", errno, strerror(errno));

terminate:

    if (fd >= 0) {
        close(fd);
    }

    return err;
}
