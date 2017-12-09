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

int main(int argc, char *argv[]) {
    struct timeval now, start;
    FILE *out = stdout;

    /* Parse command line arguments... */
    if (argc > 1) {
        if (argv[1][0] == '-') {
            fprintf(stderr,
                "usage: record [FILENAME]\n"
                "\n"
                "The above will read from STDIN. Every time a newline is received, it\n"
                "will write what it received to FILENAME prefixed with a timestamp\n"
                "indicating the number of milliseconds from the time the program was\n"
                "launched when that line was received.\n"
                "\n"
                "This can be used in conjunctino with 'playback' to play a buffer back to\n"
                "FILENAME for piping to the 'transform' program.\n"
                "\n"
                "If FILENAME is ommitted, the timestamped value is sent to STDOUT.\n"
                "If FILENAMEis provided, timestamped values are sent to FILENAME, and\n"
                "the original STDIN is echoed back to STDOUT.\n"
                "\n"
                "This allows you to record a live session for playback later.\n");
            return 1;
        } else {
            out = fopen(argv[1], "w");
            if (out == NULL) {
                fprintf(stderr, "ERROR: Unable to create '%s': %s\n", argv[1],
                        strerror(errno));
                return -1;
            }
        }
    }

    gettimeofday(&start, NULL);

    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin) == buf) {
        gettimeofday(&now, NULL);
        fprintf(out, "%ld %s",
               (long)((now.tv_sec - start.tv_sec) * 1000 +
                      (now.tv_usec - start.tv_usec) / 1000), buf);
        fflush(out);
        if (out != stdout) {
            fputs(buf, stdout);
            fflush(stdout);
        }
    }

    if (out != stdout) {
        fclose(out);
    }

    return 0;
}
