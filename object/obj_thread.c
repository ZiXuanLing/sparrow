//
// Created by ZiXuan on 2022/6/12.
//
#include "obj_thread.h"
#include "../vm/vm.h"
#include "class.h"

/**
 * 为运行准备栈帧
 * @param objThread
 * @param objClosure
 * @param stackStart
 */
void prepareFrame(ObjThread *objThread, ObjClosure *objClosure, Value *stackStart) {
    ASSERT(objThread->frameCapacity > objThread->usedFrameNum, "frame not enough!");
    Frame *frame = &(objThread->frames[objThread->usedFrameNum + 1]);

    frame->stackStart = stackStart;
    frame->closure = objClosure;
    frame->ip = objClosure->fn->instrStream.datas;
}

/**
 * 新建线程
 * @param vm
 * @param objClosure
 * @return
 */
ObjThread* newObjThread(VM *vm, ObjClosure *objClosure) {
    ASSERT(objClosure != NULL, "objClosure is NULL!");

    Frame *frames = ALLOCATE_ARRAY(vm, Frame, INITIAL_FRAME_NUM);

    uint32_t stackCapacity = ceilToPowerOf2(objClosure->fn->maxStackSlotUsedNum + 1);

    Value *newStack = ALLOCATE_ARRAY(vm, Value, stackCapacity);

    ObjThread *objThread = ALLOCATE(vm, ObjThread);
    initObjHeader(vm, &objThread->objHeader, OT_THREAD, vm->threadClass);

    objThread->frames = frames;
    objThread->frameCapacity = INITIAL_FRAME_NUM;
    objThread->stack = newStack;
    objThread->stackCapacity = stackCapacity;

    resetThread(objThread, objClosure);
    return objThread;
}

/**
 * 重置thread
 * @param objThread
 * @param objClosure
 */
void resetThread(ObjThread *objThread, ObjClosure *objClosure) {
    objThread->esp = objThread->stack;
    objThread->openUpvalues = NULL;
    objThread->caller = NULL;
    objThread->errorObj = VT_TO_VALUE(VT_NULL);
    objThread->usedFrameNum = 0;

    ASSERT(objClosure != NULL, "objClosure is NULL in function resetThread");
    prepareFrame(objThread, objClosure, objThread->stack);
}