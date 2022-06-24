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

// 把opcode定义到数组opCodeSlotsUsed中
#define OPCODE_SLOTS(OpCode, effect) effect,
static const int OpCodeSlotsUsed[] = {
#include "opcode.inc"
};
#undef OPCODE_SLOTS

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

/**
 * 初始化CompileUnit
 * @param parser
 * @param cu
 * @param enclosingUnit
 * @param isMethod
 */
static void initCompileUint(Parser *parser, CompileUnit *cu, CompileUnit *enclosingUnit, bool isMethod) {
    parser->curCompileUnit = cu;
    cu->curParser = parser;
    cu->enclosingUnit = enclosingUnit;
    cu->curLoop = NULL;
    cu->enclosingClassBK = NULL;

    // 若没有外层，说明当前属于模块作用域
    if (enclosingUnit == NULL) {
        // 编译代码时是从上到下从最外层的模块作用域开始，模块作用域设为-1
        cu->scopeDepth = -1;
        // 模块作用域中没有局部变量
        cu->localVarNum = 0;
    }
    else { // 若是内层单元
        if (isMethod) { // 若是类中的方法
            // 如果是类的方法就设定饮食thisWie第0个局部变量，即实例对象
            // 他是方法（消息）的接受者，this这种特殊对象被处理为局部变量
            cu->localVars[0].name = "this";
            cu->localVars[0].length = 4;
        }
        else { // 若为普通函数
            // 空出第0个局部变量，保持统一
            cu->localVars[0].name = NULL;
            cu->localVars[0].length = 0;
        }
        // 第0个局部变量的特殊性使其作用域为模块级别
        cu->localVars[0].scopeDepth = -1;
        cu->localVars[0].isUpvalue = false;
        cu->localVarNum = 1;

        // 对于函数和方法来说，初始作用域就是局部作用域
        // 0表示局部作用域和的最外层
        cu->scopeDepth = 0;
    }
    // 局部变量保存在栈中，初始时栈中已使用的slot数量等于局部变量的数量
    cu->stackSlotNum = cu->localVarNum;

    cu->fn = newObjFn(cu->curParser->vm, cu->curParser->curModule, cu->localVarNum);
}

/**
 * 往函数的指令流中写入1字节，返回其索引
 * @param cu
 * @param byte
 * @return
 */
static int writeByte(CompileUnit *cu, int byte) {
    // 若在调试状态，额外在debug->lineNo中写入当前token行号
#ifdef DEBUG
    IntBufferAdd(cu->curParser->vm,
                 &cu->fn->debug.lineNo, cu->curParser->preToken.lineNo);
#endif
    ByteBufferAdd(cu->curParser->vm,
                  &cu->fn->instrStream, (uint8_t) byte);
    return (int)cu->fn->instrStream.count - 1;
}

/**
 * 写入操作码
 * @param cu
 * @param opCode
 */
static void writeOpCode(CompileUnit *cu, OpCode opCode) {
    writeByte(cu, opCode);
    // 累计需要的运行时空间大小
    cu->stackSlotNum += OpCodeSlotsUsed[opCode];
    if (cu->stackSlotNum > cu->fn->maxStackSlotUsedNum) {
        cu->fn->maxStackSlotUsedNum = cu->stackSlotNum;
    }
}

/**
 * 写入1B的操作数
 * @param cu
 * @param operand
 * @return
 */
static int writeByteOperand(CompileUnit *cu, int operand) {
    return writeByte(cu, operand);
}

/**
 * 写入2字节的操作数 按大端字节序写入参数
 * @param cu
 * @param operand
 */
inline static void writeShortOperand(CompileUnit *cu, int operand) {
    writeByte(cu, (operand >> 8) & 0xff);
    writeByte(cu, operand & 0xff);
}

/**
 * 写入操作数为1字节大小的指令
 * @param cu
 * @param opCode
 * @param operand
 * @return
 */
static int writeOpCodeByteOperand(CompileUnit *cu, OpCode opCode, int operand) {
    writeByte(cu, opCode);
    return writeByteOperand(cu, operand);
}

/**
 * 写入操作数为2字节大小的指令
 * @param cu
 * @param opCode
 * @param operand
 * @return
 */
static void writeOpCodeShortOperand(CompileUnit *cu, OpCode opCode, int operand) {
    writeByte(cu, opCode);
    writeShortOperand(cu, operand);
}

/**
 * 编译模块
 * @param vm
 * @param objModule
 * @param moduleCore
 * @return
 */
ObjFn* compileModule(VM *vm, ObjModule *objModule, const char *moduleCore) {
    // 各源码模块文件需要单独的parser
    Parser parser;
    parser.parent = vm->curParser;
    vm->curParser = &parser;

    if (objModule->name == NULL) {
        // 核心模块是core.script.inc
        initParser(vm, &parser, "core.script.inc", moduleCore, objModule);
    }
    else {
        initParser(vm, &parser, (const char *)objModule->name->value.start, moduleCore, objModule);
    }

    CompileUnit moduleCU;
    initCompileUint(&parser, &moduleCU, NULL, false);

    // 记录现在模块变量的数量，后面检查预定义模块变量时可减少遍历
    uint32_t moduleVarNumBdfor = objModule->moduelVarValue.count;

    // 初始的parser->curToken.type为TOKEN_UNKNOWN，先使其指向第一个合法的token
    getNextToken(&parser);

    // 此时compilerProgram为桩函数，并不会读进token，因此是死循环
    while (!matchToken(&parser, TOKEN_EOF)) {
        compileProgram(&moduleCU);
    }

    // other
}

typedef enum {
    BP_NONE, // 无绑定能力

    // 从上到下，优先级越来越高
    BP_LOWEST, // 最低绑定能力
    BP_ASSIGN, // =
    BP_CONDITION, // ?:
    BP_LOGIC_OR, // ||
    BP_LOGIC_AND, // &&
    BP_EQUAL, // == !=
    BP_IS, // is
    BP_CMP, // < >  <= >=
    BP_BIT_OR, // |
    BP_BIT_AND, // &
    BP_BIT_SHIFT, // << >>
    BP_RANGE, // ..
    BP_TERM, // + -
    BP_FACTOR, // * / %
    BP_UNARY, // - ! ~
    BP_CALL, // .() []
    BP_HIGHEST
} BindPower; // 定义了操作符的绑定权值，即优先级

// 指示符函数指针
typedef void (*DenotationFn) (CompileUnit *CU, bool canAssign);

// 签名函数指针
typedef void (*methodSignatureFn) (CompileUnit *cu, Signature *signature);

/**
 * 添加常量并返回其索引
 * @param cu
 * @param constant
 * @return
 */
static uint32_t addConstant(CompileUnit *cu, Value constant) {
    ValueBufferAdd(cu->curParser->vm, &cu->fn->constants, constant);
    return cu->fn->constants.count - 1;
}

/**
 * 生成加载常量的指令
 * @param cu
 * @param value
 */
static void emitLoadConstant(CompileUnit *cu, Value value) {
    int index = (int)addConstant(cu, value);
    writeOpCodeShortOperand(cu, OPCODE_LOAD_CONSTANT, index);
}

/**
 * 数字和字符.nud() 编译字面量
 * @param cu
 * @param canAssign
 */
static void literal(CompileUnit *cu, bool canAssign UNUSED) {
    emitLoadConstant(cu, cu->curParser->preToken.value);
}

/**
 * 把Signature转换为字符串，返回字符串长度
 * @param sign
 * @param buf
 * @return
 */
static uint32_t sign2String(Signature *sign, char *buf) {
    uint32_t pos = 0;

    // 复制方法名xxx
    memcpy(buf + pos, sign->name, sign->length);
    pos += sign->length;

    // 以下单独处理方法名之后的部分
    switch (sign->type) {
        case SIGN_GETTER:
            break;
        case SIGN_SETTER:
            buf[pos ++] = '=';
            // 下面添加=右边的赋值，只支持一个赋值
            buf[pos ++] = '(';
            buf[pos ++] = '_';
            buf[pos ++] = ')';
            break;

        case SIGN_CONSTRUCT:
        case SIGN_METHOD:
            buf[pos ++] = '(';
            uint32_t idx = 0;
            while (idx < sign->argNum) {
                buf[pos ++] = '_';
                buf[pos ++] = ',';
                idx ++;
            }
            if (idx == 0) { // 说明没有参数
                buf[pos ++] = ')';
            }
            else {
                buf[pos - 1] = ')';
            }
            break;
        case SIGN_SUBSCRIPT: {
            buf[pos ++] = '[';
            uint32_t idx = 0;
            while (idx < sign->argNum) {
                buf[pos ++] = '_';
                buf[pos ++] = ',';
                idx ++;
            }
            if (idx == 0) {// 说明没有参数
                buf[pos ++] = ']';
            }
            else {
                buf[pos - 1] = ']';
            }
            break;
        }
        case SIGN_SUBSCRIPT_SETTER: {
            buf[pos ++] = '[';
            uint32_t idx = 0;
            // argNum包括了等号右边的一个赋值参数
            // 这里是处理等号左边的subscript中的参数列表，因此减1
            // 后面专门添加该参数
            while (idx < sign->argNum - 1) {
                buf[pos ++] = '_';
                buf[pos ++] = ',';
                idx ++;
            }
            if (idx == 0) {  // 说明没有参数
                buf[pos ++] = ']';
            }
            else {
                buf[pos - 1 ] = ']';
            }

            // 下面为等号右边的参数构造签名部分
            buf[pos ++] = '=';
            buf[pos ++] = '(';
            buf[pos ++] = '_';
            buf[pos ++] = ')';

        }
    }
    buf[pos] = '\0';
    return pos; //返回签名串的长度
}

/**
 * 语法分析的核心
 * @param cu
 * @param rbp
 */
