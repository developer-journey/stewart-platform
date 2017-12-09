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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/limits.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "i2c.h"

struct _I2CDev {
    int fd;
    int addr;
};

int i2c_read8(I2CDev *dev, unsigned char reg, unsigned char *buf) {
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data data;

    /* First message is an empty write to the register (no payload) */
    messages[0].addr = dev->addr;
    messages[0].len = sizeof(reg);
    messages[0].buf = &reg;
    messages[0].flags = 0;

    /* Second message will return the value */
    messages[1].addr = dev->addr;
    messages[1].len = sizeof(*buf);
    messages[1].buf = buf;
    messages[1].flags = I2C_M_RD;

    data.msgs = messages;
    data.nmsgs = 2;

    if (dev->fd >= 0 && ioctl(dev->fd, I2C_RDWR, &data) < 0) {
        return -1;
    }

    return 0;
}

int i2c_read(I2CDev *dev, unsigned char reg, unsigned char* buf, size_t len) {
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data data;

    /* First message is an empty write to the register (no payload) */
    messages[0].addr = dev->addr;
    messages[0].len = sizeof(reg);
    messages[0].buf = &reg;
    messages[0].flags = 0;

    /* Second message will return the value */
    messages[1].addr = dev->addr;
    messages[1].len = len;
    messages[1].buf = buf;
    messages[1].flags = I2C_M_RD;

    data.msgs = messages;
    data.nmsgs = 2;

    if (dev->fd >= 0 && ioctl(dev->fd, I2C_RDWR, &data) < 0) {
        return -errno;
    }

    return 0;
}

int i2c_write8(I2CDev *dev, unsigned char reg, unsigned char value) {
    unsigned char buf[2];
    struct i2c_msg message;
    struct i2c_rdwr_ioctl_data data;

    buf[0] = reg;    /* register to write to */
    buf[1] = value;  /* value to write */

    message.addr = dev->addr;
    message.len = sizeof(buf);
    message.buf = buf;
    message.flags = 0;

    data.msgs = &message;
    data.nmsgs = 1;

    if (ioctl(dev->fd, I2C_RDWR, &data) < 0) {
        return -errno;
    }

    return 0;
}

I2CDev *i2c_open(int device, int addr) {
    I2CDev *dev;
    char filename[PATH_MAX];

    dev = (I2CDev *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));

    snprintf(filename, sizeof(filename), "/dev/i2c-%d", device);
    dev->fd = open(filename, O_RDWR);
    if (dev->fd < 0) {
        free(dev);
        return NULL;
    }

    dev->addr = addr;

    if (ioctl(dev->fd, I2C_SLAVE, dev->addr) < 0) {
        fprintf(stderr,
                "Error opening address 0x%02x:\n"
                "%d: %s\n", dev->addr, errno,
                strerror(errno));
        i2c_close(dev);
        return NULL;
    }

    return dev;
}

void i2c_close(I2CDev *dev) {
    if (!dev) {
        return;
    }
    if (dev->fd > 0) {
        close(dev->fd);
    }
    free(dev);
}
