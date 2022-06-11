//
// Created by ZiXuan on 2022/6/11.
//
#include "obj_map.h"
#include "class.h"
#include "../vm/vm.h"
#include "obj_string.h"
#include "obj_range.h"

/**
 * 创建新map对象
 * @param vm
 * @return
 */
ObjMap* newObjMap(VM *vm) {
    ObjMap *objMap = ALLOCATE(vm, ObjMap);
    initObjHeader(vm, &objMap->objHeader, OT_MAP, vm->mapClass);
    objMap->capacity = objMap->count = 0;
    objMap->entries = NULL;
    return objMap;
}

/**
 * 计算数字的哈希码
 * @param num
 * @return
 */
static uint32_t hashNum(double num) {
    Bits64 bits64;
    bits64.num = num;
    return bits64.bits32[0] ^ bits64.bits32[1];
}

/**
 * 计算对象的哈希码
 * @param objHeader
 * @return
 */
static uint32_t hashObj(ObjHeader *objHeader) {
    switch (objHeader->type) {
        case OT_CLASS:
            return hashString(((Class *)objHeader)->name->value.start,
                              ((Class *)objHeader)->name->value.length);
//            break;
        case OT_RANGE: {
            ObjRange *objRange = (ObjRange *) objHeader;
            return hashNum(objRange->from) ^ hashNum(objRange->to);
//            break;
        }
        case OT_STRING:
            return ((ObjString *)objHeader)->hashCode;
        default:
            RUN_ERROR("the hashable are objstring, objrange and class.");
    }
    return 0;
}

void mapSet(VM *vm, ObjMap *objMap, Value key, Value value) {

}

Value mapGet(ObjMap *objMap, Value key) {

}

void clearMap(VM *vm, ObjMap *objMap) {

}

Value removeKey(VM *vm, ObjMap *objMap, Value key) {

}