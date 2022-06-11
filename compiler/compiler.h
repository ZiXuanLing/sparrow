//
// Created by ZiXuan on 2022/6/12.
//

#ifndef SPARROW_COMPILER_H
#define SPARROW_COMPILER_H

#include "../object/obj_fn.h"

#define MAX_LOCAL_VAR_NUM 128
#define MAX_UPVALUE_NUM 128
#define MAX_ID_LEN 128 // 变量名最大长度

#define MAX_METHOD_NAME_LEN NAX_ID_LEN
#define MAX_ARG_NUM 16

#define MAX_SIGN_LEN MAX_METHOD_NAME_LEN + MAX_ARG_NUM * 2 + 1

#define MAX_FILED_NUM 128

typedef struct {
    int isEnclosingLocalVar;
    uint32_t index;
} Upvalue;

typedef struct {
    const char *name;
    uint32_t length;
    int scopeDepth; //局部变量作用域
    int isUpvalue;
} LocalVar;  // 局部变量

typedef enum {
    SIGN_CONSTRUCT, // 构造函数
    SIGN_METHOD, // 普通方法
    SIGN_GETTER, // getter
    SIGN_SETTER, // setter
    SIGN_SUBSCRIPT, // getter形式的下标
    SIGN_SUBSCRIPT_SETTER, // setter形式的下标
} SignatureType; // 方法的签名

typedef struct {
    SignatureType type; // 签名类型
    const char *name; // 签名
    uint32_t length; // 签名长度
    uint32_t argNum; // 参数个数
} Signature; // 签名

typedef struct loop {
    int condStartIndex;
    int bodyStartIndex;
    int scopeDepth;
    int exitIndex;
    struct loop *enclosingLoop;
} Loop;

typedef struct {
    ObjString *name; // 类名
    SymbolTable fields; // 类属性符号表
    int inStatic; // 若当前编译静态方法就为真
    IntBuffer instantMethods; // 实例方法
    IntBuffer staticMethods; // 静态方法
    Signature *signature; // 当前正在编译的签名
} ClassBookKeep; // 用于记录类编译时的信息

typedef struct compileUnit CompileUnit;
int defineModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value);

#endif //SPARROW_COMPILER_H
