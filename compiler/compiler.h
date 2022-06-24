//
// Created by ZiXuan on 2022/6/12.
//

#ifndef SPARROW_COMPILER_H
#define SPARROW_COMPILER_H

#include "../object/obj_fn.h"

#define MAX_LOCAL_VAR_NUM 128
#define MAX_UPVALUE_NUM 128
#define MAX_ID_LEN 128 // 变量名最大长度

#define MAX_METHOD_NAME_LEN MAX_ID_LEN
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

//不关注左操作符的符号称为前缀符号
// 用于如字面量、变量名，前缀符合等非运算符
#define PREFIX_SYMBOL(nud) {NULL, BP_NONE, nud, NULL, NULL}

// 前缀运算符，如！
#define PREFIX_OPERATOR(id) {id, BP_NONE, unaryOperator, NULL, unaryMethodSignature}

// 关注左操作数的符合称为中缀符合
// 数组[,函数(
#define INFIX_SYMBOL(lbp, led) {NULL, lbp, NULL, led, NULL}

// 中缀运算符
#define INFIX_OPERATOR(id, lbp) {id, lbp, NULL, infixOperator, infixMethodSignature}

// 即可做前缀又可做中缀的运算符，如-
#define MIX_OPERATOR(id) {id, BP_TERM, unaryOperator, infixOperator, mixMethodSignature}

// 占位用
#define UNUSED_RULE {NULL, BP_NONE, NULL, NULL, NULL}

SymbolBindRule Rules[] = {
        UNUSED_RULE,
        PREFIX_SYMBOL(literal),
        PREFIX_SYMBOL(literal),
};

typedef struct {
    const char *id; // 符号

    // 左绑定权值，不关注左边操作数的符号此值为0
    BindPower lbp;

    //字面量，变量，前缀运算符等不关注左操作符的Token调用的方法
    DenotationFn nud;

    // 中缀运算符等关注左操作数的Token调用方法
    DenotationFn led;

    // 表示本符号在类中被视为一个方法
    // 为其生成一个方法签名
    methodSignatureFn methodSign;

} SymbolBindRule; // 符号绑定规则

typedef enum {
    VAR_SCOPE_INVALID,
    VAR_SCOPE_LOCAL,
    VAR_SCOPE_UPVALUE,
    VAR_SCOPE_MODULE
} VarScopeType;

typedef struct {
    VarScopeType scopeType; // 变量的作用域
    // 根据scopeType的值
    // 此索引可能指向局部变量或者upvalue或模块变量
    int index;
} Variable;

