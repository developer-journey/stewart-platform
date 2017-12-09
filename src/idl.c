/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <stdlib.h>
#include <stdio.h>

#include "stewart.h"
#include "stewart-pubsub.h"

int printServo(int i) {
    return i;
};

int printFloat(int i) {
    fprintf(stdout, "buffer.readFloatLE(%d)", i);
    return i + 4;
}

int printInt32(int i) {
    fprintf(stdout, "buffer.readInt32LE(%d)", i);
    return i += 4;
}

int printInt64(int i) {
    i = printFloat(i);
    fprintf(stdout, " << 32 | ");
    return printFloat(i);
}

typedef enum {
    TYPE_INT32,
    TYPE_INT64,
    TYPE_FLOAT
} Type;

int printType(int i, const char *name, Type type, int count) {
    int array = count != 0;
    
    if (count == 0) {
        fprintf(stdout, "        %s: ", name);
    } else {
        fprintf(stdout, "        %s: [\n", name);
    }

    do {
        if (array) {
            fprintf(stdout, "            ");
        }
        
        switch (type) {
        case TYPE_INT32:
            i = printInt32(i);
            break;
        case TYPE_INT64: 
            i = printInt64(i);
            break;
        case TYPE_FLOAT:
            i = printFloat(i);
            break;
        }
        if (count) {
            fprintf(stdout, ",\n");
        }
    } while (count && --count);
    if (array) {
        fprintf(stdout, "]");
    }
    fprintf(stdout, ",\n");
    return i;
}

int main(int argc, const char *argv[]) {
    StewartMessage msg;

    int ofs = 0, i;
    
    fprintf(stdout, "function readStatus(buffer) {\n");
    fprintf(stdout, "    if (!(buffer instanceof Buffer)) {\n");
    fprintf(stdout, "        throw Error('Invalid type. Buffer expected.');\n");
    fprintf(stdout, "    }\n");
    fprintf(stdout, "    if (buffer.length != %ld) {\n", sizeof(msg));
    fprintf(stdout, "        throw Error('Invalid buffer size: ' + buffer.length + ' vs. %ld');\n", sizeof(msg));
    fprintf(stdout, "    }\n");
    fprintf(stdout, "    var header = {\n");
    ofs = printType(ofs, "version", TYPE_INT32, 0);
    ofs = printType(ofs, "size", TYPE_INT32, 0);
    ofs = printType(ofs, "type", TYPE_INT32, 0);
    fprintf(stdout, "    };\n");
    fprintf(stdout, "    if (header.version != %d) {\n", STEWART_PROTOCOL);
    fprintf(stdout, "        throw Error('Invalid Stewart protocol version: ' + header.version + ' vs. %d');\n", STEWART_PROTOCOL);
    fprintf(stdout, "    }\n");
    fprintf(stdout, "    if (header.size != %ld) {\n", sizeof(msg));
    fprintf(stdout, "        throw Error('Invalid declared size: ' + header.size + ' vs. %ld');\n", sizeof(msg));
    fprintf(stdout, "    }\n");
    fprintf(stdout, "    if (header.type != %d) {\n", STEWART_MESSAGE_STATUS);
    fprintf(stdout, "        throw Error('Invalid message type: ' + header.type + ' vs. %d');\n", STEWART_MESSAGE_STATUS);
    fprintf(stdout, "    }\n");
    fprintf(stdout, "    return {\n");
    ofs = printType(ofs, "sec", TYPE_INT64, 0);
    ofs = printType(ofs, "usec", TYPE_INT64, 0);
    fprintf(stdout, "        servos: [{\n");

    for (i = 0; i < 6; i++) {
        ofs = printType(ofs, "        angle", TYPE_FLOAT, 0);
        ofs = printType(ofs, "        trim", TYPE_FLOAT, 0);
        if (i != 5) {
            fprintf(stdout, "        }, {\n");
        } else {
            fprintf(stdout, "        }],\n");
        }
    }
                        
    ofs = printType(ofs, "status", TYPE_INT32, 0);
    ofs = printType(ofs, "origin", TYPE_FLOAT, 3);
    ofs = printType(ofs, "rotation", TYPE_FLOAT, 9);
    ofs = printType(ofs, "reserved", TYPE_FLOAT, 3);
    fprintf(stdout, "    };\n");
    fprintf(stdout, "}\n");
    return 0;
}
