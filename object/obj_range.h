//
// Created by ZiXuan on 2022/6/11.
//

#ifndef SPARROW_OBJ_RANGE_H
#define SPARROW_OBJ_RANGE_H

#include "class.h"

typedef struct {
    ObjHeader objHeader;
    int from;
    int to;
} ObjRange;  // range对象

ObjRange* newObjRange(VM *vm, int from, int to);

#endif //SPARROW_OBJ_RANGE_H