static void expression(CompileUnit *cu, BindPower rbp) {
    // 以中缀运算符aSwTe为例
    // 大小字符表示运算符，小写字符表示操作数

    // 进入expression时，curToken是操作数w，preToken是运算符S
    DenotationFn nud = Rules[cu->curParser->curToken.type].nud;

    // 表达式开头的要么是操作数要么是前缀运算符，必然有nud方法
    ASSERT(nud != NULL, "nud is NULL!");

    getNextToken(cu->curParser);  // 执行后curToken为运算符T

    bool canAssign = rbp < BP_ASSIGN;
    nud(cu, canAssign); // 计算操作数w的值

    while (rbp < Rules[cu->curParser->curToken.type].lbp) {
        DenotationFn led = Rules[cu->curParser->curToken.type].led;
        getNextToken(cu->curParser); // 执行后curToken为操作数e
        led(cu, canAssign); // 计算运算符T.led方法
    }
}

/**
 * 通过签名编译方法调用，包括callx和superx指令
 * @param cu
 * @param sign
 * @param opcode
 */
static void emitCallBySignature(CompileUnit *cu, Signature *sign, OpCode opcode) {

    char signBuffer[MAX_SIGN_LEN];
    uint32_t length = sign2String(sign, signBuffer);

    // 确保签名录入到vm->alMethodNames
    int symbolIndex = ensureSymbolExist(cu->curParser->vm,
                                         &cu->curParser->vm->allMethodNames, signBuffer, length);
    writeOpCodeShortOperand(cu, opcode + sign->argNum, symbolIndex);

    // 此时在常量表中创建一个空slot位，将来绑定方法时再装入基类
    if (opcode == OPCODE_SUPER0) {
        writeShortOperand(cu, (int)addConstant(cu, VT_TO_VALUE(VT_NUM)));
    }
}

/**
 * 生成调用的指令，仅限callX指令
 * @param cu
 * @param numArgs
 * @param name
 * @param length
 */
static void emitCall(CompileUnit *cu, int numArgs, const char *name, int length) {
    int symbolIndex = ensureSymbolExist(cu->curParser->vm,
                                        &cu->curParser->vm->allMethodNames, name, length);
    writeOpCodeShortOperand(cu, OPCODE_CALL0 + numArgs, symbolIndex);
}

/**
 * 中缀运算符.led方法
 * @param cu
 * @param canAssign
 */
static void infixOperator(CompileUnit *cu, bool canAssign UNUSED) {
    SymbolBindRule *rule = &Rules[cu->curParser->preToken.type];

    // 中缀运算符对左右操作数的绑定权值一样
    BindPower rbp = rule->lbp;
    expression(cu,rbp);  // 解析右操作数

    // 生成一个参数的签名
    Signature sign = {SIGN_METHOD, rule->id, strlen(rule->id), 1};
    emitCallBySignature(cu, &sign, OPCODE_CALL0);
}

/**
 * 前缀运算符 nud方法
 * @param cu
 * @param canAssign
 */
static void unaryOperator(CompileUnit *cu, bool canAssign UNUSED) {
    SymbolBindRule *rule = &Rules[cu->curParser->preToken.type];

    // BP_UNARY作为rbp调用expression解析右操作数
    expression(cu, BP_UNARY);

    // 生成调用前缀运算符的指令
    // 0个参数，前缀运算符都是1个字符，长度是1
    emitCall(cu, 0, rule->id, 1);
}

/**
 * 添加局部变量到cu
 * @param cu
 * @param name
 * @param length
 * @return
 */
static uint32_t addLocalVar(CompileUnit *cu, const char *name, uint32_t length) {
    LocalVar *var = &(cu->localVars[cu->localVarNum]);
    var->name = name;
    var->length = length;
    var->scopeDepth = cu->scopeDepth;
    var->isUpvalue = false;
    return cu->localVarNum ++;
}

/**
 * 声明局部变量
 * @param cu
 * @param name
 * @param length
 * @return
 */
static int declareLocalVar(CompileUnit *cu, const char *name, uint32_t length) {
    if (cu->localVarNum >= MAX_LOCAL_VAR_NUM) {
        COMPILE_ERROR(cu->curParser,
                      "the max length of local variable of one scope is %d", MAX_LOCAL_VAR_NUM);
    }

    // 判断当前作用域中改变量是否已存在
    int idx = (int) cu->localVarNum - 1;
    while (idx >= 0) {
        LocalVar *var = &cu->localVars[idx];

        // 只在当前作用域中查找同名变量
        // 如果导论父作用域就退出，减少没必要的遍历
        if (var->scopeDepth < cu->scopeDepth) {
            break;
        }

        if (var->length == length && memcmp(var->name, name, length) == 0) {
            char id[MAX_ID_LEN] = {'\0'};
            memcpy(id, name, length);
            COMPILE_ERROR(cu->curParser, "identifier \"%s\" redefinition!", id);
        }
        idx --;
    }

    // 检查过声明该局部变量
    return (int) addLocalVar(cu, name, length);
}

/**
 * 根据作用域声明变量
 * @param cu
 * @param name
 * @param length
 * @return
 */
static int declareVariable(CompileUnit *cu, const char *name, uint32_t length) {
    // 若当前是模块作用域就声明为模块变量
    if (cu->scopeDepth == -1) {
        int index = defineModuleVar(cu->curParser->vm,
                                    cu->curParser->curModule, name, length, VT_TO_VALUE(VT_NUM));
        if (index == -1) {
            char id[MAX_ID_LEN] = {'\0'};
            memcpy(id, name, length);
            COMPILE_ERROR(cu->curParser, "identifier \"%s\" redefinition!", id);
        }
        return index;
    }

    // 否则是局部作用域，声明局部变量
    return declareLocalVar(cu, name, length);
}

/**
 * 为单运算符方法创建签名
 * @param cu
 * @param sign
 */
static void unaryMethodSignature(CompileUnit *cu UNUSED, Signature *sign UNUSED) {
    // 名称部分在调用前已经完成，只修改类型
    sign->type = SIGN_GETTER;
}

/**
 * 为中缀运算符创建签名
 * @param cu
 * @param sign
 */
static void infixMethodSignature(CompileUnit *cu, Signature *sign) {
    // 在类中的运算符都是方法，类型都是SIGN_METHOD
    sign->type = SIGN_METHOD;

    //中缀运算符只有一个参数，故初始为1
    sign->argNum = 1;
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' after infix operator!");
    consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
    declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);

    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')after pparameter!");
}

/**
 * 为既做单运算符又做中缀运算符的符号方法创建签名
 * @param cu
 * @param sign
 */
static void mxMethodSignature(CompileUnit *cu, Signature *sign) {
    // 假设是单运算符方法，因此默认为getter
    sign->type = SIGN_GETTER;

    // 若后面有)，说明其为中缀运算符，那就置其类型为SIGN_METHOD
    if (matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
        sign->type = SIGN_METHOD;
        sign->argNum =  1;
        consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
        declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
        consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter!");
    }
}

/**
 * 声明模块变量，与definemodulevar的区别是不做重定义检查，默认为声明
 * @param vm
 * @param objModule
 * @return
 */
static int declareModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value) {
    ValueBufferAdd(vm, &objModule->moduelVarValue, value);
    return addSymbol(vm, &objModule->moduleVarName, name, length);
}

/**
 * 返回包含cu->enclosingClassBK的最近的CompileUnit
 * @param cu
 * @return
 */
static CompileUnit* getEnclosingBKUnit(CompileUnit *cu) {
    while (cu != NULL) {
        if (cu->enclosingClassBK != NULL) {
            return cu;
        }
        cu = cu->enclosingUnit;
    }
    return NULL;
}

/**
 * 返回包含cu最近的ClassBookKeep
 * @param cu
 * @return
 */
static ClassBookKeep* getEnclosingClassBK(CompileUnit *cu) {
    CompileUnit *ncu = getEnclosingBKUnit(cu);
    if (ncu != NULL) {
        return ncu->enclosingClassBK;
    }
    return NULL;
}

/**
 * 为实参列表中的各个实参生成加载实参的指令
 * @param cu
 * @param sign
 */
static void processArgList(CompileUnit *cu, Signature *sign) {
    // 由主调方保障参数不空
    ASSERT(cu->curParser->curToken.type != TOKEN_RIGHT_PAREN &&
    cu->curParser->curToken.type != TOKEN_RIGHT_BRACKET, "empty argument list!");
    do {
        if (++ sign->argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }
        expression(cu, BP_LOWEST);// 加载实参
    } while (matchToken(cu->curParser, TOKEN_COMMA));
}

/**
 * 声明形参列表中的各个形参
 * @param cu
 * @param sign
 */
static void processParaList(CompileUnit *cu, Signature *sign) {
    // 由主调方保障参数不空
    ASSERT(cu->curParser->curToken.type != TOKEN_RIGHT_PAREN &&
           cu->curParser->curToken.type != TOKEN_RIGHT_BRACKET, "empty argument list!");
    do {
        if (++ sign->argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }
        consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
        declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
    } while (matchToken(cu->curParser, TOKEN_COMMA));
}

/**
 * 尝试编译setter
 * @param cu
 * @param sign
 * @return
 */
static bool trySetter(CompileUnit *cu, Signature *sign) {
    if (!matchToken(cu->curParser, TOKEN_ASSIGN)) {
        return false;
    }

    if (sign->type == SIGN_SUBSCRIPT) {
        sign->type = SIGN_SUBSCRIPT_SETTER;
    }
    else {
        sign->type = SIGN_SETTER;
    }

    // 读取等号右边的形参左边的(
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' after '='!");

    //读取形参
    consumeCurToken(cu->curParser, TOKEN_ID, "expect ID!");

    declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);

    //读取等号右边的形参右边的(
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')'after argument list!");

    sign->argNum ++;
    return true;
}

