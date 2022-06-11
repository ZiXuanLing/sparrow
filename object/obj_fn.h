#ifndef SPARROW_OBJ_FN_H
#define SPARROW_OBJ_FN_H

#include "meta_obj.h"
#include "../include/utils.h"

typedef struct {
    char *fnName; // 函数名
    IntBuffer lineNo;// 行号
} FnDebug; // 在函数中的调试结构

typedef struct {
    ObjHeader objHeader;
    ByteBuffer instrStream; // 函数编译后的指令流
    ValueBuffer constants; // 函数中的常量表

    ObjModule *module; // 本函数所属模块

    uint32_t maxStackSlotUsedNum;
    uint32_t upvalueNum; // 本函数所涵盖的upvalue
    uint8_t argNum; // 函数期望的参数个数
#if DEBUG
    FnDebug debug;
#endif
} ObjFn;  // 函数对象

typedef struct upvalue {
    ObjHeader objHeader;

    Value *localVarPtr;

    Value closedUpvalue;  // 已被关闭的upvalue

    struct upvalue *next; // 用以链接openUpvalue链表
} ObjUpvalue;  // upvalue对象

typedef struct {
    ObjHeader objHeader;
    ObjFn *fn; // 闭包中所要引用的函数

    ObjUpvalue *upvalues[0]; // 用以存储该函数的closed upvalue
} ObjClosure;  // 闭包对象

typedef struct {
    uint8_t *ip;  // 程序计数器，指向下一个将被指向的指令

    ObjClosure *closure; // 在本frame中执行的闭包函数

    // 此项用于指向frame所在thread运行时栈的起始地址
    Value *stackStart;  // frame是共享thread.stack

} Frame;  // 调用框架

#define INITIAL_FRAME_NUM 4

ObjUpvalue* newObjUpvalue(VM *vm, Value *localVarPtr);
ObjClosure* newObjClosure(VM *vm, ObjFn *objFn);
ObjFn* newObjFn(VM *vm, ObjModule *objModule, uint32_t maxStackSlotUsedNum);

#endif //SPARROW_OBJ_FN_H
