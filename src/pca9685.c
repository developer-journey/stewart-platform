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

#include <linux/i2c-dev.h>
#include <linux/limits.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "i2c.h"
#include "pca9685.h"
#include "delay.h"

typedef struct {
    int on;        /* Last requested PWM ON */
    int off;       /* Last requested PWM OFF */
} PCA9685Channel;

struct _PCA9685 {
    I2CDev *dev;
    PCA9685Channel channels[16];
    int frequency;
    int pulse_width;
};

/*
 * PCA9685 registers from:
 *
 * http://www.nxp.com/documents/data_sheet/PCA9685.pdf
 *
 */
enum {
    PCA9685_MODE1       = 0x00,
    PCA9685_MODE2       = 0x01,
    PCA9685_SUBADR1     = 0x02,
    PCA9685_SUBADR2     = 0x03,
    PCA9685_SUBADR3     = 0x04,
    PCA9685_ALLCALLADR  = 0x05,

    PCA9685_CHANNEL_SIZE      = 4,
    PCA9685_CHANNEL_0_OFFSET  = 0x06,
    PCA9685_ALL_OFFSET        = 0xFA,
    PCA9685_ON_L              = 0,
    PCA9685_ON_H,
    PCA9685_OFF_L,
    PCA9685_OFF_H,

    PCA9685_PRE_SCALE   = 0xFE,

    PCA9685_MODE1_RESTART = 1 << 7,
    PCA9685_MODE1_SLEEP   = 1 << 4,
    PCA9685_MODE1_ALLCALL = 1 << 0,

    PCA9685_MODE2_INVRT   = 1 << 4,
    PCA9685_MODE2_OUTDRV  = 1 << 2,

    PCA9685_INTERNAL_OSCILLATOR = 25000000 /* 25MHz */
};

PCA9685 *pca9685_open(int bus, int addr) {
    int i;
    PCA9685 *pca = malloc(sizeof(*pca));

    memset(pca, 0, sizeof(*pca));
    pca->dev = i2c_open(bus, addr);
    if (!pca->dev) {
        pca9685_close(pca);
        return NULL;
    }

    int err;
    err = pca9685_set_channel_pulse(pca, PCA9685_ALL_CHANNELS, 0, 0);
    if (err) {
        fprintf(stderr, "Could not reset all PWM fields!\n");
        pca9685_close(pca);
        return NULL;
    }

    err = i2c_write8(pca->dev, PCA9685_MODE2, PCA9685_MODE2_OUTDRV);
    if (err) {
        pca9685_close(pca);
        return NULL;
    }

    err = i2c_write8(pca->dev, PCA9685_MODE1, PCA9685_MODE1_ALLCALL);
    if (err) {
        pca9685_close(pca);
        return NULL;
    }

    /* Spec says maximum of 500us for oscillator to be up once SLEEP
     * is set to 0 */
    delay(500);

    unsigned char mode = 0;
    err = i2c_read8(pca->dev, PCA9685_MODE1, &mode);
    if (err) {
        pca9685_close(pca);
        return NULL;
    }

    mode &= ~PCA9685_MODE1_SLEEP; /* Clear SLEEP bit */
    err = i2c_write8(pca->dev, PCA9685_MODE1, mode);
    if (err) {
        pca9685_close(pca);
        return NULL;
    }

    /* Wait 500us again after clearing SLEEP for clock to get back */
    delay(500);

    for (i = 0; i < sizeof(pca->channels) / sizeof(pca->channels[0]); i++) {
        pca->channels[i].on =
        pca->channels[i].off = -1;
    }

    return pca;
}

void pca9685_close(PCA9685 *pca) {
    if (!pca) {
        return;
    }
    i2c_close(pca->dev);
    free(pca);
}