/**
 * 标识符的签名函数
 * @param cu
 * @param sign
 */
static void idMethodSignature(CompileUnit *cu, Signature *sign) {
    sign->type = SIGN_GETTER;

    if (sign->length == 3 && memcmp(sign->name, "new", 3) == 0) {

        if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
            COMPILE_ERROR(cu->curParser, "constructor shouldn't be setter!");
        }

        if (!matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
            COMPILE_ERROR(cu->curParser, "constructor shouldn't be method!");
        }

        sign->type = SIGN_CONSTRUCT;

        if (matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            return;
        }
    }
    else {  // 若不是构造函数
        if (trySetter(cu, sign)) {
            return;
        }

        if (!matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
            return;
        }

        sign->type = SIGN_METHOD;

        if (matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            return;
        }
    }

    //下面处理形参
    processParaList(cu, sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter list!");
}

/**
 * 查找局部变量
 * @param cu
 * @param name
 * @param length
 * @return
 */
static int findLocal(CompileUnit *cu, const char *name, uint32_t length) {
    // 内部作用域变量会覆盖外层，故从后往前，由最外层逐渐往外层找
    int index = (int) cu->localVarNum - 1;
    while (index >= 0) {
        if (cu->localVars[index].length == length &&
                memcmp(cu->localVars[index].name, name, length) == 0) {
            return index;
        }
        index --;
    }
    return -1;
}

/**
 * 添加upvalue到cu->upvalues，返回其索引，若已存在则只返回索引
 * @param cu
 * @param isEnclosingLocalVar
 * @param index
 * @return
 */
static int addUpvalue(CompileUnit *cu, bool isEnclosingLocalVar, uint32_t index) {
    uint32_t  idx = 0;
    while (idx < cu->fn->upvalueNum) {
        if (cu->upvalues[idx].index == index &&
            cu->upvalues[idx].isEnclosingLocalVar == isEnclosingLocalVar) {
            return (int) idx;
        }
        idx++;
    }

    cu->upvalues[cu->fn->upvalueNum].isEnclosingLocalVar = isEnclosingLocalVar;
    cu->upvalues[cu->fn->upvalueNum].index = index;
    return cu->fn->upvalueNum ++;
}

/**
 * 查找name指代的upvalue后添加到cu-upvalues，返回其索引，否则返回-1
 * @param cu
 * @param name
 * @param length
 * @return
 */
static int findUpvalue(CompileUnit *cu, const char *name, uint32_t length) {
    if (cu->enclosingUnit == NULL) {
        return -1;
    }

    if (!strchr(name, ' ') && cu->enclosingUnit->enclosingClassBK != NULL) {
        return -1;
    }

    int directOuterLocalIndex = findLocal(cu->enclosingUnit, name, length);

    if (directOuterLocalIndex != -1) {
        cu->enclosingUnit->localVars[directOuterLocalIndex].isUpvalue = true;
        return addUpvalue(cu, true, (uint32_t)directOuterLocalIndex);
    }

    int directOuterUpvalueIndex = findLocal(cu->enclosingUnit, name, length);;
    if (directOuterUpvalueIndex != -1) {
        return addUpvalue(cu, false, (uint32_t) directOuterUpvalueIndex);
    }
    // 执行到这里还是没有该upvalue对应的局部变量，返回-1
    return -1;
}

/**
 *
 * @param cu
 * @param name
 * @param length
 * @return
 */
static Variable getVarFromLocalOrUpvalue(CompileUnit *cu, const char *name, uint32_t length) {
    Variable var;

    // 默认为无效作用域类型，查找到后会被更正
    var.scopeType = VAR_SCOPE_INVALID;

    var.index = findLocal(cu, name, length);
    if (var.index != -1) {
        var.scopeType = VAR_SCOPE_LOCAL;
        return var;
    }
    var.index = findUpvalue(cu, name, length);
    if (var.index != -1) {
        var.scopeType = VAR_SCOPE_UPVALUE;
    }
    return var;
}

/**
 * 生成把变量var加载到栈的指令
 * @param cu
 * @param var
 */
static void emitLoadVariable(CompileUnit *cu, Variable var) {
    switch (var.scopeType) {
        case VAR_SCOPE_LOCAL:
            writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, var.index);
            break;
        case VAR_SCOPE_UPVALUE:
            writeOpCodeByteOperand(cu, OPCODE_LOAD_UPVALUE, var.index);
            break;
        case VAR_SCOPE_MODULE:
            writeOpCodeByteOperand(cu, OPCODE_LOAD_MODULE_VAR, var.index);
            break;
        default:
            NOT_REACHED();
    }
}

/**
 * 为变量var生成存储的指令
 * @param cu
 * @param var
 */
static void emitStoreVariable(CompileUnit *cu, Variable var) {
    switch (var.scopeType) {
        case VAR_SCOPE_LOCAL:
            writeOpCodeByteOperand(cu, OPCODE_STORE_LOCAL_VAR, var.index);
            break;
        case VAR_SCOPE_UPVALUE:
            writeOpCodeByteOperand(cu, OPCODE_STORE_UPVALUE, var.index);
            break;
        case VAR_SCOPE_MODULE:
            writeOpCodeByteOperand(cu, OPCODE_STORE_MODULE_VAR, var.index);
            break;
        default:
            NOT_REACHED();
    }
}

/**
 * 生成加载或存储变量的指令
 * @param cu
 * @param canAssign
 * @param var
 */
static void emitLoadOrStoreVariable(CompileUnit *cu, bool canAssign, Variable var) {
    if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
        expression(cu, BP_LOWEST);
        emitStoreVariable(cu, var);// 为var生成赋值指令
    }
    else {
        emitLoadVariable(cu, var); // 为var生成读取指令
    }
}

/**
 * 生成把实例对象this加载到栈的指令
 * @param cu
 */
static void emitLoadThis(CompileUnit *cu) {
    Variable var = getVarFromLocalOrUpvalue(cu, "this", 4);
    ASSERT(var.scopeType != VAR_SCOPE_INVALID, "get variable failed!");
    emitLoadVariable(cu, var);
}

/**
 * 编译代码块
 * @param cu
 */
static void compileBlock(CompileUnit *cu) {
    // 进入本函数之前已经读入了{
    while (!matchToken(cu->curParser, TOKEN_RIGHT_BRACE)) {
        if (PEEK_TOKEN(cu->curParser) == TOKEN_EOF) {
            COMPILE_ERROR(cu->curParser, "expect ')' at the end of block!");
        }
        compileProgram(cu);
    }
}

/**
 * 编译函数或方法体
 * @param cu
 * @param isConstruct
 */
static void compileBody(CompileUnit *cu, bool isConstruct) {
    // 进入本函数之前已经读入了{
    compileBlock(cu);
    if (isConstruct) {
        writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, 0);
    }
    else {
        // 否则加载null占位
        writeOpCode(cu, OPCODE_PUSH_NULL);
    }

    // 返回编译结果，若是构造函数就返回this， 否则返回null
    writeOpCode(cu, OPCODE_RETURN);
}

/**
 * 结束cu的编译工作，在其外层编译单元中为其创建闭包
 */
