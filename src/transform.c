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
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <linux/limits.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "pca9685.h"

#include "stewart.h"
#include "stewart-pubsub.h"
#include "config.h"
#include "delay.h"

void usage(int ret) {
    fprintf(stderr,
            "usage: transform [OPTIONS] [TRANSFORM]\n"
            "\n"
            "If not supplied on the command line, the TRANSFORM value is read\n"
            "from STDIN, one line at a time.\n"
            "\n"
            "TRANSFORM can be of the form:"
            "axis-angle:\n"
            "    axis-x axis-y axis-z angle-in-degrees [TRANSLATE]\n"
            "euclidian:\n"
            "    roll pitch yaw translate-x translate-y translate-z [ORIGIN]\n"
            "\n"
            "ORIGIN is optional and specifies the coordinates to use as the origin\n"
            "for the transformation."
            "\n"
            "Options:\n"
            "-s            Simulate. Do not connect to PCA9685.\n"
            "-q            Quiet. Supress transform output.\n"
            "-d            Debug. Turn on Stewart platform debug information\n"
            "              (if local)\n"
            "-h HOST:PORT  Connect to Stewart platform on HOST:PORT\n"
            "\n"
            "If -s is not provided, transform will attempt to connect to a\n"
            "Stewart platform on i2c bus.\n\n");
    exit(ret);
}

void version() {
    fprintf(stdout,
            "transform: Stewart platform set-transform application\n"
            "Copyright (C) 2017 Intel Corporation\n"
            "Licensed under the terms of the Apache 2.0 license. See LICENSE file.\n"
            "\n"
            "Version: " VERSION "\n");
    exit(0);
}

int set_transform(const float *params, int param_count, Transform *transform, Point *origin) {
  switch (param_count) {
    case 4: /* axis-angle */
    case 7: /* axis-angle + origin */
      transform->type = TRANSFORM_AXIS_ANGLE;

      /* Roll (X-axis), Pitch (Y-axis), Yaw (Z-axis) */
      transform->rotate.x = params[0];
      transform->rotate.y = params[1];
      transform->rotate.z = params[2];

      transform->angle = params[3];
      memset(origin, 0, sizeof(*origin));
      if (param_count == 7) {
        transform->translate.x = params[4];
        transform->translate.y = params[5];
        transform->translate.z = params[6];
      } else {
        memset(&transform->translate, 0, sizeof(transform->translate));
      }
      return 0;

    case 6: /* euler angle */
    case 9: /* euler angle + origin */
      transform->type = TRANSFORM_EUCLIDEAN;

      /* Roll (X-axis), Pitch (Y-axis), Yaw (Z-axis) */
      transform->rotate.x = params[0];
      transform->rotate.y = params[1];
      transform->rotate.z = params[2];
      /* Roll (X-axis), Pitch (Y-axis), Yaw (Z-axis) */
      transform->translate.x = params[3];
      transform->translate.y = params[4];
      transform->translate.z = params[5];
      if (param_count == 9) {
        origin->x = params[6];
        origin->y = params[7];
        origin->z = params[8];
      } else {
        memset(origin, 0, sizeof(*origin));
      }
      return 1;

    default:
      return -1;
    }
}

