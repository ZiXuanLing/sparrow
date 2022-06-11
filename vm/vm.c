#include "vm.h"
#include "core.h"

/**
 * @brief 初始化虚拟机
 *
 * @param vm
 */
void initVM(VM *vm) {
    vm->allocatedBytes = 0;
    vm->allObjects = NULL;
    vm->curParser = NULL;
    StringBufferInit(&vm->allMethodNames);
    vm->allObjects = NULL;
    vm->curParser = NULL;
}

/**
 * @brief
 *
 * @return VM*
 */
VM* newVM(void) {
    VM* vm = (VM *)malloc(sizeof(VM));
    if (vm == NULL) {
        MEM_ERROR("allcate VM failed!");
    }
    initVM(vm);
    buildCore(vm);
    return vm;
}