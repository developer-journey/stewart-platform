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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "delay.h"

int main(int argc, char *argv[]) {
    struct timeval now, start;
    FILE *in = stdin;

    /* Parse command line arguments... */
    if (argc > 1) {
        if (argv[1][0] == '-') {
            fprintf(stderr,
                "usage: playback [FILENAME]\n"
                "\n"
                "The above will read from FILENAME (or STDIN if no file provided.)\n"
                "\n"
                "Each line of input must contain the number of milliseconds from\n"
                "program launch when the rest of that line should be sent to STDOUT.\n"
                "\n");
            return 1;
        } else {
            in = fopen(argv[1], "r");
            if (in == NULL) {
                fprintf(stderr, "ERROR: Unable to open '%s': %s\n", argv[1],
                        strerror(errno));
                return -1;
            }
        }
    }

    gettimeofday(&start, NULL);

    char buf[1024];
    long pos = 0;
    while (fgets(buf, sizeof(buf), in) == buf) {
        long wait, elapsed;
        char *line = NULL;

        pos++;

        gettimeofday(&now, NULL);

        if (sscanf(buf, "%ld %m[+a-z \n\tA-Z0-9.-]\n", &wait, &line) != 2) {
            fprintf(stderr, "Error parsing line from input.\n");
            break;
        }

        elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                  (now.tv_usec - start.tv_usec) / 1000;

        if (elapsed > wait) {
            fprintf(stderr, "WARNING: Playback can't keep up at line %ld! Behind by %ldms\n",
                    pos, elapsed - wait);
        } else {
            delay((wait - elapsed) * 1000);
        }

        if (line) {
            fputs(line, stdout);
            fflush(stdout);
            free(line);
        }
    }

    if (in != stdin) {
        fclose(in);
    }

    return 0;
}
