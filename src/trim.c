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
#include <sys/types.h>

#include "pca9685.h"

#include "config.h"

void usage(int ret) {
    fprintf(stderr,
            "usage: servo [-u UNIT] channel [trim-in-units "
            "[pulse-width(us) [pulse-frequency(cycle-per-second)]]]\n\n");

    fprintf(stderr, "-u UNIT    Units for TRIM to be specified.\n"
                    "           Options: rad, deg, us. Default us\n\n");
    fprintf(stderr, "Compiled default values (see config.h):\n");
    fprintf(stderr, "   pulse-width defaults to %dus\n", PULSE_WIDTH_ZERO_POS);
    fprintf(stderr, "   pulse-frequency defaults to %dHz\n\n",
            PULSE_WIDTH_FREQUENCY);
    exit(ret);
}

void version() {
    fprintf(stdout, 
            "servo: Stewart platform servo trim utility\n"
            "Copyright (C) 2017 Intel Corporation\n"
            "Licensed under the terms of the Apache 2.0 license. See LICENSE file.\n"
            "\n"
            "Version: " VERSION "\n");
    exit(0);
}

void showTrim(StewartConfig *config, int channel) {
    fprintf(stderr, "   Servo %3d trim: %+3dus (%+6.2f degrees)\n",
            channel, (int)RADIANS_TO_PULSE_WIDTH(DEG2RAD(config->servo_trim[channel])) -
            PULSE_WIDTH_ZERO_POS,
            config->servo_trim[channel]);
}