int main(int argc, char *argv[]) {
    PCA9685 *pca = NULL;
    StewartConfig config = {
      .debug = 0
    };
    StewartPlatform *platform = NULL;
    int err = 0;
    int i;
    int use_stdin = 1; /* read from stdin for commands */
    const int ERR = 16384;
    int port;
    char *host = NULL;
    int sock = -1;
    int simulate = 0;
    int quiet = 0;
    int euler = 0;
    float *params = NULL;
    int param_count = 0;

    /* Solve for the solution angles for the given platform transform */
    Solution solutions[6];

    Transform transform = {
        .type = TRANSFORM_AXIS_ANGLE,
        .angle = ERR,
        .rotate = {
            .x = ERR,
            .y = ERR,
            .z = ERR
        },
        .translate = {
            .x = ERR,
            .y = ERR,
            .z = ERR
        }
    };
    Point origin = {
        .x = ERR,
        .y = ERR,
        .z = ERR
    };

    params = (float *)malloc(sizeof(float *) * argc);

    /* Parse command line arguments... */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || !(argv[i][1] < '0' || argv[i][1] > '9')) {
            params[param_count++] = strtof(argv[i], NULL);
            continue;
        }

        switch (argv[i][1]) {
            case '?':
                usage(0);
                break;

            case 'd':
                config.debug = 1;
                break;

            case 'q':
                quiet = 1;
                break;

            case 's':
                simulate = 1;
                break;

            case 'h':
                i++;
                if (i == argc) {
                    fprintf(stderr, "Invalid argument %d: %s", i, argv[i]);
                    usage(-1);
                }
                char *colon = strchr(argv[i], ':');
                if (!colon) {
                    fprintf(stderr, "Invalid argument %d: %s", i, argv[i]);
                    usage(-1);
                }
                *colon = '\0';
                colon++;
                host = argv[i];
                port = strtol(colon, NULL, 0);
                break;

            case 'v':
                version();
                break;

            default:
                usage(-1);
                break;
        }
    }

    /* Process parameters provided on command line */
    if (param_count == 0) {
        use_stdin = 1;
    } else {
      switch (set_transform(params, param_count, &transform, &origin)) {
        case 0: /* axis-angle */
          euler = 0;
          break;
        case 1: /* euler */
          euler = 1;
          break;
        case -1: /* error */
        default:
          usage(-1);
          break;
      }
    }

    if (!quiet) {
        fprintf(stdout, "Stewart platform transform\n"
                "Copyright (C) 2015-2017 Intel Corporation\n\n");
    }

    if (!quiet) {
        if (use_stdin) {
            fprintf(stdout, "Reading from STDIN\n");
        } else {
            fprintf(stdout, "Executing %s transform from command line.\n",
                    euler ? "EULER" : "AXIS-ANGLE");
        }
    }

    /* If transform is not connecting to a remote server, initialize the
     * stewart platform locally */
    if (host == NULL) {
        /* Initialize the default values for the Stewart platform as
         * documented at https://01.org/developerjourney/recipe/stewert-platform */
        config_get(&config);

        /* Create the Stewart platform solver as configured */
        platform = stewart_platform_create(&config);
        if (!platform) {
            fprintf(stderr, "Could not create a Stewart platform solver!\n");
            return -1;
        }

        if (config.debug) {
            stewart_platform_dump(platform);
        }

        if (!simulate) {
            /* Attempt to connect to PCA9685 in order to program the servo locations */
            pca = pca9685_open(1, 0x40);
            if (!pca) {
                fprintf(stderr, "Could not initialize PCA9685!\n");
                err = 1;
                goto terminate;
            }

            err = pca9685_set_pulse_frequency(pca, PULSE_WIDTH_FREQUENCY);
            if (err) {
                fprintf(stderr, "Could not set PWM frequency!\n");
                err = 2;
                goto terminate;
            }
        }
    }

    fd_set read_fd;
    int fd = -1;
    if (use_stdin) {
        fd = fileno(stdin);
        fcntl(fd, F_SETFL,  O_NONBLOCK);
        FD_ZERO(&read_fd);
        FD_SET(fd, &read_fd);
    }

    long then, now;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    then = now = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    if (host) {
        struct addrinfo hint = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP
        };

        struct addrinfo *res;
        fprintf(stdout, "Looking up %s...", host);
        if (getaddrinfo(host, NULL, &hint, &res) == -1 || res == NULL) {
            fprintf(stdout, "\n");
            fprintf(stderr, "Error: Unable to get host address for %s: %s\n", host, strerror(errno));
            goto terminate;
        }

        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == -1) {
            fprintf(stderr, "Error: Unable to open socket: %s\n", strerror(errno));
            goto terminate;
        }
        ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);

        if (!quiet) {
            fprintf(stdout, "%s\n", inet_ntoa(((struct sockaddr_in *)res->ai_addr)->sin_addr));
        }
        freeaddrinfo(res);

        if (!quiet) {
            fprintf(stdout, "Connecting to %s:%d...", host, port);
        }

        if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
            fprintf(stdout, "\n");
            fprintf(stderr, "Error: Unable to connect socket: %s\n", strerror(errno));
            goto terminate;
        }

        if (!quiet) {
            fprintf(stdout, "done\n");
        }
    }

    while (1) {
        float params[9]; /* max of 9 parameters can be provided */
        int param_count;

        /* Only output to the PWM once per cycle */
        gettimeofday(&tv, NULL);
        now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        if (now - then < 1000 / PULSE_WIDTH_FREQUENCY) {
            delay(1000 * (1000 / PULSE_WIDTH_FREQUENCY - (now - then)));
        }
        then = now;

        if (use_stdin) {
            char buf[1024];
            buf[0] = '\0';
            /* Block waiting for data to be available */
            if (select(fd + 1, &read_fd, NULL, NULL, NULL) > 0) {
                char *res;
                /* Read as much data as is available (only use the most
                 * recent value) */
                do {
                    res =  fgets(buf, sizeof(buf), stdin);
                } while (res == buf);

                if (buf[0] != '\0') {
                    param_count = sscanf(buf, "%f %f %f %f %f %f %f %f %f\n",
                            &params[0], &params[1], &params[2],
                            &params[3], &params[4], &params[5],
                            &params[6], &params[7], &params[8]);
                    if (set_transform(params, param_count, &transform, &origin) == -1) {
                        fprintf(stderr, "Error parsing input!\n");
                        fprintf(stderr, "'%s'\n", buf);
                    }
                }
            }

            /* If stdin is EOF and no buffers were read, exit... */
            if (feof(stdin) && buf[0] == '\0') {
                fprintf(stderr, "EOF");
                break;
            }

            if (ferror(stdin) && errno && errno != EAGAIN) {
                fprintf(stderr, "Error reading from STDIN: %s\n", strerror(errno));
                break;
            }

            fflush(stdout);
        }

        if (!quiet) {
            if (origin.x != 0 || origin.y != 0 || origin.z != 0) {
                fprintf(stdout, "Origin: X: %+5.02f, Y: %+5.02f, Z: %+5.02f\n",
                        origin.x, origin.y, origin.z);
            }
        }

        StewartMessage message = {
            .version = STEWART_PROTOCOL,
            .size = sizeof(message)
        };

        if (!quiet) {
            switch (transform.type) {
                case TRANSFORM_AXIS_ANGLE:
                    fprintf(stdout, "X: %+5.02f, Y: %+5.02f, Z: %+5.02f, Angle: %+5.02f\n",
                            transform.rotate.x,
                            transform.rotate.y,
                            transform.rotate.z,
                            transform.angle);
                    message.type = STEWART_MESSAGE_SET_AXISANGLE;
                    message.axisAngle.x = transform.rotate.x;
                    message.axisAngle.y = transform.rotate.y;
                    message.axisAngle.z = transform.rotate.z;
                    message.axisAngle.translate.x = transform.translate.x;
                    message.axisAngle.translate.y = transform.translate.y;
                    message.axisAngle.translate.z = transform.translate.z;
                    message.axisAngle.angle = transform.angle;
                    break;
                case TRANSFORM_EUCLIDEAN:
                    fprintf(stdout,
                            "Roll: %+5.02f, Pitch: %+5.02f, Yaw: %+5.02f,"
                            "X: %+5.02f, Y: %+5.02f, Z: %+5.02f\n",
                            transform.rotate.x,
                            transform.rotate.y,
                            transform.rotate.z,
                            transform.translate.x,
                            transform.translate.y,
                            transform.translate.z);
                    message.type = STEWART_MESSAGE_SET_EUCLIDEAN;
                    /* Yaw (Z-axis), Pitch (Y-axis), Roll (X-axis) */
                    message.euclidean.yaw = transform.rotate.z;
                    message.euclidean.pitch = transform.rotate.y;
                    message.euclidean.roll = transform.rotate.x;
                    message.euclidean.translate.x = transform.translate.x;
                    message.euclidean.translate.y = transform.translate.y;
                    message.euclidean.translate.z = transform.translate.z;
                    break;
            }
        }

        if (host) {
            if (send(sock, &message, sizeof(message), MSG_WAITALL) == -1) {
                fprintf(stderr, "Error sending to Stewart platform: %s\n", strerror(errno));
                goto terminate;
            }

            if (!use_stdin) {
                break;
            }

             continue;
        }

        stewart_get_solutions(platform, &origin, &transform, solutions, NULL);

        int constrained = 0;
        for (i = 0; i < 6; i++) {
            const char *type = "UNKNOWN";

            switch (solutions[i].type & MASK) {
            case SOLUTION:
                if (solutions[i].type & LIMITED) {
                    type = "LIMITED";
                    constrained++;
                    if (solutions[i].type & TRIM) {
                        type = "LIMITED TRIM";
                    }
                } else {
                    type = "SOLUTION";
                }
                break;
            case IMPOSSIBLE:
                type = "IMPOSSIBLE";
                constrained++;
                break;
            default:
                fprintf(stderr, "Type: %d\n", solutions[i].type);
                break;
            }

            if (!quiet) {
                fprintf(stdout, "Servo %d: %.02f [%s]\n", i, solutions[i].angle, type);
            }
        }

        /* Send servo positions to servos */
        for (i = 0; !simulate && i < 6; i++) {
            pca9685_set_channel_pulse(
                pca, i, 0, RADIANS_TO_PULSE_WIDTH(DEG2RAD(solutions[i].angle)));
        }

        if (!use_stdin) {
            break;
        }
    }

terminate:
    if (platform) {
        stewart_platform_delete(platform);
    }

    if (pca) {
        pca9685_close(pca);
    }

    if (sock != -1) {
        close(sock);
    }

    return err;
}
