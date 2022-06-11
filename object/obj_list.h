//
// Created by ZiXuan on 2022/6/11.
//

#ifndef SPARROW_OBJ_LIST_H
#define SPARROW_OBJ_LIST_H

/**
 * 实现list列表对象
 */

#include "class.h"
#include "../vm/vm.h"

typedef struct {
    ObjHeader objHeader;
    ValueBuffer elements; // lists中的元素
} ObjList;

ObjList* newObjList(VM *vm, uint32_t elementNum);
Value removeElement(VM *vm, ObjList *objList, uint32_t index);
void insertElement(VM *vm, ObjList *objList, uint32_t index, Value value);

#endif //SPARROW_OBJ_LIST_H
