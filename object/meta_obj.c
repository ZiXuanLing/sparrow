//
// Created by ZiXuan on 2022/6/10.
//
#include "meta_obj.h"
#include "obj_fn.h"
#include "class.h"
#include "../vm/vm.h"
#include <string.h>

/**
 * 新建模块
 * @param vm
 * @param modName
 * @return
 */
ObjModule * newObjModule(VM *vm, const char *modName) {
    ObjModule  *objModule = ALLOCATE(vm, ObjModule);
    if (objModule == NULL) {
        MEM_ERROR("allocate objModule failed!");
    }

    // objModule是元信息对象，不属于任何一个类
    initObjHeader(vm, &objModule->objHeader, OT_MODULE, NULL);

    StringBufferInit(&objModule->moduleVarName);
    ValueBufferInit(&objModule->moduelVarValue);

    objModule->name = NULL;
    if (modName != NULL) {
        objModule->name = newObjString(vm, modName, strlen(modName));
    }

    return objModule;
}

/**
 * 创建类的实例
 * @param vm
 * @param class
 * @return
 */
ObjInstance* newObjInstance(VM *vm, Class *class) {
    // 参数class主要作用是提供类中field的数目
    ObjInstance *objInstance = ALLOCATE_EXTRA(vm, ObjInstance, sizeof(Value) * class->fieldNum);

    // 在此关联对象的类为参数class
    initObjHeader(vm, &objInstance->objHeader, OT_INSTANCE, class);

    // 初始化field为NULL
    uint32_t idx = 0;
    while (idx < class->fieldNum) {
        objInstance->fields[idx ++] = VT_TO_VALUE(VT_NULL);
    }
    return objInstance;
}