/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __pca9685_h__
#define __pca9685_h__

typedef struct _PCA9685 PCA9685;

#define PCA9685_ALL_CHANNELS -1

PCA9685 *pca9685_open(int bus, int addr);
void pca9685_close(PCA9685 *pca);
int pca9685_set_channel_pulse(PCA9685 *pca, int channel,
                              unsigned int on, unsigned int off);
int pca9685_set_pulse_frequency(PCA9685 *pca, int frequency);
int pca9685_get_frequency(PCA9685 *pca);
float pca9685_get_effective_frequency(PCA9685 *pca);

#endif