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
#include <ifaddrs.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <linux/limits.h>

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "pca9685.h"

#include "stewart.h"
#include "config.h"
#include "stewart-pubsub.h"
#include "delay.h"

int quiet = 0;

#define MAX_CONNECTIONS 11 /* 10 CLIENTs, and 1 SERVER */
struct pollfd fds[MAX_CONNECTIONS];
char buffer[MAX_CONNECTIONS][sizeof(StewartMessage) * 2];
int bufferIndex[MAX_CONNECTIONS];
StewartMessage pending[MAX_CONNECTIONS];
Transform transform;

float origin[3] = { 0, 0, 0 };
Solution solutions[6];
float rotationMatrix[9];

void usage(int ret) {
    fprintf(stderr, "usage: server -p PORT [-i INTERFACE]\n"
            "\n"
            "-p PORT       Listen on PORT for socket connections\n"
            "-i INTERFACE  Bind to INTERFACE to listen accept connections\n"
            "-q            Quiet. Supress transform output.\n"
            "-d            Debug. Turn on Stewart platform debug information (if local)\n"
            "-s            Simulate. Don't try and connect to the PCA9685.\n"
            "-?            Help\n"
            "-v            Version\n"
            "\n"
            "See PROTOCOL for details on the Stewart platform protocol.\n");
    exit(ret);
}

void version() {
    fprintf(stdout,
            "server: Stewart platform server control\n"
            "Copyright (C) 2017 Intel Corporation\n"
            "Licensed under the terms of the Apache 2.0 license. See LICENSE file.\n"
            "\n"
            "Version: " VERSION "\n");
    exit(0);
}

int init_platform(StewartConfig *config, PCA9685 **pca, StewartPlatform ** platform) {
    int err = 0;

    /* Initialize the default values for the Stewart platform as
     * documented at https://01.org/developerjourney/recipe/stewert-platform */
    config_get(config);

    /* Create the Stewart platform solver as configured */
    *platform = stewart_platform_create(config);
    if (!*platform) {
        fprintf(stderr, "Could not create a Stewart platform solver!\n");
        return -1;
    }

    if (config->debug) {
        stewart_platform_dump(*platform);
    }

    /* If pca == NULL then this is a simulator; don't connect to the PCA9685 */
    if (pca) {
        /* Attempt to connect to PCA9685 in order to program the servo locations */
        *pca = pca9685_open(0, 0x40);
        if (!*pca) {
            fprintf(stderr, "Could not initialize PCA9685!\n");
            err = -2;
            goto terminate;
        }

        err = pca9685_set_pulse_frequency(*pca, PULSE_WIDTH_FREQUENCY);
        if (err) {
            fprintf(stderr, "Could not set PWM frequency!\n");
            err = -3;
            goto terminate;
        }
    }

    return 0;

terminate:
    if (*platform) {
        stewart_platform_delete(*platform);
        *platform = NULL;
    }

    if (pca && *pca) {
        pca9685_close(*pca);
        *pca = NULL;
    }

    return err;
}

void getStatus(StewartConfig *config, StewartPlatform *platform, StewartStatus *status) {
    struct timeval elapsed;
    int i;

    memset(status, 0, sizeof(*status));

    elapsed = stewart_platform_get_elapsed(platform);
    status->sec = elapsed.tv_sec;
    status->usec = elapsed.tv_usec;

    for (i = 0; i < 6; i++) {
        status->servos[i].angle = solutions[i].angle;
        status->servos[i].trim = config->servo_trim[i];
    }

    memcpy(status->origin, origin, sizeof(origin));
    memcpy(status->rotation, rotationMatrix, sizeof(rotationMatrix));
}

