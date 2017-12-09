/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#ifndef __stewart_pubsub_h__
#define __stewart_pubsub_h__

#include <stdint.h>
#include <pthread.h>

#define STEWART_PROTOCOL          1

#define STEWART_STATUS_STATIONARY 0
#define STEWART_STATUS_MOVING     1

typedef struct _StewartServo {
    float angle;        /* [Target] angle in radians               */
//    float speed;        /* Radians per second                      */
    float trim;         /* Trim angle in radians                   */
//    int64_t sec;        /* Estimated timestamp of move completion  */
//    int64_t usec;       /* Estimated timestamp of move completion  */
    /* 
     * If speed != 0, then actual position can be estimated via:
     *
     * pos = angle - speed / (sec + usec / 1000000)
     *
     */
} __attribute__((packed)) StewartServo;

static inline float servo_get_current_pos(StewartServo *servo) {
    return servo->angle;
    /*
    if (servo->speed == 0.0) {
        return servo->angle;
    }
    return servo->angle - servo->speed / (servo->sec + servo->usec / 1000000.0);
*/
}

typedef struct _StewartStatus {
    /* Time elapsed (sec + usec) since server launched when msg composed */
    int64_t sec;              
    int64_t usec;             

    StewartServo servos[6]; 

    uint32_t status;          /* STEWART_STATUS_{STATIONARY,MOVING}      
                               * At least ONE servo's speed will be !0 */

    float origin[3];          /* Point around which rotation matrix is 
                               * applied */
    float rotation[9];        /* Rotation matrix currently applied to
                               * Stewart platform. If "MOVING" then 
                               * this is the target and may not match where
                               * the platform is RIGHT NOW */
    float reserved[3];        /* Reserved: Translation of platform after rotation */

    /* To determine "current" position, the status would need to include:
     *
     * -- start origin, rotation, translation, start time
     * -- target origin, rotation, translation, estimated end time
     *
     * Decompose the rotation matrix into quaternions and use SLERP [1].
     * The linear translation operations (origin and translation)
     * can use simpler linear interpolation 
     *
     * 1. https://en.wikipedia.org/wiki/Slerp
     */
} __attribute__((packed)) StewartStatus;

typedef enum {
    STEWART_MESSAGE_INVALID = -1,
    STEWART_MESSAGE_SET_AXISANGLE = 1,
    STEWART_MESSAGE_GET_STATUS = 2,
    STEWART_MESSAGE_STATUS = 3,
    STEWART_MESSAGE_SET_TRIM = 4,
    STEWART_MESSAGE_SET_EUCLIDEAN = 5,
} MessageType;

typedef struct {
    uint32_t version;
    uint32_t size;
    uint32_t type;
    union {
        struct AxisAngle {
            float x;
            float y;
            float z;
            float angle;
            struct {
                float x;
                float y;
                float z;
            } __attribute__ ((packed)) translate;
        } __attribute__((packed)) axisAngle;
        struct Euclidean {
            float yaw;
            float pitch;
            float roll;
            struct {
                float x;
                float y;
                float z;
            } __attribute__ ((packed)) translate;
        } __attribute__((packed)) euclidean;
        struct Trim {
            uint32_t servo;
            float angle;
        } __attribute__((packed)) trim;
        StewartStatus status;
    };
} __attribute__((packed)) StewartMessage;

#endif