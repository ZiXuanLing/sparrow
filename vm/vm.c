#include "vm.h"

/**
 * @brief 初始化虚拟机
 *
 * @param vm
 */
void initVM(VM *vm) {
    vm->allocatedBytes = 0;
    vm->curParser = 0;
}

/**
 * @brief
 *
 * @return VM*
 */
VM* newVM(void) {
    VM* vm = (VM *)malloc(sizeof(VM));
    if (vm == NULL) {

    }
    initVM(vm);
    return vm;
}