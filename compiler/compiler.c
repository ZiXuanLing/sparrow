//
// Created by ZiXuan on 2022/6/12.
//
#include "compiler.h"
#include "../parser/parser.h"
#include "../vm/core.h"
#include "../object/class.h"

#include <string.h>


#ifdef DEBUG
#include "debug.h"
#endif

struct compileUnit {
    ObjFn *fn; // 所编译的函数

    LocalVar localVars[MAX_LOCAL_VAR_NUM]; // 作用域中所允许的局部变量上限

    uint32_t localVarNum; // 已分配的局部变量个数

    Upvalue upvalues[MAX_UPVALUE_NUM]; //记录本层所引用的upvalue

    int scopeDepth; // 此项表示当前正在编译的代码所处的作用域

    uint32_t stackSlotNum; // 当前使用的slot个数

    Loop *curLoop; // 当前正在编译的循环层

    ClassBookKeep *enclosingClassBK; // 当前正在编译的类的编译信息

    struct compileUnit *enclosingUnit; // 包含此编译单元的编译单元，即直接外层

    Parser *curParser; // 当前parser

}; // 编译单元

int defineModuleVar(VM *vm, ObjModule *objModule,
                    const char *name,
                    uint32_t length,
                    Value value) {
    if (length > MAX_ID_LEN) {
        char id[MAX_ID_LEN] = {'\0'};
        memcpy(id, name, length);
        if (vm->curParser != NULL) {
            COMPILE_ERROR(vm->curParser, "length of identifier \"%s\" should be no more than %d", id, MAX_ID_LEN);
        }
        else {
            MEM_ERROR("length of identifier \"%s\" should be no more than %d", id, MAX_ID_LEN);
        }
    }

    // 从模块变量名中查找变量，若不存在就添加
    int symbolIndex = getIndexFromSymbolTable(&objModule->moduleVarName, name, length);
    if (symbolIndex == - 1) {
        symbolIndex = addSymbol(vm, &objModule->moduleVarName, name, length);
        ValueBufferAdd(vm, &objModule->moduelVarValue, value);
    }
    else if (VALUE_IS_NUM(objModule->moduelVarValue.datas[symbolIndex])) {
        objModule->moduelVarValue.datas[symbolIndex] = value;
    }
    else {
        symbolIndex = -1; // 已定义则返回01，用于判断重定义
    }

    return symbolIndex;
}

ObjFn* compileModule(VM *vm, ObjModule *objModule, const char *moduleCore) {
    ;
}