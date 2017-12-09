/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __i2c_h__
#define __i2c_h__

typedef struct _I2CDev I2CDev;

int i2c_read8(I2CDev *, unsigned char, unsigned char *);
int i2c_read(I2CDev *, unsigned char, unsigned char*, size_t);
int i2c_write8(I2CDev *, unsigned char, unsigned char);
I2CDev *i2c_open(int, int);
void i2c_close(I2CDev *);

#endif