int pca9685_set_pulse_frequency(PCA9685 *pca, int frequency) {
    if (pca == NULL) {
        fprintf(stderr, "NULL passed to pca9685_set_pulse_frequency!\n");
        return -1;
    }
    if (frequency == 0) {
        fprintf(stderr, "Attempt to set frequency to 0Hz!\n");
        return -2;
    }

    /* Determine PRE_SCALE value based on 7.3.5 PWM frequency PRE_SCALE
     * from http://www.nxp.com/documents/data_sheet/PCA9685.BASE_DISTANCEf */
    int err;
    int prescale;
    unsigned char mode = 0, tmp;
    prescale = round((float)PCA9685_INTERNAL_OSCILLATOR / (4096.0 * frequency)) - 1.0;

    /* The PRE_SCALE can only be set if SLEEP is set to 1 */
    err = i2c_read8(pca->dev, PCA9685_MODE1, &mode);
    if (err) {
        fprintf(stderr, "Could not read mode from PCA9685!\n");
        return err;
    }
    tmp = mode;
    mode &= ~PCA9685_MODE1_RESTART; /* Clear the RESTART bit if set */
    mode |= PCA9685_MODE1_SLEEP;    /* Set the SLEEP bit */
    err = i2c_write8(pca->dev, PCA9685_MODE1, mode); /* Initiate SLEEP */
    if (err) {
        fprintf(stderr, "Could not write SLEEP mode to PCA9685!\n");
        return err;
    }

    /* Set the new PRE_SCALE */
    err = i2c_write8(pca->dev, PCA9685_PRE_SCALE, prescale);
    if (err) {
        fprintf(stderr, "Could not write PRE_SCALE to PCA9685!\n");
        return err;
    }

    /* Set the mode back to what it was */
    err = i2c_write8(pca->dev, PCA9685_MODE1, tmp);
    if (err) {
        fprintf(stderr, "Could not set mode back on PCA9685!\n");
        return err;
    }

    /* After 500us, raise the RESTART bit to resume PWM values */
    delay(500);
    err = i2c_write8(pca->dev, PCA9685_MODE1, tmp | PCA9685_MODE1_RESTART);
    if (err) {
        fprintf(stderr, "Could not raise RESTART on PCA9685!\n");
        return err;
    }

    pca->frequency = frequency;
    pca->pulse_width = 1000000L / pca->frequency;

    return 0;
}

int pca9685_set_channel_pulse(PCA9685 *pca, int channel,
                              unsigned int on, unsigned int off) {
    int channel_offset;
    int i;
    int err;

    /* frequency isn't set during initialization, which is when the pulse is being
     * set to 0 */
    if (pca->frequency > 0) {
        on = 4096 * on / (1000000L / pca->frequency);
        off = 4096 * off / (1000000L / pca->frequency);
    } else {
        on = off = 0;
    }

    if (channel == PCA9685_ALL_CHANNELS) {
        for (i = 0; i < sizeof(pca->channels) / sizeof(pca->channels[0]); i++) {
            pca->channels[i].on = -1;
            pca->channels[i].off = -1;
        }
        channel_offset = PCA9685_ALL_OFFSET;
    } else {
        if (pca->channels[channel].on == on &&
            pca->channels[channel].off == off) {
            return 0;
        }
        channel_offset = PCA9685_CHANNEL_0_OFFSET + channel * PCA9685_CHANNEL_SIZE;
    }

    err = i2c_write8(pca->dev, channel_offset + PCA9685_ON_L, on & 0x0ff);
    if (err) {
        fprintf(stderr, "Unable to set channel ON LOW to 0x%02x\n", on & 0xff);
        return err;
    }
    err = i2c_write8(pca->dev, channel_offset + PCA9685_ON_H, on >> 8);
    if (err) {
        fprintf(stderr, "Unable to set channel ON HI to 0x%02x\n", on >> 8);
        return err;
    }

    err = i2c_write8(pca->dev, channel_offset + PCA9685_OFF_L, off & 0x0ff);
    if (err) {
        fprintf(stderr, "Unable to set channel OFF LOW to 0x%02x\n", off & 0xff);
        return err;
    }
    err = i2c_write8(pca->dev, channel_offset + PCA9685_OFF_H, off >> 8);
    if (err) {
        fprintf(stderr, "Unable to set channel OFF HI to 0x%02x\n", off >> 8);
        return err;
    }

    if (channel == PCA9685_ALL_CHANNELS) {
        for (i = 0; i < sizeof(pca->channels) / sizeof(pca->channels[0]); i++) {
            pca->channels[i].on = on;
            pca->channels[i].off = off;
        }
    } else {
        pca->channels[channel].on = on;
        pca->channels[channel].off = off;
    }

    return 0;
}

int pca9685_get_frequency(PCA9685 *pca) {
    return pca->frequency;
}

float pca9685_get_effective_frequency(PCA9685 *pca) {
    int prescale = round((float)PCA9685_INTERNAL_OSCILLATOR /
                         (4096.0 * pca->frequency)) - 1.0;
    return (float)PCA9685_INTERNAL_OSCILLATOR / (prescale * 4096);
}
