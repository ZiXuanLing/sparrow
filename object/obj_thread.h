//
// Created by ZiXuan on 2022/6/12.
//

#ifndef SPARROW_OBJ_THREAD_H
#define SPARROW_OBJ_THREAD_H

#include "obj_fn.h"

typedef struct objThread {
    ObjHeader objHeader;
    Value *stack; // 运行时的栈
    Value *esp;
    uint32_t stackCapacity;

    Frame *frames;
    uint32_t usedFrameNum;
    uint32_t frameCapacity;

    ObjUpvalue *openUpvalues;

    // 当前thread调用者
    struct objThread *caller;
    // 导致运行时错误的对象会放在此处，否则为空
    Value errorObj;
} ObjThread;  // 线程对象

void prepareFrame(ObjThread *objThread, ObjClosure *objClosure, Value *stackStart);
ObjThread* newObjThread(VM *vm, ObjClosure *objClosure);
void resetThread(ObjThread *objThread, ObjClosure *objClosure);

#endif //SPARROW_OBJ_THREAD_H