#if DEBUG
static ObjFn* endCompileUnit(CompileUnit *cu, const char *debugName, uint32_t debugNameLen) {
        bindDebugFnName(cu->curParser->vm, cu->fn->debug,debugName, debugNameLen);
#else
static ObjFn* endCompileUnit(CompileUnit *cu) {
#endif
    // 标识单元编译结束
    writeOpCode(cu, OPCODE_END);
    if (cu->enclosingUnit != NULL) {
        // 把当前编译的objFn作为常量添加到父编译单元的常量表
        uint32_t index = addConstant(cu->enclosingUnit, OBJ_TO_VALUE(cu->fn));
        // 内层函数以闭包形式在外层函数中存在
        // 在外层函数的指令流中添加“为当前内层函数创建闭包的指令”
        writeOpCodeShortOperand(cu->enclosingUnit, OPCODE_CREATE_CLOSURE, index);

        // 为vm在创建闭包时判断引用的是局部变量还是upvalue
        // 为先的每个upvalue生成参数
        index = 0;
        while (index < cu->fn->upvalueNum) {
            writeByte(cu->enclosingUnit, cu->upvalues[index].isEnclosingLocalVar ? 1 : 0);
            writeByte(cu->enclosingUnit, cu->upvalues[index].index);
            index ++;
        }
    }

    // 下调本编译单元，使当前编译单元指向外层编译单元
    cu->curParser->curCompileUnit = cu->enclosingUnit;
    return cu->fn;
}

/**
 * 生成getter或一般method调用指令
 * @param cu
 * @param sign
 * @param opCode
 */
static void emitGetterMethodCall(CompileUnit *cu, Signature *sign, OpCode opCode) {
    Signature newSign;
    newSign.type = SIGN_GETTER; // 默认为getter，假设下面的两个if不执行
    newSign.name = sign->name;
    newSign.length = sign->length;
    newSign.argNum = 0;

    // 如果是method，有可能有参数列表，在生成调用方法的指令必须把参数入栈，否则运输方法时除了
    // 会获取到错误的参数（即栈中已有数据）外，g还会在从方法返回时，错误的回收参数空间而导致栈失衡

    // 下面调用的processArgList是把实参入栈，供方法使用
    if (matchToken(cu->curParser, TOKEN_LEFT_PAREN)) { // 判断后面是否有(
        newSign.type = SIGN_METHOD;

        // 若后面不是)，说明有参数列表
        if (!matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            processArgList(cu, &newSign);
            consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' afterargument list!");
        }
    }

    // method来说可能还传入了块参数
    if (matchToken(cu->curParser, TOKEN_LEFT_BRACE)) {
        newSign.argNum ++;
        // 进入本if块时，上面的if块未必执行过
        // 此时newSign.type也许还是GETTER，下面要将其设置为METHOD
        newSign.type = SIGN_METHOD;
        CompileUnit fnCU;
        initCompileUint(cu->curParser, &fnCU, cu, false);

        Signature tmpFnSign = {SIGN_METHOD, "", 0, 0}; // 临时用于编译函数
        if (matchToken(cu->curParser, TOKEN_BIT_OR)) {  // 若块参数也有参数
            processParaList(&fnCU, &tmpFnSign); // 将形参声明为函数的局部变量
            consumeCurToken(cu->curParser, TOKEN_BIT_OR, "expect '|' after argument list!");
        }
        fnCU.fn->argNum = tmpFnSign.argNum;

        // 编译函数体，将指令流写进该函数自己的指令单元fnCU
        compileBody(&fnCU, false);
#if DEBUG
        //
        char fnName[MAX_SIGN_LEN + 10] = {'\0'};
        uint32_t len = sign2String(&newSign, fnName);
        memmove(fnName + len, "block arg", 10);
        endCompileUnit(&fnCU, fnName, len + 10);
#else
        endCompileUnit(&fnCU);
#endif
    }
    // 如果是在构造函数中调用了super则会指向到此，构造函数中调用的方法只能是super
    if (sign->type == SIGN_CONSTRUCT) {
        if (newSign.type != SIGN_METHOD) {
            COMPILE_ERROR(cu->curParser, "the form of supercall is super() or super(arguments)");
        }
        newSign.type = SIGN_CONSTRUCT;
    }

    // 根据签名生成调用指令，如果上面的三个if都未能执行，此处就是getter调用
    emitCallBySignature(cu, &newSign, opCode);
}

/**
 * 生成方法调用指令，包括getter和setter
 * @param cu
 * @param name
 * @param length
 * @param opCode
 * @param canAssign
 */
static void emitMethodCall(CompileUnit *cu, const char *name,
                           uint32_t length, OpCode opCode, bool canAssign) {
    Signature sign;
    sign.type = SIGN_GETTER;
    sign.name = name;
    sign.length = length;

    // 若是setter则生成调用setter的指令
    if (matchToken(cu->curParser, TOKEN_ASSIGN) && canAssign) {
        sign.type = SIGN_SETTER;
        sign.argNum = 1;  // setter只接受一个参数

        // 载入实参，为先方法调用传参
        expression(cu, BP_LOWEST);

        emitCallBySignature(cu, &sign, opCode);
    }
    else {
        emitGetterMethodCall(cu, &sign, opCode);
    }
}

/**
 * 小写字符开头便是局部变量
 * @param name
 * @return
 */
static bool isLocalName(const char *name) {
    return (name[0] >= 'a' && name[0] <= 'z');
}

/**
 * 标识符.nud():变量或方法名
 * @param cu
 * @param canAssign
 */
static void id(CompileUnit *cu, bool canAssign) {
    // 备份变量名
    Token name = cu->curParser->preToken;
    ClassBookKeep *classBK = getEnclosingClassBK(cu);

    // 标识符可以是任意字符，按照此顺序处理
    // 函数调用 局部变量和upvalue 实例域 静态域 类getter方法调用 模块变量

    // 处理函数调用
    if (cu->enclosingUnit == NULL && matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
        char id[MAX_ID_LEN] = {'\0'};

        // 函数名加上Fn前缀作为模块变量名
        memmove(id, "Fn ", 3);
        memmove(id + 3, name.start, name.length);

        Variable var;
        var.scopeType = VAR_SCOPE_MODULE;
        var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, id, strlen(id));
        if (var.index == -1) {
            memmove(id, name.start, name.length);
            id[name.length] = '\0';
            COMPILE_ERROR(cu->curParser, "Undefined function: '%s'!", id);
        }

        // 把模块变量即函数闭包加载到栈
        emitLoadVariable(cu, var);

        Signature sign;
        // 函数调用的形式和method类似
        // 只不过method有一个可选的块参数
        sign.type = SIGN_METHOD;

        // 把函数调用编译为'闭包.call'的形式，故name为call
        sign.name = "call";
        sign.length = 4;
        sign.argNum = 0;

        // 若后面不是)，说明有参数列表
        if (!matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            // 压入实参
            processArgList(cu, &sign);
            consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after argument list!");
        }
        // 生成调用指令以调用函数
        emitCallBySignature(cu, &sign, OPCODE_CALL0);
    }
    else {  // 否则按照各种变量来处理

        // 按照局部变量和upvalue来处理
        Variable var = getVarFromLocalOrUpvalue(cu, name.start, name.length);
        if (var.index != -1) {
            emitLoadOrStoreVariable(cu, canAssign, var);
            return;
        }

        // 按照实例域来处理
        if (classBK != NULL) {
            int fieldIndex = getIndexFromSymbolTable(&classBK->fields, name.start, name.length);
            if (fieldIndex != -1) {
                bool isRead = true;
                if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    isRead = false;
                    expression(cu, BP_LOWEST);
                }

                // 如果当前正在编译类方法，则直接在该实例对象中加载field
                if (cu->enclosingUnit != NULL) {
                    writeOpCodeByteOperand(cu, isRead ? OPCODE_LOAD_THIS_FIELD : OPCODE_STORE_FIELD, fieldIndex);
                } else {
                    emitLoadThis(cu);
                    writeOpCodeByteOperand(cu, isRead ? OPCODE_LOAD_FIELD : OPCODE_STORE_FIELD, fieldIndex);
                }
                return;
            }
        }

        // 按照静态域查找
        if (classBK != NULL) {
            char *staticFieldId = ALLOCATE_ARRAY(cu->curParser->vm, char, MAX_ID_LEN);
            memset(staticFieldId, 0, MAX_ID_LEN);
            uint32_t staticFieldIdLen;
            char *clsName = classBK->name->value.start;
            uint32_t clsLen = classBK->name->value.length;

            // 各类中静态域的名称以"Cls类名 静态域名"来命名
            memmove(staticFieldId, "Cls", 3);
            memmove(staticFieldId + 3, clsName, clsLen);
            memmove(staticFieldId + 3 + clsLen, " ", 1);
            const char *tkName = name.start;
            uint32_t tkLen = name.length;
            memmove(staticFieldId + 4 + clsLen, tkName, tkLen);
            staticFieldIdLen = strlen(staticFieldId);
            var = getVarFromLocalOrUpvalue(cu, staticFieldId, staticFieldIdLen);

            DEALLOCATE_ARRAY(cu->curParser->vm, staticFieldId, MAX_ID_LEN);

            if (var.index != -1) {
                emitLoadOrStoreVariable(cu, canAssign, var);
                return;
            }
        }

        // 如果以上未找到同名变量，有可能是该标识符是同类中的其他方法调用
        // 方法规定以小写字符开头
        if (classBK != NULL && isLocalName(name.start)) {
            emitLoadThis(cu); // 确保args[0]是this对象，以便查找到方法
            // 因为类可能尚未编译完，未统计所有方法
            // 故此时无法判断方法是否为未定义，留代运行时检测
            emitMethodCall(cu, name.start, name.length, OPCODE_CALL0, canAssign);
            return;
        }

        // 按照模块变量处理
        var.scopeType = VAR_SCOPE_MODULE;
        var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, name.start, name.length);
        if (var.index == -1) {
            // 模块变量属于模块作用域,若当前引用处之.前未定义该模块变量,
            //说不定在后面有其定义,因此暂时先声明它,待模块统计完后再检查

            // 用关键字fun定义的函数是以前缀Fn后接函数名作为模块变量
            // 下面加上Fn前缀按照函数名重新查找
            char fnName[MAX_SIGN_LEN + 4] = {'\0'};
            memmove(fnName, "Fn ", 3);
            memmove(fnName + 3, name.start, name.length);
            var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, fnName, strlen(fnName));

            // 若不是函数名,那可能是该模块变量定义在引用处的后面，
            // 先将行号作为该变量值去声明
            if (var.index == -1) {
                var.index = declareModuleVar(cu->curParser->vm,
                                             cu->curParser->curModule, name.start, name.length,
                                             NUM_TO_VALUE(cu->curParser->curToken.lineNo));
            }
        }
        emitLoadOrStoreVariable(cu, canAssign, var);
    }
}

//SymbolBindRule Rules[] = {
//        UNUSED_RULE,
//        PREFIX_SYMBOL(literal),
//        PREFIX_SYMBOL(literal),
//        {NULL, BP_NONE, id, NULL, idMethodSignature},
//};

/***********************************************************************************************
 ********************************* 编译内嵌表达式 ************************************************
 ***********************************************************************************************/

/**
 * 生成加载类的指令
 * @param cu
 * @param name
 */
static void emitLoadModuleVar(CompileUnit *cu, const char *name) {
    int index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, name, strlen(name));

    ASSERT(index != -1, "symbol should have been defined");
    writeOpCodeShortOperand(cu, OPCODE_LOAD_MODULE_VAR, index);
}

/**
 * 内嵌表达式.nud()
 * @param cu
 * @param canAssign
 */
static void stringInterpolation(CompileUnit *cu, bool canAssign UNUSED) {


    emitLoadModuleVar(cu, "List");
    emitCall(cu, 0, "new()", 5);

    do {
        literal(cu, false);
        emitCall(cu, 1, "addCore_(_)", 11);

        expression(cu, BP_LOWEST);
        // 将结果添加到list
        emitCall(cu, 1, "addCore_(_)", 11); // 生成addCore_前缀的函数
    } while (matchToken(cu->curParser, TOKEN_INTERPOLATION));

    consumeCurToken(cu->curParser, TOKEN_STRING, "expect string at the end of interpolatation!");

    literal(cu, false);

    emitCall(cu, 1, "addCore_(_)", 11);

    emitCall(cu, 0, "join()", 6);
}

