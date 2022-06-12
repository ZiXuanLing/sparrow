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
int valueIsEqual(Value a, Value b) {
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


/**
 * 新建一个裸类
 * @param vm
 * @param name
 * @param fieldNum
 * @return
 */
Class* newRawClass(VM *vm, const char *name, uint32_t fieldNum) {
    Class *class = ALLOCATE(vm, Class);

    // 裸类没有元类
    initObjHeader(vm, &class->objHeader, OT_CLASS, NULL);
    class->name = newObjString(vm, name, strlen(name));
    class->fieldNum = fieldNum;
    class->superClass = NULL; // 默认没有基类

    MethodBufferInit(&class->methods);
    return class;
}

/**
 * 数字等value也被视为对象，因此参数为value，获得对象obj所属的类
 * @param vm
 * @param object
 * @return
 */
inline Class *getClassOfObj(VM *vm, Value object) {
    switch (object.type) {
        case VT_NULL:
            return vm->nullClass;
        case VT_FALSE:
        case VT_TRUE:
            return vm->boolClass;
        case VT_NUM:
            return vm->numClass;
        case VT_OBJ:
            return VALUE_TO_OBJ(object)->class;
        default:
            NOT_REACHED();
    }
    return NULL;
}