void processMessage(StewartConfig *config, StewartPlatform *platform, PCA9685 *pca, int index, const StewartMessage *message) {
    static long now = 0;
    static long then = 0;
    struct timeval tv;
    int i;
    Point _origin = { .x = origin[0], .y = origin[1], .z = origin[2] };

    if (message->version != STEWART_PROTOCOL ||
        message->size != sizeof(*message)) {
        fprintf(stdout, "Invalid message received %d %d.\n", message->version, message->size);
        return;
    }

    switch (message->type) {
        case STEWART_MESSAGE_SET_AXISANGLE:
            memset(&transform, 0, sizeof(transform));

            fprintf(stdout, "Axis-Angle: <%+.02f, %+.02f, %+.02f>, Angle: %+.02fdeg\n",
                    message->axisAngle.x, message->axisAngle.y, message->axisAngle.z,
                    message->axisAngle.angle);
            fprintf(stdout, "Translation: <X: %+.02f, Y: %+.02f, Z: %+.02f>\n",
                    message->axisAngle.translate.x,
                    message->axisAngle.translate.y,
                    message->axisAngle.translate.z);

            transform.type = TRANSFORM_AXIS_ANGLE;
            transform.rotate.x = message->axisAngle.x;
            transform.rotate.y = message->axisAngle.y;
            transform.rotate.z = message->axisAngle.z;
            transform.angle = message->axisAngle.angle;
            transform.translate.x = message->axisAngle.translate.x;
            transform.translate.y = message->axisAngle.translate.y;
            transform.translate.z = message->axisAngle.translate.z;

            break;

        case STEWART_MESSAGE_SET_EUCLIDEAN:
            memset(&transform, 0, sizeof(transform));

            fprintf(stdout, "Rotation: <Roll: %+.02f, Pitch: %+.02f, Yaw: %+.02f>\n",
                    message->euclidean.roll,
                    message->euclidean.pitch,
                    message->euclidean.yaw);
            fprintf(stdout, "Translation: <X: %+.02f, Y: %+.02f, Z: %+.02f>\n",
                    message->euclidean.translate.x,
                    message->euclidean.translate.y,
                    message->euclidean.translate.z);

            transform.type = TRANSFORM_EUCLIDEAN;
            transform.rotate.x = message->euclidean.pitch;
            transform.rotate.y = message->euclidean.yaw;
            transform.rotate.z = message->euclidean.roll;
            transform.translate.x = message->euclidean.translate.x;
            transform.translate.y = message->euclidean.translate.y;
            transform.translate.z = message->euclidean.translate.z;
            break;

        case STEWART_MESSAGE_SET_TRIM:
            if (!quiet) {
                fprintf(stdout, "Setting trim %d = %+5.02fdeg\n",
                        message->trim.servo, message->trim.angle);
            }
            if (message->trim.servo >= 0 && message->trim.servo <= 5) {
                config->servo_trim[message->trim.servo] = message->trim.angle;
                config_write(config);
            }
            /* Fall through to set the servos to their value based on the new
             * trim values */
            break;

        case STEWART_MESSAGE_GET_STATUS:
            if (!quiet) {
                fprintf(stdout, "Status requested. Sending...\n");
            }
            memset(&pending[index], 0, sizeof(pending[index]));
            pending[index].type = STEWART_MESSAGE_STATUS;
            pending[index].version = STEWART_PROTOCOL;
            pending[index].size = sizeof(pending[index]);
            getStatus(config, platform, &pending[index].status);
            fds[index].events |= POLLOUT;
            return;

        default:
            fprintf(stderr, "Warning: Invalid message type: %d\n", message->type);
            return;
    }

    stewart_get_solutions(platform, &_origin, &transform, solutions, rotationMatrix);

    int constrained = 0;
    for (i = 0; i < 6; i++) {
        const char *type = "UNKNOWN";

        switch (solutions[i].type & MASK) {
            case SOLUTION:
                if (solutions[i].type & LIMITED) {
                    type = "LIMITED";
                    constrained++;
                } else {
                    type = "SOLUTION";
                }
                break;
            case IMPOSSIBLE:
                type = "IMPOSSIBLE";
                constrained++;
                break;
            default:
                fprintf(stderr, "Error: Invalid solution type: %d\n", solutions[i].type);
                break;
        }

        if (!quiet) {
            fprintf(stdout, "Servo %d: %.02fdeg [%s]\n", i, solutions[i].angle, type);
        }
    }

    /* Only output to the PWM once per cycle */
    gettimeofday(&tv, NULL);
    now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (now - then < 1000 / PULSE_WIDTH_FREQUENCY) {
        delay(1000 * (1000 / PULSE_WIDTH_FREQUENCY - (now - then)));
    }
    then = now;

    /* Send servo positions to servos */
    if (pca) {
        for (i = 0; i < 6; i++) {
            pca9685_set_channel_pulse(
                pca, i, 0, RADIANS_TO_PULSE_WIDTH(DEG2RAD(solutions[i].angle)));
        }
    }
}

