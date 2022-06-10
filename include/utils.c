//
// Created by ZiXuan on 2022/6/10.
//
#include "utils.h"
#include "../vm/vm.h"
#include "../parser/parser.h"

#include <stdlib.h>
#include <stdarg.h>

/**
 * 内存管理3种方法
 *      1 申请内存 2 修改空间大小 3 释放内存
 * @param vm
 * @param ptr
 * @param oldSize
 * @param newSize
 * @return
 */
void* memManager(VM *vm, void *ptr, uint32_t oldSize, uint32_t newSize) {
    vm->allocatedBytes += newSize - oldSize;

    if (newSize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, newSize);
}

/**
 * 找出大于等于v最近的2次幂
 * @param v
 * @return
 */
uint32_t ceilToPowerOf2(uint32_t v) {
    v += (v == 0);
    v --;

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    v ++;
    return v;
}

DEFINE_BUFFER_METHOD(String)

DEFINE_BUFFER_METHOD(Int)

DEFINE_BUFFER_METHOD(Char)

DEFINE_BUFFER_METHOD(Byte)

void errorReport(void *parser, ErrorType errorType, const char *fmt, ...) {
    char buffer[DEFAULT_BUFFER_SIZE] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, DEFAULT_BUFFER_SIZE, fmt, ap);
    va_end(ap);

    switch (errorType) {
        case ERROR_IO:
        case ERROR_MEM:
            fprintf(stderr, "%s:%d In function %s():%s\n",
                    __FILE__, __LINE__, __func__ , buffer);
            break;
        case ERROR_LEX:
        case ERROR_COMPILE:
            ASSERT(parser != NULL, "parser is null!");
            fprintf(stderr, "%s:%d \"%s\"\n", ((Parser *)parser)->file,
                    ((Parser *)parser)->preToken.lineNo, buffer);
            break;
        case ERROR_RUNTIME:
            fprintf(stderr, "%s\n", buffer);
            break;
        default:
            NOT_REACHED();
    }
    exit(1);
}

void symbolTableClear(VM *vm, SymbolTable *buffer) {
    uint32_t idx = 0;
    while (idx < buffer->count) {
        memManager(vm, buffer->datas[idx ++].str, 0 ,0);
    }
    StringBufferClear(vm, buffer);
}