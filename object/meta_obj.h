#ifndef SPARROW_META_OBJ_H
#define SPARROW_META_OBJ_H

/**
 * 元信息，仅内部可见
 */

#include "obj_string.h"
#include "header_obj.h"

typedef struct {
    ObjHeader objHeader;
    SymbolTable moduleVarName; // 模块中的模块变量名
    ValueBuffer  moduelVarValue; // 模块中的模块变量值
    ObjString  *name;
} ObjModule; // 模块对象

typedef struct {
    ObjHeader  objHeader;
    Value  fields[0];
} ObjInstance;  // 对象实例

ObjModule * newObjModule(VM *vm, const char *modName);
ObjInstance* newObjInstance(VM *vm, Class *class);

#endif //SPARROW_META_OBJ_H
