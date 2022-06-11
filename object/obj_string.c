#include "obj_string.h"
#include "header_obj.h"
#include <string.h>
#include "../vm/vm.h"
#include "../include/utils.h"
#include "../include/common.h"
#include <stdlib.h>

/**
 * fnv-la算法
 * @param str
 * @param length
 * @return
 */
uint32_t hashString(char *str, uint32_t length) {
    uint32_t hashCode = 2166136261, idx = 0;
    while (idx < length) {
        hashCode ^= str[idx];
        hashCode *= 1677619;
        idx ++;
    }
    return hashCode;
}

/**
 * 为string计算哈希码并将值存储到string->hash
 * @param objString
 */
void hashObjString(ObjString *objString) {
    objString->hashCode = hashString(objString->value.start, objString->value.length);
}

/**
 * 以str字符串创建objString对象，允许空串""
 * @param vm
 * @param str
 * @param length
 * @return
 */
ObjString* newObjString(VM *vm, const char *str, uint32_t length) {
    ASSERT(length == 0 || str != NULL, "str length don't match str!");

    ObjString *objString = ALLOCATE_EXTRA(vm, ObjString, length + 1);

    if (objString != NULL) {
        initObjHeader(vm, &objString->objHeader, OT_STRING, vm->stringClass);
        objString->value.length = length;

        if (length > 0) {
            memcpy(objString->value.start, str, length);
        }
        objString->value.start[length] = '\0';
        hashObjString(objString);
    }
    else {
        MEM_ERROR("Allocating objString failed!");
    }
    return objString;
}