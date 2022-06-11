#include "header_obj.h"
#include "../vm/vm.h"
#include "class.h"


DEFINE_BUFFER_METHOD(Value)

/**
 * @brief 初始化对象头
 * 
 * @param vm 
 * @param objHeader 
 * @param objType 
 */
void initObjHeader(VM *vm, ObjHeader *objHeader, ObjType objType, Class *class) {
    objHeader->type = objType;
    objHeader->isDark = false;
    objHeader->class = class;
    objHeader->next = vm->allObjects;
    vm->allObjects = objHeader;
}