#ifndef __SPARROW_VM_H__

#define __SPARROW_VM_H__

#include "../include/common.h"

struct vm {
    uint32_t allocatedBytes; // 累计已分配的内存量
    Parser *curParser; // 当前词法分析器
};

void initVM(VM *vm);
VM* newVM(void);

#endif // !__SPARROW_VM_H__