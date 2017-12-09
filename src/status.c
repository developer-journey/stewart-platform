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
            "usage: status -h HOST:PORT [POLL-RATE]\n"
            "\n"
            "POLL-RATE     Times per second to ask for status. Max 50.\n"
            "              If not specified, status runs a one-shot, exiting\n"
            "              after fetching one status message.\n"
            "-q            Quiet. Suppress non-status output.\n"
            "-h HOST:PORT  Connect to Stewart platform on HOST:PORT\n"
            "\n\n");
    exit(ret);
}

void version() {
    fprintf(stdout,
            "status: Stewart platform status viewer\n"
            "Copyright (C) 2017 Intel Corporation\n"
            "Licensed under the terms of the Apache 2.0 license. See LICENSE file.\n"
            "\n"
            "Version: " VERSION "\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    StewartConfig config;
    int err = 0;
    int port;
    char *host = NULL;
    long rate = -1;
    int quiet = 0;
    int i;
    int sock = -1;

    /* Parse command line arguments... */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'q':
                    quiet = 1;
                    break;

                case '?':
                    usage(0);
                    break;

                case 'h':
                    i++;
                    if (i == argc) {
                        fprintf(stderr, "-h HOST:PORT must be specified. Invalid argument %d: %s", i, argv[i]);
                        usage(-1);
                    }
                    char *colon = strchr(argv[i], ':');
                    if (!colon) {
                        fprintf(stderr, "-h HOST:PORT must be specified. Invalid argument %d: %s", i, argv[i]);
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

            continue;
        }

        if (rate == -1) {
            rate = strtol(argv[i], NULL, 0);
            if (rate <= 0 || rate > 50) {
                fprintf(stderr, "POLL-RATE must be in the range of 0-50\n");
                usage(-1);
            }

            rate = 1000 / rate;
        } else {
            fprintf(stderr, "Invalid argument %d: %s\n", i, argv[i]);
            usage(-1);
        }
    }

    if (host == NULL) {
        fprintf(stderr, "-h HOST:PORT must be specified\n");
        usage(-1);
    }

    /* Initialize the default values for the Stewart platform as
     * documented at https://01.org/developerjourney/recipe/stewert-platform */
    config_get(&config);

    long then, now;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    then = now = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    struct addrinfo hint = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };

    struct addrinfo *res;
    if (!quiet) {
        fprintf(stdout, "Looking up %s...", host);
        fflush(stdout);
    }
    if (getaddrinfo(host, NULL, &hint, &res) == -1 || res == NULL) {
        if (!quiet) {
            fprintf(stdout, "\n");
        }
        fprintf(stderr, "Error: Unable to get host address for %s: %s\n", host, strerror(errno));
        goto terminate;
    }

    if (!quiet) {
        fprintf(stdout, "%s\n", inet_ntoa(((struct sockaddr_in *)res->ai_addr)->sin_addr));
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        fprintf(stderr, "Error: Unable to open socket: %s\n", strerror(errno));
        goto terminate;
    }
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);

    freeaddrinfo(res);

    if (!quiet) {
        fprintf(stdout, "Connecting to %s:%d...", host, port);
        fflush(stdout);
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
        if (!quiet) {
            fprintf(stdout, "\n");
        }
        fprintf(stderr, "Error: Unable to connect socket: %s\n", strerror(errno));
        goto terminate;
    }

    if (!quiet) {
        fprintf(stdout, "done\n");
    }

    do {
        /* Only output to the PWM once per cycle */
        gettimeofday(&tv, NULL);
        now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        if (now - then < rate) {
            delay(1000 * (rate - (now - then)));
        }
        then = now;

        StewartMessage message = {
            .type = STEWART_MESSAGE_GET_STATUS
        };

        if (!quiet) {
            fprintf(stdout, "Sending STEWART_MESSAGE_GET_STATUS\n");
        }

        if (send(sock, &message, sizeof(message), MSG_WAITALL) == -1) {
            fprintf(stderr, "Error: Unable to send message to Stewart platform\n%s\n",
                    strerror(errno));
            goto terminate;
        }

        if (recv(sock, &message, sizeof(message), MSG_WAITALL) == -1) {
            fprintf(stderr, "Error: Unable to receive message from Stewart platform:\n%s\n",
                    strerror(errno));
            goto terminate;
        }

        if (message.type != STEWART_MESSAGE_STATUS) {
            fprintf(stderr, "Error: Invalid message type received: %d\n", message.type);
            continue;
        }

        if (!quiet) {
            fprintf(stdout, "Received STEWART_MESSAGE_GET_STATUS\n");
        }

        fprintf(stdout, "Axis-Angle: <%+.02f, %+.02f, %+.02f>, Angle: %+.02fdeg\n",
                message.axisAngle.x, message.axisAngle.y, message.axisAngle.z,
                message.axisAngle.angle);
    } while (rate != -1);

terminate:
    if (sock != -1) {
        close(sock);
    }

    return err;
}
