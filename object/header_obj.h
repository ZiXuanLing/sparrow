//
// Created by ZiXuan on 2022/6/10.
//

#ifndef SPARROW_HEADER_OBJ_H
#define SPARROW_HEADER_OBJ_H

#include "../include/utils.h"
#include "../include/common.h"

typedef enum {
    OT_CLASS,
    OT_LIST,
    OT_MAP,
    OT_MODULE,
    OT_RANGE,
    OT_STRING,
    OT_UPVALUE,
    OT_FUNCTION,
    OT_CLOSURE,
    OT_INSTANCE,
    OT_THREAD
} ObjType;  // 对象类型

typedef struct objHeader {
    ObjType type;
    int isDark;
    Class *class;  // 对象所属的类
    struct ObjHeader *next;  // 用于链接所有已分配对象
} ObjHeader; // 对象头，用于记录元信息和垃圾回收

typedef enum {
    VT_UNDEFINED,
    VT_NULL,
    VT_FALSE,
    VT_TRUE,
    VT_NUM,
    VT_OBJ
} ValueType;  // value 类型

typedef struct {
    ValueType type;
    union {
        double num;
        ObjHeader *objHeader;
    };
} Value;

DECLARE_BUFFER_TYPE(Value)

void initObjHeader(VM *vm, ObjHeader *objHeader, ObjType objType, Class *class);

#endif //SPARROW_HEADER_OBJ_H