/***********************************************************************************************
 ********************************* 编译bool及null ************************************************
 ***********************************************************************************************/

/**
 * 编译bool
 * @param cu
 * @param canAssign
 */
static void boolean(CompileUnit *cu, bool canAssign UNUSED) {
    OpCode opCode = cu->curParser->preToken.type == TOKEN_TRUE ? OPCODE_PUSH_TRUE : OPCODE_PUSH_FALSE;

    writeOpCode(cu, opCode);
}

/**
 * 生成OPCODE_PUSH_NULL 指令
 * @param cu
 * @param canAssign
 */
static void null(CompileUnit *cu, bool canAssign UNUSED) {
    writeOpCode(cu, OPCODE_PUSH_NULL);
}

/***********************************************************************************************
 ********************************* this 继承 基类 ************************************************
 ***********************************************************************************************/

/**
 * “this”.nud()
 * @param cu
 * @param canAssign
 */
static void this(CompileUnit *cu, bool canAssign UNUSED) {
    if (getEnclosingClassBK(cu) == NULL) {
        COMPILE_ERROR(cu->curParser, "this must be inside a class method!");
    }
    emitLoadThis(cu);
}

/**
 * “super”.nud()
 * @param cu
 * @param canAssign
 */
static void super(CompileUnit *cu, bool canAssign) {
    ClassBookKeep *enclosingClassBK = getEnclosingClassBK(cu);
    if (enclosingClassBK == NULL) {
        COMPILE_ERROR(cu->curParser, "can't invoke super outside a class method");
    }

    emitLoadThis(cu);

    if (matchToken(cu->curParser, TOKEN_DOT)) {
        consumeCurToken(cu->curParser, TOKEN_ID, "expect name after '.'!");
        emitMethodCall(cu, cu->curParser->preToken.start, cu->curParser->preToken.length, OPCODE_SUPER0, canAssign);
    }
    else {
        emitGetterMethodCall(cu, enclosingClassBK->signature, OPCODE_SUPER0);
    }
}

/***********************************************************************************************
 ********************************* 小括号 中括号 list列表字面量 ************************************************
 ***********************************************************************************************/

/**
 * 编译圆括号
 * @param cu
 * @param canAssign
 */
static void parentheses(CompileUnit *cu, bool canAssign UNUSED) {
    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after expression!");
}

/**
 * '['.nud()处理用字面量形式定义的list列表
 * @param cu
 * @param canAssign
 */
static void listLiteral(CompileUnit *cu, bool canAssign UNUSED) {
    // 进入函数后，curToken是[右边的符号

    // 先创建list对象
    emitLoadModuleVar(cu, "list");
    emitCall(cu, 0, "new()", 5);

    do {
        // 支持字面量形式定义的空列表
        if (PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_BRACKET) {
            break;
        }
        expression(cu, BP_LOWEST);
        emitCall(cu, 1, "addCore_(_)", 11);
    } while (cu->curParser, TOKEN_RIGHT_BRACKET, "expect ')' after list element!");
}

/**
 * '['.led() 用于索引list元素，如list[x]
 * @param cu
 * @param canAssign
 */
static void subscript(CompileUnit *cu, bool canAssign) {

    // 确保[之间不空
    if (matchToken(cu->curParser, TOKEN_RIGHT_BRACKET)) {
        COMPILE_ERROR(cu->curParser, "need argument in the '[]'!");
    }

    // 默认是[_]， 即subscript getter
    Signature sign = {SIGN_SUBSCRIPT, "", 0, 0};

    // 读取参数并把参数加载到栈，统计参数个数
    processArgList(cu, &sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACKET, "expect ']' after argument list!");

    // 若是[_]=(_)，即subscript setter
    if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
        sign.type = SIGN_SUBSCRIPT_SETTER;

        if (++sign.argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }

        // 获取=右边的表达式
        expression(cu, BP_LOWEST);
    }

    emitCallBySignature(cu, &sign, OPCODE_CALL0);
}

/**
 * 为下标操作符[编译签名
 * @param cu
 * @param sign
 */
static void subscriptMethodSignature(CompileUnit *cu, Signature *sign) {
    sign->type = SIGN_SUBSCRIPT;
    sign->length = 0;
    processParaList(cu, sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACKET, "expect ']' after index list!");
    trySetter(cu, sign); // 判断]后面是否接=为setter
}

/***********************************************************************************************
 ********************************* 方法调用和map ************************************************
 ***********************************************************************************************/

/**
 * '.'led() 编译方法调用，所有调用的入口
 * @param cu
 * @param sign
 */
static void callEntry(CompileUnit *cu, bool canAssign) {
    consumeCurToken(cu->curParser, TOKEN_ID, "expect method name after '.'!");

    // 生成方法调用指令
    emitMethodCall(cu, cu->curParser->preToken.start, cu->curParser->preToken.length, OPCODE_CALL0, canAssign);
}

/**
 * map对象字面量
 * @param cu
 * @param canAssign
 */
static void mapLiteral(CompileUnit *cu, bool canAssign UNUSED) {

    emitLoadModuleVar(cu, "Map");

    emitCall(cu, 0, "new()", 5);

    do {
        if (PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_BRACE) {
            break;
        }

        // 读取key
        expression(cu, BP_UNARY);

        consumeCurToken(cu->curParser, TOKEN_COLON, "expect ':' after key!");

        expression(cu, BP_LOWEST);

        emitCall(cu, 2, "addCore_(_,_)", 13);
    } while (matchToken(cu->curParser, TOKEN_COMMA));

    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACE, "map literal should end with\')\'!");
}

/***********************************************************************************************
 ********************************* 数学运算符 ************************************************
 ***********************************************************************************************/

/**
 * 用占位符作为参数设置指令
 * @param cu
 * @param opCode
 * @return
 */
static uint32_t emitInstrWithPlaceholder(CompileUnit *cu, OpCode opCode) {
    writeOpCode(cu, opCode);
    writeByte(cu, 0xff); // 先写入高位的0xff

    // 再写入低位的0xff后，减1返回高位地址，此地址将来用于回填
    return writeByte(cu, 0xff) - 1;
}

/**
 * 用跳转到当前字节码结束地址的偏移量去替换占位符参数0xffff
 * absIndex是指指令流中绝对索引
 * @param cu
 * @param absIndex
 */
static void patchPlaceholder(CompileUnit *cu, uint32_t absIndex) {
    uint32_t offset = cu->fn->instrStream.count - absIndex - 2;

    cu->fn->instrStream.datas[absIndex] = (offset >> 8) & 0xff;

    cu->fn->instrStream.datas[absIndex + 1] = offset & 0xff;
}

/**
 * ‘||’.led()
 * @param cu
 * @param canAssign
 */
static void logicOr(CompileUnit *cu, bool canAssign UNUSED) {
    uint32_t placeholderIndex = emitInstrWithPlaceholder(cu, OPCODE_OR);

    expression(cu, BP_LOGIC_OR);

    patchPlaceholder(cu, placeholderIndex);
}

/**
 * ‘&&’.led()
 * @param cu
 * @param canAssign
 */
static void logicAnd(CompileUnit *cu, bool canAssign UNUSED) {
    uint32_t placeholderIndex = emitInstrWithPlaceholder(cu, OPCODE_AND);

    expression(cu, BP_LOGIC_OR);

    patchPlaceholder(cu, placeholderIndex);
}

/**
 * “?:”.led()
 * @param cu
 * @param canAssign
 */
static void condition(CompileUnit *cu, bool canAssign UNUSED) {

    ///若condition为false, if跳转到false分支的起始地址,为该地址设置占位符
    uint32_t falseBranchStart = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 编译true分支
    expression(cu, BP_LOWEST);

    consumeCurToken(cu->curParser, TOKEN_COLON, "expect ':' after true branch!");

    // 执行完true分支后需要跳过false分支
    uint32_t falseBranchEnd = emitInstrWithPlaceholder(cu, OPCODE_JUMP);

    //编译true分支已经结束.此时知道了true分支的结束地址,
    // 编译false分支之前须先回填falseBranchStart
    patchPlaceholder(cu, falseBranchStart);

    // 编译false分支
    expression(cu, BP_LOWEST);

    // 知道了false分支的结束地址,回填falseBranchEnd
    patchPlaceholder(cu, falseBranchEnd);
}

/**
 * 编译变量定义
 * @param cu
 * @param isStatic
 */