typedef struct compileUnit CompileUnit;
int defineModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value);
ObjFn* compileModule(VM *vm, ObjModule *objModule, const char *moduleCore);
static void initCompileUint(Parser *parser, CompileUnit *cu, CompileUnit *enclosingUnit, bool isMethod);
static int writeByte(CompileUnit *cu, int byte);
static void writeOpCode(CompileUnit *cu, OpCode opCode);
static int writeByteOperand(CompileUnit *cu, int operand);
inline static void writeShortOperand(CompileUnit *cu, int operand);
static int writeOpCodeByteOperand(CompileUnit *cu, OpCode opCode, int operand);
static void writeOpCodeShortOperand(CompileUnit *cu, OpCode opCode, int operand);
static uint32_t addConstant(CompileUnit *cu, Value constant);
static void emitLoadConstant(CompileUnit *cu, Value value);
static void literal(CompileUnit *cu, bool canAssign UNUSED);
static uint32_t sign2String(Signature *sign, char *buf);
static void expression(CompileUnit *cu, BindPower rbp);
static void emitCallBySignature(CompileUnit *cu, Signature *sign, OpCode opcode);
static void emitCall(CompileUnit *cu, int numArgs, const char *name, int length);
static void infixOperator(CompileUnit *cu, bool canAssign UNUSED);
static void unaryOperator(CompileUnit *cu, bool canAssign UNUSED);
static uint32_t addLocalVar(CompileUnit *cu, const char *name, uint32_t length);
static int declareLocalVar(CompileUnit *cu, const char *name, uint32_t length);
static int declareVariable(CompileUnit *cu, const char *name, uint32_t length);
static void unaryMethodSignature(CompileUnit *cu UNUSED, Signature *sign UNUSED);
static void infixMethodSignature(CompileUnit *cu, Signature *sign);
static void mxMethodSignature(CompileUnit *cu, Signature *sign);
static int declareModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value);
static CompileUnit* getEnclosingBKUnit(CompileUnit *cu);
static ClassBookKeep* getEnclosingClassBK(CompileUnit *cu);
static void processArgList(CompileUnit *cu, Signature *sign);
static void processParaList(CompileUnit *cu, Signature *sign);
static bool trySetter(CompileUnit *cu, Signature *sign);
static void idMethodSignature(CompileUnit *cu, Signature *sign);
static int findLocal(CompileUnit *cu, const char *name, uint32_t length);
static int addUpvalue(CompileUnit *cu, bool isEnclosingLocalVar, uint32_t index);
static int findUpvalue(CompileUnit *cu, const char *name, uint32_t length);
static Variable getVarFromLocalOrUpvalue(CompileUnit *cu, const char *name, uint32_t length);
static void emitLoadVariable(CompileUnit *cu, Variable var);
static void emitStoreVariable(CompileUnit *cu, Variable var);
static void emitLoadOrStoreVariable(CompileUnit *cu, bool canAssign, Variable var);
static void emitLoadThis(CompileUnit *cu);
static void compileBlock(CompileUnit *cu);
static void compileBody(CompileUnit *cu, bool isConstruct);
static ObjFn* endCompileUnit(CompileUnit *cu);
static void emitGetterMethodCall(CompileUnit *cu, Signature *sign, OpCode opCode);
static void emitMethodCall(CompileUnit *cu, const char *name, uint32_t length, OpCode opCode, bool canAssign);
static bool isLocalName(const char *name);
static void id(CompileUnit *cu, bool canAssign);
static void emitLoadModuleVar(CompileUnit *cu, const char *name);
static void stringInterpolation(CompileUnit *cu, bool canAssign UNUSED);
static void boolean(CompileUnit *cu, bool canAssign UNUSED);
static void null(CompileUnit *cu, bool canAssign UNUSED);
static void this(CompileUnit *cu, bool canAssign UNUSED);
static void super(CompileUnit *cu, bool canAssign);
static void parentheses(CompileUnit *cu, bool canAssign UNUSED);
static void listLiteral(CompileUnit *cu, bool canAssign UNUSED);
static void subscript(CompileUnit *cu, bool canAssign);
static void subscriptMethodSignature(CompileUnit *cu, Signature *sign);
static void callEntry(CompileUnit *cu, bool canAssign);
static void mapLiteral(CompileUnit *cu, bool canAssign UNUSED);
static uint32_t emitInstrWithPlaceholder(CompileUnit *cu, OpCode opCode);
static void patchPlaceholder(CompileUnit *cu, uint32_t absIndex);
static void logicOr(CompileUnit *cu, bool canAssign UNUSED);
static void logicAnd(CompileUnit *cu, bool canAssign UNUSED);
static void condition(CompileUnit *cu, bool canAssign UNUSED);
static void compileDefinition(CompileUnit *cu, bool isStatic);
static void compileIfStatement(CompileUnit *cu);
static void compileStatement(CompileUnit *cu);
static void enterLoopSetting(CompileUnit *cu, Loop *loop);
static void compileLoopBody(CompileUnit *cu);
uint32_t getBytesOfOperands(Byte *instrStream, Value *constants, int ip);
static void leaveLoopPatch(CompileUnit *cu);
static void compileWhileStatment(CompileUnit *cu);
static uint32_t discardLocalVar(CompileUnit *cu, int scopeDepth);
inline static void compileReturn(CompileUnit *cu);
inline static void compileBreak(CompileUnit *cu);
inline static void compileContinue(CompileUnit *cu);
static void enterScope(CompileUnit *cu);
static void leaveScope(CompileUnit *cu);
static void compileForStatment(CompileUnit *cu);
static void emitStoreModuleVar(CompileUnit *cu, int index);
static int declareMethod(CompileUnit *cu, char *signStr, uint32_t length);
static void defineMethod(CompileUnit *cu, Variable classVar, bool isStatic, int methodIndex);
static void emitCreateInstance(CompileUnit *cu, Signature *sign, uint32_t constructorIndex);
static void compileMethod(CompileUnit *cu, Variable classVar, bool isStatic);
static void compileClassBody(CompileUnit *cu, Variable classVar);
static void compileClassDefinition(CompileUnit *cu);
static void compileFunctionDefinition(CompileUnit *cu);
static void compileImport(CompileUnit *cu);

#endif //SPARROW_COMPILER_H
