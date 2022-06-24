#ifndef __SPARROW_VM_H__

#define __SPARROW_VM_H__

#include "../include/common.h"
#include "../object/header_obj.h"
#include "../object/obj_map.h"
#include "../object/obj_thread.h"

// 为定义opcode.inc中的操作码加上前缀OPCODE_
#define OPCODE_SLOTS(opcode, effect) OPCODE_##opcode,
typedef enum {
#include "opcode.inc"
} OpCode;
#undef OPCODE_SLOTS

typedef enum vmResult {
    VM_RESULT_SUCCESS,
    VM_RESULT_ERROR
} VMResult;  // 虚拟机执行结果
// 如果输出无误，可以将字节码输出到文件缓存，避免下次重新编译

struct vm {
    Class *classOfClass;
    Class *objectClass;
    Class *mapClass;
    Class *nullClass;
    Class *boolClass;
    Class *numClass;
    Class *threadClass;
    Class *rangeClass;
    Class *listClass;
    Class *fnClass;
    Class *stringClass;
    uint32_t allocatedBytes; // 累计已分配的内存量
    Parser *curParser; // 当前词法分析器
    struct ObjHeader *allObjects; // 所有已分配对象链表
    SymbolTable allMethodNames; // 所有类的方法名
    ObjMap *allModules;
    ObjThread *curThread; // 当前正在执行的线程
};

void initVM(struct vm *vm);
VM* newVM(void);

#endif // !__SPARROW_VM_H__