static void compileDefinition(CompileUnit *cu, bool isStatic) {
    consumeCurToken(cu->curParser, TOKEN_ID, "missing variable name!");
    Token name = cu->curParser->preToken;

    // 只支持单个变量
    if (cu->curParser->curToken.type == TOKEN_COMMA) {
        COMPILE_ERROR(cu->curParser, "'var' only support declaring a variable.");
    }

    // 先判断是否是类中的域定义，确保cu是模块cu
    if (cu->enclosingUnit == NULL && cu->enclosingClassBK != NULL) {
        if (isStatic) { // 静态域
            char *staticFieldId = ALLOCATE_ARRAY(cu->curParser->vm, char, MAX_ID_LEN);
            memset(staticFieldId, 0, MAX_ID_LEN);
            uint32_t staticFieldIdLen;
            char *clsName = cu->enclosingClassBK->name->value.start;
            uint32_t clsLen = cu->enclosingClassBK->name->value.length;

            // 用前缀'Cls+类名+变量名'作为静态域在模块编译单元中的局部变量
            memmove(staticFieldId, "Cls", 3);
            memmove(staticFieldId + 3, clsName, clsLen);
            memmove(staticFieldId + 3 + clsLen, " ", 1);
            const char *tkName = name.start;
            uint32_t tkLen = name.length;
            memmove(staticFieldId + 4 + clsLen, tkName, tkLen);

            staticFieldIdLen = strlen(staticFieldId);

            if (findLocal(cu, staticFieldId, staticFieldIdLen) == -1) {
                int index = declareLocalVar(cu, staticFieldId, staticFieldIdLen);
                writeOpCode(cu, OPCODE_PUSH_NULL);
                ASSERT(cu->scopeDepth == 0, "should in class scope!");
                defineVariable(cu, index);

                // 静态域可初始化
                Variable var = findVariable(cu, staticFieldId, staticFieldIdLen);
                if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    expression(cu, BP_LOWEST);
                    emitStoreVariable(cu, var);
                }
            }
            else {
                COMPILE_ERROR(cu->curParser, "static field '%s' redefinition!", strchr(staticFieldId, ' ') + 1);
            }
        }
        else {  // 定义实例域
            ClassBookKeep *classBK = getEnclosingClassBK(cu);

            int fieldIndex = getIndexFromSymbolTable(&classBK->fields, name.start, name.length);

            if (fieldIndex == -1) {
                fieldIndex = addSymbol(cu->curParser->vm, &classBK->fields, name.start, name.length);
            }
            else {
                if (fieldIndex > MAX_FILED_NUM) {
                    COMPILE_ERROR(cu->curParser, "the max number of instance field is %d!", MAX_FILED_NUM);
                }
                else {
                    char id[MAX_ID_LEN] = {'\0'};
                    memcpy(id, name.start, name.length);
                    COMPILE_ERROR(cu->curParser, "instance field '%s' redefinition!", id);
                }

                if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    COMPILE_ERROR(cu->curParser, "instance field isn't allowed initialization!");
                }
            }
        }
        return;
    }

    // 若不是类中的域定义，就按照一般的变量定义
    if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
        // 若在定义时赋值就解析表达式，结果会留到栈顶
        expression(cu, BP_LOWEST);
    }
    else {
        // 否则就初始化为NULL， 即在栈顶压入NULL
        // 也是为了与上面显式初始化保持相同栈结构
        writeOpCode(cu, OPCODE_PUSH_NULL);
    }

    uint32_t index = declareVariable(cu, name.start, name.length);
    defineVariable(cu, index);
}


/**
 * 编译if语句
 * @param cu
 */
static void compileIfStatement(CompileUnit *cu) {
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "missing '(' after if!");
    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "missing ')' before '{' in if!");

    // 若条件为假，if跳转到FALSE分支的起始地址，现为该地址设置占位符
    uint32_t falseBranchStart = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 编译then分支
    // 代码块前后的{和}由compileStatement负责读取
    compileStatement(cu);

    // 如果有else分支
    if (matchToken(cu->curParser, TOKEN_ELSE)) {
        // 添加跳过else分支的跳转指令
        uint32_t falseBranchEnd = emitInstrWithPlaceholder(cu, OPCODE_JUMP);

        // 进入else分支编译之前，先回填falseBranchStart
        patchPlaceholder(cu, falseBranchStart);

        // 编译else分支
        compileStatement(cu);

        // 此时知道了FALSE块的结束地址，回填
        patchPlaceholder(cu, falseBranchEnd);
    }
    else {  // 若不包括else块
        // 此时falseBranchStart就是条件为假时,需要跳过整个true分支的目标地址
        patchPlaceholder(cu, falseBranchStart);
    }
}

/**
 * 编译语句
 * @param cu
 */
static void compileStatement(CompileUnit *cu) {
    if (matchToken(cu->curParser, TOKEN_IF)) {
        compileIfStatement(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_WHILE)) {
        compileWhileStatment(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_RETURN)) {
        compileReturn(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_BREAK)) {
        compileBreak(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_CONTINUE)) {
        compileContinue(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_FOR)) {
        compileForStatment(cu);
    }
    else if (matchToken(cu->curParser, TOKEN_LEFT_BRACE)) {
        // 代码块有单独的作用域
        enterScope(cu);
        compileBlock(cu);
        leaveScope(cu);
    }
    else {
        // 若不是以上的语法结构则是单一表达式
        expression(cu, BP_LOWEST);

        // 表达式的结果不重要，弹出栈顶结果
        writeOpCode(cu, OPCODE_POP);
    }
}

/**
 * 开始循环，进入循环体相关设置
 * @param cu
 * @param loop
 */
static void enterLoopSetting(CompileUnit *cu, Loop *loop) {
    // cu->fn->instrStream.count是下一条指令的地址，所以-1
    loop->condStartIndex = cu->fn->instrStream.count - 1;

    loop->scopeDepth = cu->scopeDepth;

    // 在当前循环层中嵌套新的循环层，当前层成为内嵌层的外层
    loop->enclosingLoop = cu->curLoop;

    // 使cu->curLoop指向新的内层
    cu->curLoop = loop;
}

/**
 * 编译循环体
 * @param cu
 */
static void compileLoopBody(CompileUnit *cu) {
    // 使循环体起始地址指向下一条指令地址
    cu->curLoop->bodyStartIndex = cu->fn->instrStream.count;

    compileStatement(cu); // 编译循环体
}

/**
 * 获得ip所指向的操作码的操作数占用的字节数
 * @param instrStream 
 * @param constants 
 * @param ip 
 * @return 
 */
uint32_t getBytesOfOperands(Byte *instrStream, Value *constants, int ip) {
    switch ((OpCode)instrStream[ip]) {
        case OPCODE_CONSTRUCT:
        case OPCODE_END:
        case OPCODE_CLOSE_UPVALUE:
        case OPCODE_PUSH_NULL:
        case OPCODE_PUSH_FALSE:
        case OPCODE_PUSH_TRUE:
        case OPCODE_POP:
            return 0;

        case OPCODE_CREATE_CLASS:
        case OPCODE_LOAD_THIS_FIELD:
        case OPCODE_STORE_THIS_FIELD:
        case OPCODE_LOAD_FIELD:
        case OPCODE_STORE_FIELD:
        case OPCODE_LOAD_LOCAL_VAR:
        case OPCODE_STORE_LOCAL_VAR:
        case OPCODE_LOAD_UPVALUE:
        case OPCODE_STORE_UPVALUE:
            return 1;

        case OPCODE_CALL0:
        case OPCODE_CALL1:
        case OPCODE_CALL2:
        case OPCODE_CALL3:
        case OPCODE_CALL4:
        case OPCODE_CALL5:
        case OPCODE_CALL6:
        case OPCODE_CALL7:
        case OPCODE_CALL8:
        case OPCODE_CALL9:
        case OPCODE_CALL10:
        case OPCODE_CALL11:
        case OPCODE_CALL12:
        case OPCODE_CALL13:
        case OPCODE_CALL14:
        case OPCODE_CALL15:
        case OPCODE_CALL16:
        case OPCODE_LOAD_CONSTANT:
        case OPCODE_LOAD_MODULE_VAR:
        case OPCODE_STORE_MODULE_VAR:
        case OPCODE_LOOP:
        case OPCODE_JUMP:
        case OPCODE_JUMP_IF_FALSE:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_INSTANCE_METHOD:
        case OPCODE_STATIC_METHOD:
            return 2;

        case OPCODE_SUPER0:
        case OPCODE_SUPER1:
        case OPCODE_SUPER2:
        case OPCODE_SUPER3:
        case OPCODE_SUPER4:
        case OPCODE_SUPER5:
        case OPCODE_SUPER6:
        case OPCODE_SUPER7:
        case OPCODE_SUPER8:
        case OPCODE_SUPER9:
        case OPCODE_SUPER10:
        case OPCODE_SUPER11:
        case OPCODE_SUPER12:
        case OPCODE_SUPER13:
        case OPCODE_SUPER14:
        case OPCODE_SUPER15:
        case OPCODE_SUPER16:
            return 4;

        case OPCODE_CREATE_CLOSURE: {
            uint32_t fnIdx = (instrStream[ip + 1] << 8 | instrStream[ip + 2]);

            return 2 + (VALUE_TO_OBJFN(constants[fnIdx])->upvalueNum * 2);
        }
        default:
            NOT_REACHED();

    }
}

/**
 * 离开循环体时的相关设置
 * @param cu
 */
static void leaveLoopPatch(CompileUnit *cu) {
    // 获取往回跳转的偏移量，偏移量都为正数
    int loopBackOffset = cu->fn->instrStream.count - cu->curLoop->condStartIndex + 2;

    // 生成向回跳转从指令
    writeOpCodeShortOperand(cu, OPCODE_LOOP, loopBackOffset);

    // 回填循环体的结束地址
    patchPlaceholder(cu, cu->curLoop->exitIndex);

    // 下面在循环体中回填break的占位符
    // 循环体开始地址
    uint32_t idx = cu->curLoop->bodyStartIndex;
    // 循环体结束地址
    uint32_t loopEndindex = cu->fn->instrStream.count;
    while (idx < loopEndindex) {
        if (OPCODE_END == cu->fn->instrStream.datas[idx]) {
            cu->fn->instrStream.datas[idx] = OPCODE_JUMP;

            patchPlaceholder(cu, idx + 1);

            idx += 3;
        }
        else {
            idx += 1 + getBytesOfOperands(cu->fn->instrStream.datas, cu->fn->constants.datas, idx);
        }
    }
    // 退出当前循环体，即恢复cu->curLoop为当前循环层的外层循环
    cu->curLoop = cu->curLoop->enclosingLoop;
}

/**
 * 编译while循环
 * @param cu
 */
static void compileWhileStatment(CompileUnit *cu) {
    Loop loop;

    enterLoopSetting(cu, &loop);
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' before condition!");

    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect '(' after condition!");

    loop.exitIndex = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    compileLoopBody(cu);

    // 设置循环体结束等等
    leaveLoopPatch(cu);
}

/**
 * 丢掉作用域scopeDepth之内的局部变量
 * @param cu
 * @param scopeDepth
 * @return
 */
