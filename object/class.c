//
// Created by ZiXuan on 2022/6/11.
//
#include "class.h"
#include "../include/common.h"
#include "obj_range.h"
#include "string.h"
#include "../vm/core.h"
#include "../vm/vm.h"

DEFINE_BUFFER_METHOD(Method)

/**
 * 判断a和b是否相等
 * @param a
 * @param b
 * @return
 */
int ValueIsEqual(Value a, Value b) {
    // 类型不同则无需进行后面的比较

    if (a.type != b.type) {
        return false;
    }
    if (a.type == VT_NUM) {
        return a.num == b.num;
    }

    if (a.objHeader == b.objHeader) {
        return true;
    }

    if (a.objHeader->type != b.objHeader->type) {
        return false;
    }

    if (a.objHeader->type == OT_STRING) {
        ObjString *strA = VALUE_TO_OBJSTR(a);
        ObjString *strB = VALUE_TO_OBJSTR(b);
        return (strA->value.length == strB->value.length &&
        memcmp(strA->value.start, strB->value.start, strA->value.length) == 0);
    }

    if (a.objHeader->type == OT_RANGE) {
        ObjRange *rgA = VALUE_TO_OBJRANGE(a);
        ObjRange *rgB = VALUE_TO_OBJRANGE(b);
        return (rgA->from == rgB->from && rgA->to == rgB->to);
    }
    return false;
}