int main(int argc, char *argv[]) {
    StewartConfig _config;
    StewartConfig *c = &_config;
    PCA9685 *pca = NULL;
    int err = 0;
    int channel = -1, frequency = -1, pulse_width = -1, trim = -1;
    float trimValue = -1;
    int i;
    typedef enum {
        US = 1,
        DEG = 2,
        RAD = 3
    } Unit;
    Unit units = US;
    char *unitWord = "us";
    
    config_get(c);

    fprintf(stdout, "Servo trim configuration utility.\n"
            "Copyright (C) 2015-2017 Intel Corporation\n\n");

    /* Parse command line arguments... */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && (argv[i][1] < '0' || argv[i][1] > '9')) {
            switch (argv[i][1]) {
                case '?': usage(0);
                    break;

                case 'u':
                    i++;
                    if (i == argc) {
                        usage(-1);
                    }
                    if (!strcmp("us", argv[i])) {
                        units = US;
                    } else if (!strcmp("deg", argv[i])) {
                        units = DEG;
                    } else if (!strcmp("rad", argv[i])) {
                        units = RAD;
                    } else {
                        fprintf(stderr, "Invalid unit: %s\n\n", argv[i]);
                        usage(-1);
                    }
                    unitWord = argv[i];
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

        if (channel == -1) {
            channel = strtol(argv[i], NULL, 0);
            if (channel < 0 || channel > 5) {
                fprintf(stderr, "Invalid channel requested: %d\n"
                        "Valid range: 0 to 5\n\n",
                        channel);
                usage(-1);
            }
            continue;
        }

        if (trimValue == -1) {
            trimValue = strtof(argv[i], NULL);
            continue;
        }

        if (pulse_width == -1) {
            pulse_width = strtol(argv[i], NULL, 0);
            if (pulse_width <= 0) {
                fprintf(stderr, "Pulse width must be higher than 0us\n\n");
                usage(-1);
            }
            continue;
        }
        
        if (frequency == -1) {
            frequency = strtol(argv[i], NULL, 0);
            if (frequency <= 0) {
                fprintf(stderr, "Frequency must be higher than 0Hz!\n\n");
                usage(-1);
            }
            continue;
        }

        usage(-1);
    }

    /* Only set the actual "trim" field if the trimValue was specified on the
     * command line */
    if (trimValue != -1) {
        switch (units) {
            case US:
                trim = trimValue;
                break;

            case RAD:
                trim  = RADIANS_TO_PULSE_WIDTH(trimValue) - PULSE_WIDTH_ZERO_POS;
                break;

            case DEG:
                trim = RADIANS_TO_PULSE_WIDTH(DEG2RAD(trimValue)) - PULSE_WIDTH_ZERO_POS;
                break;
        }
    }

    /* If no channel was specified, show all channel trim values */
    if (channel == -1) {
        for (i = 0; i < 6; i++) {
            showTrim(c, i);
        }
        return 0;
    }
    
    /* If no trim value was specified, show the requested channel trim value */
    if (trim == -1) {
        showTrim(c, channel);        
        return 0;
    }

    fprintf(stdout, "Current trim value for channel %d:\n", channel);
    showTrim(c, channel);
    fprintf(stdout, "\n");

    /* Verify supplied values */
    if ((trim + PULSE_WIDTH_ZERO_POS < PULSE_WIDTH_MIN_POS) ||
        (trim + PULSE_WIDTH_ZERO_POS > PULSE_WIDTH_MAX_POS)) {
        fprintf(stderr, "Requested trim outside of compiled PULSE limits: %dus (%+5.02f%s)\n\n"
                "Valid range: %dus to %dus. See config.h\n\n",
                trim, trimValue, unitWord, PULSE_WIDTH_MIN_POS - PULSE_WIDTH_ZERO_POS,
                PULSE_WIDTH_MAX_POS - PULSE_WIDTH_ZERO_POS);
        return -1;
    }
    
    fprintf(stdout, "Requested trim: %.02f%s = %dus\n\n", trimValue, unitWord, trim);
    
    fprintf(stdout, "Channel               : %d\n", channel);
    fprintf(stdout, "Trim                  : %dus (%+.02f deg, %+.02f rad)\n", 
            trim,
            RAD2DEG(PULSE_WIDTH_TO_RADIANS(trim + PULSE_WIDTH_ZERO_POS)),
            PULSE_WIDTH_TO_RADIANS(trim + PULSE_WIDTH_ZERO_POS));

    if (frequency == -1) {
        frequency = PULSE_WIDTH_FREQUENCY;
    }
    fprintf(stdout, "Pulse frequency       : %dHz\n", frequency);

    if (pulse_width == -1) {
        pulse_width = PULSE_WIDTH_ZERO_POS;
    }

    if (1000000 / frequency < pulse_width + trim) {
        fprintf(stderr, "Calculated pulse width (%dus) is longer than "
                "frequency allows: %dms @ %dHz\n",
                pulse_width, 1000000 / frequency, frequency);
        return -1;
    }
    fprintf(stdout, "Pulse width limits    : [%dus, %dus]\n",
            PULSE_WIDTH_MIN_POS, PULSE_WIDTH_MAX_POS);
    fprintf(stdout, "Base pulse width      : %dus\n", pulse_width);
    fprintf(stdout, "Pulse width with trim : %dus (%+.02f degrees)\n",
            pulse_width + trim,
            RAD2DEG(PULSE_WIDTH_TO_RADIANS(pulse_width + trim))),

    /* Attempt to connect to PCA9685 in order to program the servo locations */
    pca = pca9685_open(1, 0x40);
    if (!pca) {
        fprintf(stderr, "Warning: Could not initialize PCA9685!\n");
    }

    if (pca) {
        err = pca9685_set_pulse_frequency(pca, frequency);
        if (err) {
            fprintf(stderr, "Warning: Could not set PWM frequency!\n");
        }

        fprintf(stdout, "Effective frequency: %.02fHz\n",
                pca9685_get_effective_frequency(pca));

        pca9685_set_channel_pulse(pca, channel, 0, pulse_width + trim);
    }
    
    c->servo_trim[channel] = RAD2DEG(PULSE_WIDTH_TO_RADIANS(pulse_width + trim));
    
    if (config_write(c) == 0) {
        fprintf(stdout, "\nstewart.cfg updated\n\n");
    }

    if (pca) {
        pca9685_close(pca);
    }

    return err;
}