static uint32_t discardLocalVar(CompileUnit *cu, int scopeDepth) {
    ASSERT(cu->scopeDepth > -1, "upmost scope can't exit!");
    int localIdx = cu->localVarNum - 1;

    // 变量作用域大于scopeDepth的为其内嵌作用域中的变量，
    // 跳出scodeDepth时内层也没用了,要回收其局部变量
    while (localIdx >= 0 && cu->localVars[localIdx].scopeDepth >= scopeDepth) {
        if (cu->localVars[localIdx].isUpvalue) {
            writeByte(cu, OPCODE_CLOSE_UPVALUE);
        }
        else {
            writeByte(cu, OPCODE_POP);
        }
        localIdx --;
    }

    // 返回丢掉的局部变量个数
    return cu->localVarNum - 1 - localIdx;
}

/**
 * 编译return
 * @param cu
 */
inline static void compileReturn(CompileUnit *cu) {
    if (PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_BRACE) {  // 空返回值
        writeOpCode(cu, OPCODE_PUSH_NULL);
    }
    else {
        expression(cu, BP_LOWEST);
    }

    writeOpCode(cu, OPCODE_RETURN); // 将栈顶的值返回

}

/**
 * 编译break
 * @param cu
 */
inline static void compileBreak(CompileUnit *cu) {
    if (cu->curLoop == NULL) {
        COMPILE_ERROR(cu->curParser, "break should be used inside a loop!");
    }

    discardLocalVar(cu, cu->curLoop->scopeDepth + 1);

    emitInstrWithPlaceholder(cu, OPCODE_END);
}

/**
 * 编译continue
 * @param cu
 */
inline static void compileContinue(CompileUnit *cu) {
    if (cu->curLoop == NULL) {
        COMPILE_ERROR(cu->curParser, "continue should be used inside a loop!");
    }

    // 回收本作用域中局部变量在栈中的空间，+1 是指循环体(包括循环条件)的作用域
    // 不能在cu->localVars数组中去掉,
    // 否则在continue语句后面若引用了前面的变量则提示找不到
    discardLocalVar(cu, cu->curLoop->scopeDepth + 1);

    int loopBackOffset = cu->fn->instrStream.count - cu->curLoop->condStartIndex + 2;

    // 生成向回跳转的CODE_ LOOP指令，即使ip -= loopBackOffset
    writeOpCodeByteOperand(cu, OPCODE_LOOP, loopBackOffset);
}

/**
 * 进入新作用域
 * @param cu
 */
static void enterScope(CompileUnit *cu) {
    // 进入内嵌作用域
    cu->scopeDepth ++;
}

/**
 * 退出作用域
 * @param cu
 */
static void leaveScope(CompileUnit *cu) {
    // 对于非模块编译单元，丢弃局部变量
    if (cu->enclosingUnit != NULL) {
        // 出作用域后丢弃本作用域以内的局部变量
        uint32_t discardNum = discardLocalVar(cu, cu->scopeDepth);
        cu->localVarNum -= discardNum;
        cu->stackSlotNum -= discardNum;
    }

    // 回到上一层作用域
    cu->scopeDepth --;
}

/**
 * 编译for
 * @param cu
 */
static void compileForStatment(CompileUnit *cu) {
    // 为局部变量seq和iter创建作用域
    enterScope(cu);

    // 读取循环变量的名字
    consumeCurToken(cu->curParser, TOKEN_ID, "expect variable after for!");

    const char *loopVarName = cu->curParser->preToken.start;
    uint32_t loopVarLen = cu->curParser->preToken.length;

    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' before sequence!");

    // 编译迭代序列
    expression(cu, BP_LOWEST);

    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect '(' after sequence!");

    // 申请一个局部变量seq来存储序列对象
    // 其值就是上面expression存储在栈中的结果
    uint32_t seqSlot = addLocalVar(cu, "seq ", 4);

    writeOpCode(cu, OPCODE_PUSH_NULL);

    // 分配及初始化iter,其值就是.上面加载到栈中的NULL
    uint32_t iterSlot = addLocalVar(cu, "iter ", 5);

    Loop loop;
    enterLoopSetting(cu, &loop);

    // 为调用seq. iterate (iter)做准备
    // 先压入序列对象seq,即seq.iterate(iter)中的seq
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, seqSlot);
    // 再压入参数iter,即seq. iterate (iter)中的iter
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, iterSlot);
    // 调用seq.iterate (iter)
    emitCall(cu, 1, "iterate(_)", 10);

    // sea. iterate (iter)把结果(下一个迭代器)存储到
    // args[0] (即栈顶) ,现在将其同步到变量iter
    writeOpCodeByteOperand(cu, OPCODE_STORE_LOCAL_VAR, iterSlot);

    // 如果条件失败则跳出循环体,目前不知道循环体的结束地址，
    // 先写入占位符
    loop.exitIndex = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 为调用seq. iterate (iter)做准备
    // 先压入序列对象seq,即seq.iterate(iter)中的seq
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, seqSlot);
    // 再压入参数iter,即seq. iterate (iter)中的iter
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, iterSlot);
    // 调用seq.iterate (iter)
    emitCall(cu, 1, "iteratorValue(_)", 16);

    // 为循环变量i创建作用域
    enterScope(cu);

    // seq. iteratorValue (iter)已经把结果存储到栈项，
    // 添加循环变量为局部变量,其值在栈顶
    addLocalVar(cu, loopVarName, loopVarLen);

    // 编译循环体
    compileLoopBody(cu);

    leaveScope(cu); // 离开循环变量i的作用域

    leaveLoopPatch(cu);

    leaveScope(cu); // 离开变量seq和iter的作用域

}

/**
 * 生成存储模块变量的指令
 * @param cu
 * @param index
 */
static void emitStoreModuleVar(CompileUnit *cu, int index) {
    // 把栈顶数据存储到moduleVarValue[index]
    writeOpCodeShortOperand(cu, OPCODE_STORE_MODULE_VAR, index);
    writeOpCode(cu, OPCODE_POP); // 弹出栈顶数据
}

/**
 * 声明方法
 * @param cu
 * @param signStr
 * @param length
 * @return
 */
static int declareMethod(CompileUnit *cu, char *signStr, uint32_t length) {
    // 确保方法被录入到vm->allMe thodNames
    int index = ensureSymbolExist(cu->curParser->vm, &cu->curParser->vm->allMethodNames, signStr, length);

    // 下面判断方法是否已定义，即排除重定义
    IntBuffer *methods = cu->enclosingClassBK->inStatic ?
            &cu->enclosingClassBK->staticMethods:
            &cu->enclosingClassBK->instantMethods;
    uint32_t idx = 0;
    while (idx < methods->count) {
        if (methods->datas[idx] == index) {
            COMPILE_ERROR(cu->curParser, 
            "repeat define method %s in class %s!", 
            signStr, cu->enclosingClassBK->name->value.start);
        }
        idx ++;
    }

    IntBufferAdd(cu->curParser->vm, methods, index);
    return index;
}

/**
 * 将方法methodIndex指代的方法装入classVar指代的class . methods中
 * @param cu
 * @param classVar
 * @param isStatic
 * @param methodIndex
 */
static void defineMethod(CompileUnit *cu, Variable classVar, bool isStatic, int methodIndex) {
    // 待绑定的方法在调用本函数之前已经放到栈项了

    // 将方法所属的类加载到栈项
    emitLoadVariable(cu, classVar);

    // 运行时绑定
    OpCode opCode = isStatic ?
    OPCODE_STATIC_METHOD: OPCODE_INSTANCE_METHOD;
    writeOpCodeShortOperand(cu, opCode, methodIndex);
}

/**
 * 分两步创建实例, const ructorIndex是构造函数的索引
 * @param cu
 */
static void emitCreateInstance(CompileUnit *cu, Signature *sign, uint32_t constructorIndex) {
   CompileUnit methodCU;
    initCompileUint(cu->curParser, &methodCU, cu, true);

    // 生成OPCODE CONSTRUCT 指令,该指令生成新实例存储到stack [0]
    writeOpCode(&methodCU, OPCODE_CONSTRUCT);

    // 生成OPCODE CALLx 指令,该指令调用新实例的构造函数
    writeOpCodeShortOperand(&methodCU, (OpCode)(OPCODE_CALL0 + sign->argNum), constructorIndex);

    //生成return指令,将栈项中的实例返回
    writeOpCode(&methodCU, OPCODE_RETURN);

#if DEBUG
    endCompileUnit(&methodCU, "", 0);
#else
    endCompileUnit(&methodCU);
#endif
}

/**
 * 编译定义方法，isStatic表示是否在编译静态方法
 * @param cu
 * @param classVar
 * @param isStatic
 */