int main(int argc, char *argv[]) {
    PCA9685 *pca = NULL;
    StewartConfig _c;
    StewartConfig *config = &_c;
    StewartPlatform *platform = NULL;
    int err = 0;
    int sock = -1;
    int i, port = -1;
    char *iface = "lo";
    int simulate = 0;
    int ret;

    struct ifaddrs *ifaddr = NULL, *p;
    char hostIpAddr[NI_MAXHOST];

    config->debug = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 's':
                    simulate = 1;
                    break;

                case 'p': /* next is port */
                    i++;
                    if (i >= argc) {
                        usage(-1);
                    }
                    port = strtol(argv[i], NULL, 0);
                    break;
                case 'q':
                    quiet = 1;
                    break;
                case 'd':
                    config->debug = 1;
                    break;

                case '?':
                    usage(0);
                    break;
                case 'v':
                    version();
                    break;
                case 'i':
                    i++;
                    if (i >= argc) {
                        usage(-1);
                    }
                    iface = argv[i];
                    break;
            }
        }
    }

    if (port == -1) {
        usage(-1);
    }

    if (!quiet && simulate) {
        fprintf(stdout, "Simulating PCA9685.\n");
    }

    if (init_platform(config, simulate ? NULL : &pca, &platform)) {
        fprintf(stderr, "Error: Unable to initializing Stewart platform.\n");
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == -1) {
        fprintf(stderr, "Error: Unable to create socket: %s\n", strerror(errno));
        goto terminate;
    }

    if (iface != NULL && !quiet) {
        fprintf(stdout, "Interface: %s\n", iface);
    }

    if (port && !quiet) {
        fprintf(stdout, "Port: %d\n", port);
    }

    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "Error: Unable to query for interface names: %s", strerror(errno));
        goto terminate;
    }

    for (p = ifaddr; p; p = p->ifa_next) {
        if (!p->ifa_addr ||
            (p->ifa_addr->sa_family != AF_INET) ||
            strcmp(p->ifa_name, iface)) {
            continue;
        }
        err = getnameinfo(p->ifa_addr, sizeof(*p->ifa_addr),
                          hostIpAddr, sizeof(hostIpAddr), NULL, 0, NI_NUMERICHOST);
        if (err != 0) {
            fprintf(stderr, "Error: Unable to get interface names: %s", strerror(errno));
            freeifaddrs(ifaddr);
            goto terminate;
        }

        break;
    }
    freeifaddrs(ifaddr);
    if (p == NULL) {
        fprintf(stderr, "Error: Unable to find interface %s.\n", iface);
        goto terminate;
    }

    if (!quiet) {
        fprintf(stdout, "Binding to %s: %s:%d\n", iface, hostIpAddr, port);
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        fprintf(stderr, "Error: Unable to SO_REUSEADDR: %s\n", strerror(errno));
        goto terminate;
    }

    struct sockaddr_in sin;
    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;
    inet_aton(hostIpAddr, &sin.sin_addr);
    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        fprintf(stderr, "Error: Unable to bind to %s: %s:%d:\n%s\n",
                iface, hostIpAddr, port, strerror(errno));
        goto terminate;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    if (listen(sock, 10) == -1) {
        fprintf(stderr, "Error: Unable to listen on socket: %s\n",
                   strerror(errno));
        goto terminate;
    }

    fds[0].fd = sock;
    fds[0].events = POLLIN;
    for (i = 1; i < sizeof(fds) / sizeof(fds[0]); i++) {
        fds[i].fd = -1;
    }
    int maxIndex = 1;

    while (1) {
        if (!quiet) {
            fprintf(stderr, "Listening to connections %d for data and for new connections.\n", maxIndex);
        }

        int resCount = poll(fds, maxIndex, -1);
        if (resCount == -1) {
            fprintf(stderr, "Error: Poll returned an error: %s\n", strerror(errno));
            goto terminate;
        }

        /* Check the LISTEN socket for new connections */
        if (fds[0].revents & POLLIN) {
            resCount--;

            /* socket accept is ready! */
            struct sockaddr_in peerAddr;
            socklen_t peerAddrSize = sizeof(peerAddr);
            int peer;
            while (1) {
                peer = accept(fds[0].fd, (struct sockaddr *)&peerAddr, &peerAddrSize);
                if (peer == -1 && errno == EAGAIN) {
                    break;
                }
                if (peer == -1) {
                    fprintf(stderr, "Error: Unable to accept connect: %s\n",
                        strerror(errno));
                    goto terminate;
                }

                if (!quiet) {
                    fprintf(stdout, "Connection from %s on %d (%d)\n",
                            inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port), peer);
                }

                if (maxIndex == MAX_CONNECTIONS) {
                    fprintf(stderr, "Error: Too many connections. Closing new connection.\n");
                    close(peer);
                    continue;
                }

                fcntl(peer, F_SETFL,  O_NONBLOCK);

                fds[maxIndex].fd = peer;
                fds[maxIndex].events = POLLIN | POLLHUP | POLLNVAL | POLLERR;
                fds[maxIndex].revents = 0;
                bufferIndex[maxIndex] = 0;
                pending[maxIndex].type = STEWART_MESSAGE_INVALID;
                memset(buffer[maxIndex], 0, sizeof(buffer[maxIndex]));

                maxIndex++;
            }
        }

        if (!quiet) {
            fprintf(stderr, "Checking %d sockets for data.\n", maxIndex - 1);
        }

        int collapseTo = 0;

        /* Loop through all of the watched sockets until resCount descriptors
         * have been handled */
        for (i = 1; i < maxIndex && resCount > 0; i++) {
            if (fds[i].revents) {
                resCount--;
            }

            if (fds[i].revents & POLLIN) {
                while (1) {
                    ret = recv(fds[i].fd, buffer[i] + bufferIndex[i], sizeof(StewartMessage), MSG_DONTWAIT);
                    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    }

                    if (ret == 0) {
                        fprintf(stdout, "Socket disconnected.\n");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        fds[i].events = fds[i].revents = 0;
                        bufferIndex[i] = 0;
                        /* If an array collapse isn't already occurring, start collapsing
                         * to replace this now unused slot */
                        if (!collapseTo) {
                            collapseTo = i;
                        }
                        break;
                    }

                    if (!quiet) {
                        fprintf(stdout, "%d bytes received. Waiting for at least %ld\n", ret, sizeof(StewartMessage));
                    }

                    bufferIndex[i] += ret;
                    if (bufferIndex[i] >= sizeof(StewartMessage)) {
                        if (!quiet) {
                            fprintf(stdout, "Message received with %d bytes (%zu remaining.)\n",
                                   bufferIndex[i], bufferIndex[i] - sizeof(StewartMessage));
                        }
                        bufferIndex[i] -= sizeof(StewartMessage);
                        processMessage(config, platform, pca, i, (StewartMessage *)buffer[i]);
                        memcpy(buffer[i], &buffer[i][sizeof(StewartMessage)], bufferIndex[i]);
                    }
                }
            }

            if (fds[i].revents & POLLOUT) {
                if (!quiet) {
                    fprintf(stdout, "Data OUT ready %d\n", fds[i].fd);
                }
                if (pending[i].type != STEWART_MESSAGE_INVALID) {
                    ret = send(fds[i].fd, &pending[i], sizeof(pending[i]), MSG_DONTWAIT);
                    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    }
                    if (ret != sizeof(pending[i])) {
                        /* If this gets triggered, then we need to start tracking how many
                         * bytes of the packet *were* sent and continue sending the data
                         * message until the entire frame is sent. This shouldn't trigger
                         * so long as the payload is less than a TCP packet size */
                        fprintf(stderr, "Error: Sending did not transmit entire block!\n");
                        goto terminate;
                    }
                }
                fds[i].events &= ~POLLOUT;
            }

            if (fds[i].revents & (POLLHUP | POLLNVAL | POLLERR)) {
                fprintf(stdout, "Socket error and disconnect.\n");
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = fds[i].revents = 0;
                bufferIndex[i] = 0;
                /* If an array collapse isn't already occurring, start collapsing
                 * to replace this now unused slot */
                if (!collapseTo) {
                    collapseTo = i;
                }
            }

            if (collapseTo && fds[i].fd != -1) {
                /* If a collapse is occurring, copy the current fd into the
                 * collapse target, and advance the target */
                fds[collapseTo++] = fds[i];
            }
        }

        /* The array was being collapsed and all events were processed, so complete
         * collapsing the array */
        while (i < maxIndex && collapseTo != 0) {
            memcpy(buffer[collapseTo], buffer[i], bufferIndex[i]);
            fds[collapseTo++] = fds[i];
            i++;
        }

        if (collapseTo) {
            maxIndex = collapseTo;
        }
    }

    err = 0;

terminate:
    if (sock != -1) {
        close(sock);
    }

    if (platform) {
        stewart_platform_delete(platform);
    }

    if (pca) {
        pca9685_close(pca);
    }

    return err;
}
