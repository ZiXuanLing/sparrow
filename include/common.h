#ifndef __SPARROW_COMMON_H__

#define __SPARROW_COMMON_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct class Class;
typedef struct parser Parser;
typedef struct vm VM;

#define bool char
#define true 1
#define false 0
#define UNUSED __attribute__ ((unused))

#ifdef DEBUG
#define ASSERT(condition, errMsg) \
        do { \
            if (!(condition)) { \
                fprintf(stderr, "ASSERT failed! %s: %d In function %s(): %s\n", __FILE__, __LINE__, __func__, errMsg); \
                abort(); \
            } \
        } while (0)

#else
#define ASSERT(condition, errMsg) ((void) 0)
#endif

#define NOT_REACHED() \
    do { \
        fprintf(stderr, "NOT_REACHED: %s: %d In function %s()\n", __FILE__, __LINE__, __func__); \
        while (1); \
    } while (0);

#endif // !__SPARROW_COMMON_H__