static void compileMethod(CompileUnit *cu, Variable classVar, bool isStatic) {
    cu->enclosingClassBK->inStatic = isStatic;
    methodSignatureFn methodSign = Rules[cu->curParser->curToken.type].methodSign;
    if (methodSign == NULL) {
        COMPILE_ERROR(cu->curParser, "method need signature function!");
    }
    Signature sign;
    sign.name = cu->curParser->curToken.start;
    sign.length = cu->curParser->curToken.length;
    sign.argNum = 0;

    cu->enclosingClassBK->signature = &sign;

    getNextToken(cu->curParser);

    // 为了将函数或方法自己的指令流和局部变量单独存储,
    // 每个函数或方法都有自己的CompileUnit
    CompileUnit methodCU;
    // 编译一个方法,因此形参isMethod为true
    initCompileUint(cu->curParser, &methodCU, cu, true);

    // 构造签名
    methodSign(&methodCU, &sign);
    consumeCurToken(cu->curParser, TOKEN_LEFT_BRACE, "expect '{' at the of method body.");

    if (cu->enclosingClassBK->inStatic && sign.type == SIGN_CONSTRUCT) {
        COMPILE_ERROR(cu->curParser, "constructor is not allowed to be static!");
    }

    char signatureString[MAX_SIGN_LEN] = {'\0'};
    uint32_t signLen = sign2String(&sign, signatureString);

    // 将方法声明
    uint32_t methodIndex = declareMethod(cu, signatureString, signLen);

    // 编译方法体指令流到方法自己的编译单元methodCU
    compileBody(&methodCU, sign.type == SIGN_CONSTRUCT);

#if DEBUG
    endCompileUnit(&methodCU, signatureString, signLen);
#else
    endCompileUnit(&methodCU);
#endif

    // 定义方法:将.上面创建的方法闭包绑定到类
    defineMethod(cu, classVar, cu->enclosingClassBK->inStatic, methodIndex);

    if (sign.type == SIGN_CONSTRUCT) {
        sign.type == SIGN_METHOD;
        char signatureString[MAX_SIGN_LEN] = {'\0'};
        uint32_t signLen = sign2String(&sign, signatureString);

        uint32_t constructoreIndex = ensureSymbolExist(cu->curParser->vm, &cu->curParser->vm->allMethodNames, signatureString, signLen);

        emitCreateInstance(cu, &sign, methodIndex);

        // 构造函数是静态方法，即类方法
        defineMethod(cu, classVar, true, constructoreIndex);
    }
}

/**
 * 编译类体
 * @param cu
 * @param classVar
 */
static void compileClassBody(CompileUnit *cu, Variable classVar) {
    if (matchToken(cu->curParser, TOKEN_STATIC)) {
        if (matchToken(cu->curParser, TOKEN_VAR)) {
            compileVarDeinition(cu, true);
        }
        else {
            compileMethod(cu, classVar, true);
        }
    }
    else if (matchToken(cu->curParser, TOKEN_VAR)) {  // 实例域
        compileVarDeinition(cu, false);
    }
    else {  // 类的方法
        compileMethod(cu, classVar, false);
    }
}

/**
 * 编译类定义
 * @param cu
 */
static void compileClassDefinition(CompileUnit *cu) {
    Variable classVar;
    if (cu->scopeDepth != -1) {
        COMPILE_ERROR(cu->curParser, "class definition must be in the module scope!");
    }

    classVar.scopeType = VAR_SCOPE_MODULE;
    consumeCurToken(cu->curParser, TOKEN_ID, "keyword class should follow by class name!");
    classVar.index = declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);

    // 生成类名，用于创建类
    ObjString *className = newObjString(cu->curParser->vm, cu->curParser->preToken.start, cu->curParser->preToken.length);

    // 生成加载类名的指令
    emitLoadConstant(cu, OBJ_TO_VALUE(className));
    if (matchToken(cu->curParser, TOKEN_LESS)) {  // 类继承
        expression(cu, BP_LOWEST); // 把父类名加载到栈顶
    }
    else {  // 默认加载object类为基类
        emitLoadModuleVar(cu, "object");
    }

    // 创建类需要知道域的个数,目前类未定义完,因此域的个数未知，
    // 因此先临时写为255,待类编译完成后再回填属性数
    int fieldNumIndex = writeOpCodeByteOperand(cu, OPCODE_CREATE_CLASS, 3); // todo

    // 虛拟机执行完OPCODE_ CREATE CLASS 后,栈顶留下了创建好的类，
    // 因此现在可以用该类为之前声明的类名className赋值
    if (cu->scopeDepth == -1) {
        emitStoreModuleVar(cu, classVar.index);
    }

    ClassBookKeep classBK;
    classBK.name = className;
    classBK.inStatic = false;
    StringBufferInit(&classBK.fields);
    IntBufferInit(&classBK.instantMethods);
    IntBufferInit(&classBK.staticMethods);

    // 此时cu是模块的编译单元，跟踪当前编译的类
    cu->enclosingClassBK = &classBK;

    // 读入类名后的{
    consumeCurToken(cu->curParser, TOKEN_LEFT_BRACE, "expect '{' after class name in the class declaration!");

    // 进入类体
    enterScope(cu);

    // 直到类定义结束)为止
    while (!matchToken(cu->curParser, TOKEN_RIGHT_BRACE)) {
        compileClassBody(cu, classVar);
        if (PEEK_TOKEN(cu->curParser) == TOKEN_EOF) {
            COMPILE_ERROR(cu->curParser, "expect '}' at the end od class declaration!");
        }
    }

    // .上面临时写了255个字段,现在类编译完成，回填正确的字段数
    // classBK. fields的是由compileVarDefinition函数统计的
    cu->fn->instrStream.datas[fieldNumIndex] = classBK.fields.count;

    symbolTableClear(cu->curParser->vm, &classBK.fields);
    IntBufferClear(cu->curParser->vm, &classBK.instantMethods);
    IntBufferClear(cu->curParser->vm, &classBK.staticMethods);

    // enclosingClassBK用来表示是否在编译类
    // '编译完类后要置空,编译下一个类时再重新赋值
    cu->enclosingClassBK = NULL;

    // 退出作用域，丢弃相关局部变量
    leaveScope(cu);
}

/**
 * 编译函数定义
 * @param cu
 */
static void compileFunctionDefinition(CompileUnit *cu) {
    // fun关键字只用在模块作用域中
    if (cu->enclosingUnit != NULL) {
        COMPILE_ERROR(cu->curParser, "'fun' should be in module scope!");
    }

    consumeCurToken(cu->curParser, TOKEN_ID, "missing function name!");

    // 函数名加上En前缀作为模块变量存储
    char fnName[MAX_SIGN_LEN + 4] = {'\0'};
    memmove(fnName, "Fn ", 3);
    memmove(fnName + 3, cu->curParser->preToken.start, cu->curParser->preToken.length);

    uint32_t fnNameIndex = declareVariable(cu, fnName, strlen(fnName));

    // 生成fnCU,专用于存储函数指令流
    CompileUnit fnCU;
    initCompileUint(cu->curParser, &fnCU, cu, false);
    Signature tmpFnSign = {SIGN_METHOD, "", 0, 0};  // 临时用于编译函数
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' after function name!");

    // 若有形参则将形参声明局部变量
    if (!matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
        processParaList(&fnCU, &tmpFnSign);
        consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter list!");
    }

    fnCU.fn->argNum = tmpFnSign.argNum;

    consumeCurToken(cu->curParser, TOKEN_LEFT_BRACE, "expect '{' at the begining of method body.");

    // 编译函数体,将指令流写进该函数自己的指令单元fnCu
    compileBody(&fnCU, false);

#if DEBUG
    endCompileUnit(&fnCU, fnName, strlen(fnName));
#else
    endCompileUnit(&fnCU);
#endif

    // 将栈顶的闭包写入变量
    defineVariable(cu, fnNameIndex);
}

/**
 * 编译import
 * @param cu
 */
static void compileImport(CompileUnit *cu) {
    consumeCurToken(cu->curParser, TOKEN_ID, "expect module name after export!");

    // 备份模块名token
    Token moduleNameToken = cu->curParser->preToken;

    // 导入时模块的扩展名不需要,有可能用户会把模块的扩展名加上,
    // 比如import sparrow.sp, 这时候就要跳过扩展名
    if (cu->curParser->preToken.start[cu->curParser->preToken.length] == '.') {
        printf("\nwarning!!! the importd module needn't extension!, compiler try to ignore it!\n");

        // 跳过扩展名
        getNextToken(cu->curParser); // 跳过
        getNextToken(cu->curParser); // 跳过sp
    }
    // 把模块名转为字符串,存储为常量
    ObjString *moduleName = newObjString(cu->curParser->vm, moduleNameToken.start, moduleNameToken.length);
    uint32_t constModIdx = addConstant(cu, OBJ_TO_VALUE(moduleName));

    // 为调用sys tem. importModule ("foo")压入参数system
    emitLoadModuleVar(cu, "System");
    // 为调用system. importModule("foo")压入参数foo .
    writeOpCodeShortOperand(cu, OPCODE_LOAD_CONSTANT, constModIdx);
    // 现在可以调用system. importModule ("foo")
    emitCall(cu, 1, "importModule(_)", 15);

    // 回收返回值args [0]所在空间
    writeOpCode(cu, OPCODE_POP);

    // 如果后面没有关键字for就导入结束
    if (!matchToken(cu->curParser, TOKEN_FOR)) {
        return;
    }

    // 如果后面没有关键字for就导入结束
    do {
        consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name after 'for' in import!");

        // 在本模块中声明导入的模块变量
        uint32_t varId = declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
        // 把模块变量转为字符串,存储为常量
        ObjString *constVarName = newObjString(cu->curParser->vm,
                                               cu->curParser->preToken.start,
                                               cu->curParser->preToken.length);
        uint32_t constVarIdx = addConstant(cu, OBJ_TO_VALUE(constVarName));

        // 为调用System. getModuleVariable ("foo", "bar1") 压入system
        emitLoadModuleVar(cu, "System");
        // 为调用System. ge tModuleVariable ("foo", "barl") 压入foo
        writeOpCodeShortOperand(cu, OPCODE_LOAD_CONSTANT, constModIdx);
        // 为调用System. ge tModuleVariable ("foo", "barl") 压入foo
        writeOpCodeShortOperand(cu, OPCODE_LOAD_CONSTANT, constVarIdx);
        // 调用System. getModuleVariable ("foo"，"bar1")
        emitCall(cu, 2, "getModuleVariable(_,_)", 22);

        // 此时栈项是system. getModuleVariable ("foo", "barl")的返回值,
        // 即导入的模块变量的值，下面将其同步到相应变量中
        defineVariebl(cu, varId);
    } while (matchToken(cu->curParser, TOKEN_COMMA));
}