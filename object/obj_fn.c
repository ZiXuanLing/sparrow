//
// Created by ZiXuan on 2022/6/11.
//
#include "meta_obj.h"
#include "class.h"
#include "vm.h"

/**
 * 创建upvalue对象
 * @param vm
 * @param localVarPtr
 * @return
 */
ObjUpvalue* newObjUpvalue(VM *vm, Value *localVarPtr) {
    ObjUpvalue *objUpvalue = ALLOCATE(vm, ObjUpvalue);
    initObjHeader(vm, &objUpvalue->objHeader, OT_UPVALUE, NULL);
    objUpvalue->localVarPtr = localVarPtr;
    objUpvalue->closedUpvalue = VT_TO_VALUE(VT_NULL);
    objUpvalue->next = NULL;
    return objUpvalue;
}

/**
 * 以函数fn创建一个闭包
 * @param vm
 * @param objFn
 * @return
 */
ObjClosure* newObjClosure(VM *vm, ObjFn *objFn) {
    ObjClosure *objClosure = ALLOCATE_EXTRA(vm,
                                              ObjClosure,
                                              sizeof(ObjUpvalue *) * objFn->upvalueNum);
    initObjHeader(vm, &objClosure->objHeader, OT_CLOSURE, vm->fnClass);
    objClosure->fn = objFn;

    // 清除upvalue数组，以避免在填充upvalue数组之前出发GC
    uint32_t idx = 0;
    while (idx < objFn->upvalueNum) {
        objClosure->upvalues[idx] = NULL;
        idx ++;
    }

    return objClosure;

}

/**
 * 创建一个空函数
 * @param vm
 * @param objModule
 * @param maxStackSlotUsedNum
 * @return
 */
ObjFn* newObjFn(VM *vm, ObjModule *objModule, uint32_t maxStackSlotUsedNum) {
    ObjFn *objFn = ALLOCATE(vm, ObjFn);
    if (objFn == NULL) {
        MEM_ERROR("allocate ObjFn failed!");
    }
    initObjHeader(vm, &objFn->objHeader, OT_FUNCTION, vm->fnClass);
    ByteBufferInit(&objFn->instrStream);
    ValueBufferInit(&objFn->constants);
    objFn->module = objModule;
    objFn->maxStackSlotUsedNum = maxStackSlotUsedNum;
    objFn->upvalueNum = objFn->argNum = 0;
#ifdef DEBUG
    objFn->debug = ALLOCATE(vm, FnDebug);
    objFn->debug.fnName = NULL;
    IntBufferInit(&objFn->debug.lineNo);
#endif
    return objFn;

}