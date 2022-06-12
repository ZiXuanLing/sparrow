#include "core.h"

#include <string.h>
#include <sys/stat.h>

#include "../include/utils.h"
#include "../object/class.h"
#include "vm.h"
#include "../compiler/compiler.h"

#define CORE_MODULE VT_TO_VALUE(VT_NULL)

char *rootDir = NULL; // 根目录

char *readFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        IO_ERROR("Could'n open file \"%s\".", path);
    }

    struct stat fileStat;
    stat(path, &fileStat);
    size_t fileSize = fileStat.st_size;
    char *fileContent = (char *)malloc(fileSize + 1);
    if (fileContent == NULL) {
        IO_ERROR("Could'n allocate memory for reading file \"%s\".", path);
    }

    size_t numRead = fread(fileContent, sizeof(char), fileSize, file);

    if (numRead < fileSize) {
        IO_ERROR("Could'n read file \"%s\".", path);
    }
    fileContent[fileSize] = '\0';

    fclose(file);
    return fileContent;
}

/**
 * @brief 
 * 
 * @param vm 
 * @param moduleName 
 * @param moduleCode 
 * @return VMResult 
 */
// VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode) {
//     return VM_RESULT_ERROR;
// }

// 返回值类型是Value类型，且是放在args[0]，args是Value数组
// RES_VALUE的参数就是Value类型，无须转换直接赋值
// 它后面是"RET_其他类型"的基础
#define RET_VALUE(value) \
do {                     \
    args[0] = value;     \
    return true;\
}while (0);

// 将obj转换为Value后作为返回值
#define RET_OBJ(objPtr) RET_VALUE(OBJ_TO_VALUE(objPtr))

// 将bool值转换为Value后作为返回值
#define RET_BOOL(boolean) RET_VALUE(BOOL_TO_VALUE(boolean))
#define RET_NUM(num) RET_VALUE(NUM_TO_VALUE(num))
#define RET_NULL RET_VALUE(VT_TO_VALUE(VT_NULL))
#define RET_TRUE RET_VALUE(VT_TO_VALUE(VT_TRUE))
#define RET_FALSE RET_VALUE(VT_TO_VALUE(VT_FALSE))

// 设置线程报错
#define SET_ERROR_FALSE(vmPtr, errMsg) \
do {                                   \
    (vmPtr)->curThread->errorObj =       \
        OBJ_TO_VALUE(newObjString(vmPtr, errMsg, strlen(errMsg))); \
        return false;\
} while (0);

// 绑定方法func到classPtr指向的类
#define PRIM_METHOD_BIND(classPtr, methodName, func) { \
    uint32_t length = strlen(methodName);              \
    int globalIdx = getIndexFromSymbolTable(&vm->allMethodNames, methodName, length); \
    if (globalIdx == -1) {                             \
        globalIdx = addSymbol(vm, &vm->allMethodNames, methodName, length);                                                   \
    }                                                  \
    Method method;                                     \
    method.type = MT_PRIMITIVE;                         \
    method.primFn = func;                              \
    bindMethod(vm, classPtr, (uint32_t)globalIdx, method);\
}

/**
 * ！object object取反 结果为false
 * @param vm
 * @param args
 * @return
 */
static bool primObjectNot(VM *vm UNUSED, Value *args) {
    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

/**
 * args[0] == args[1]
 * @param vm
 * @param args
 * @return
 */
static bool primObjectEqual(VM *vm, Value *args) {
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[0], args[1]));
    RET_VALUE(boolValue);
}

/**
 * args[0] != args[1]
 * @param vm
 * @param args
 * @return
 */
static bool primObjectNotEqual(VM *vm UNUSED, Value *args) {
    Value boolValue = BOOL_TO_VALUE(!valueIsEqual(args[0], args[1]));
    RET_VALUE(boolValue);
}

/**
 * args[0]是否为类args[1]的子类
 * @param vm
 * @param args
 * @return
 */
static bool primObjectIs(VM *vm, Value *args) {
    if (!VALUE_IS_CLASS(args[1])) {
        RUN_ERROR("argument must be class!");
    }

    Class *thisClass = getClassOfObj(vm, args[0]);
    Class *baseClass = (Class *)(args[1].objHeader);

    // 有可能是多级集成，因此自下而上遍历基类链
    while (baseClass != NULL) {
        if (thisClass == baseClass) {
            RET_VALUE(VT_TO_VALUE(VT_TRUE));
        }
        baseClass = baseClass->superClass;
    }

    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

/**
 * args[0].tostring,返回类名
 * @param vm
 * @param args
 * @return
 */
static bool primObjectToString(VM *vm UNUSED, Value *args) {
    Class *class =args[0].objHeader->class;
    Value namevalue = OBJ_TO_VALUE(class->name);
    RET_VALUE(namevalue);
}

/**
 * 返回args[0].type
 * @param vm
 * @param args
 * @return
 */
static bool primObjectType(VM *vm, Value *args) {
    Class *class = getClassOfObj(vm, args[0]);
    RET_OBJ(class);
}

/**
 * args[0].name
 * @param vm
 * @param args
 * @return
 */
static bool primClassName(VM *vm UNUSED, Value *args) {
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

/**
 * args[0].supertype
 * @param vm
 * @param args
 * @return
 */
static bool primClassSupertype(VM *vm UNUSED, Value *args) {
    Class *class = VALUE_TO_CLASS(args[0]);
    if (class->superClass != NULL) {
        RET_OBJ(class->superClass);
    }
    RET_VALUE(VT_TO_VALUE(VT_NULL));
}

/**
 *
 * @param vm
 * @param args
 * @return
 */
static bool primClassToString(VM *vm UNUSED, Value *args) {
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

/**
 *
 * @param vm
 * @param args
 * @return
 */
static bool primObjectmetaSame(VM *vm UNUSED, Value *args) {
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[1], args[2]));
    RET_VALUE(boolValue);
}

/**
 * table中查找符号symbol，找到后返回索引
 * @param table
 * @param symbol
 * @param length
 * @return
 */
int getIndexFromSymbolTable(SymbolTable *table, const char *symbol, uint32_t length) {
    ASSERT(length != 0, "length of symbol is 0！");

    uint32_t index = 0;
    while (index < table->count) {
        if (length == table->datas[index].length && memcmp(table->datas[index].str, symbol,length) == 0) {
            return (int) index;
        }
        index ++;
    }
    return -1;
}

/**
 * 往table中添加符号symbol，返回去i索引
 * @param vm
 * @param table
 * @param symbol
 * @param length
 * @return
 */
int addSymbol(VM *vm, SymbolTable *table, const char *symbol, uint32_t length) {
    ASSERT(length != 0, "length of symbol is 0！");

    String string;
    string.str = ALLOCATE_ARRAY(vm, char, length + 1);
    memcpy(string.str, symbol, length);
    string.str[length] = '\0';
    string.length = length;
    StringBufferAdd(vm, table, string);
    return (int)table->count - 1;
}

/**
 * 定义类
 * @param vm
 * @param objModule
 * @param name
 * @return
 */
static Class* defineClass(VM *vm, ObjModule *objModule, const char *name) {
    Class *class = newRawClass(vm, name, 0);

    defineModuleVar(vm, objModule, name, strlen(name), OBJ_TO_VALUE(class));
    return class;
}

/**
 * 将class->methods[index]=method
 * @param vm
 * @param class
 * @param index
 * @param method
 */
void bindMethod(VM *vm, Class *class, uint32_t index, Method method) {
    if (index >= class->methods.count) {
        Method emptyPad = {MT_NONE, {0}};
        MethodBufferFillWrite(vm, &class->methods, emptyPad, index - class->methods.count + 1);
    }

    class->methods.datas[index] = method;
}

/**
 * 绑定基类
 * @param vm
 * @param subClass
 * @param superClass
 */
void bindSuperClass(VM *vm, Class *subClass, Class *superClass) {
    subClass->superClass = superClass;

    subClass->fieldNum += subClass->fieldNum;

    // 绑定基类方法
    uint32_t idx = 0;
    while (idx < subClass->methods.count) {
        bindMethod(vm, subClass, idx, superClass->methods.datas[idx]);
        idx ++;
    }
}

/**
 * @brief 编译核心模块
 *
 * @param vm
 */
void buildCore(VM *vm) {
    // 创建核心模块 录入到vm->allModules
    ObjModule *coreModule = newObjModule(vm, NULL);

    // 创建核心模块
    mapSet(vm, vm->allModules, CORE_MODULE, OBJ_TO_VALUE(coreModule));

    // 创建object类并绑定方法
    vm->objectClass = defineClass(vm, coreModule, "object");
    PRIM_METHOD_BIND(vm->objectClass, "!", primObjectNot);
    PRIM_METHOD_BIND(vm->objectClass, "==(_)", primObjectEqual);
    PRIM_METHOD_BIND(vm->objectClass, "!=(_)", primObjectNotEqual);
    PRIM_METHOD_BIND(vm->objectClass, "is(_)", primObjectIs);
    PRIM_METHOD_BIND(vm->objectClass, "toString", primObjectToString);
    PRIM_METHOD_BIND(vm->objectClass, "type", primObjectType);

    // 定义classOfClass类
    vm->classOfClass = defineClass(vm, coreModule, "class");

    // objectClass是任何类的基类
    bindSuperClass(vm, vm->classOfClass, vm->objectClass);
    PRIM_METHOD_BIND(vm->classOfClass, "name", primClassName);
    PRIM_METHOD_BIND(vm->classOfClass, "supertype", primClassSupertype);
    PRIM_METHOD_BIND(vm->classOfClass, "toString", primClassToString);

    // 定义object类的元信息类，他无需挂在到vm
    Class *objectMetaclass = defineClass(vm, coreModule, "objectMeta");
    bindSuperClass(vm, objectMetaclass, vm->classOfClass);

    // 类型比较
    PRIM_METHOD_BIND(objectMetaclass, "same(_,_)", primObjectmetaSame);

    // 绑定各自的meta类
    vm->objectClass->objHeader.class = objectMetaclass;
    objectMetaclass->objHeader.class = vm->classOfClass;
    vm->classOfClass->objHeader.class = vm->classOfClass;
}

/**
 * 从modules中获取名为moduleName的模块
 * @param vm
 * @param moduleName
 * @return
 */
static ObjModule* getModule(VM *vm, Value moduleName) {
    Value value = mapGet(vm->allModules, moduleName);
    if (value.type == VT_UNDEFINED) {
        return NULL;
    }

    return VALUE_TO_OBJMODULE(value);
}

/**
 * 载入模块moduleName并编译
 * @param vm
 * @param moduleName
 * @param moduleCode
 * @return
 */
static ObjThread* loadModule(VM *vm, Value moduleName, const char *moduleCode) {
    // 确保模块已经在到vm->allModules
    // 先查看是否已经导入了该模块，避免重新载入
    ObjModule* module = getModule(vm, moduleName);

    // 若该模块未加载将其载入，并继承核心模块中的变量
    if (module == NULL ){
        // 创建模块并添加到vm->allModules
        ObjString* modName= VALUE_TO_OBJSTR(moduleName);
        ASSERT(modName->value.start[modName->value.length] == '\0', "string.value.start is not termionated!");

        module = newObjModule(vm, modName->value.start);
        mapSet(vm, vm->allModules, moduleName, OBJ_TO_VALUE(module));

        // 继承核心模块中的变量
        ObjModule *coreModule = getModule(vm, CORE_MODULE);
        uint32_t idx = 0;
        while (idx < coreModule->moduleVarName.count) {
            defineModuleVar(vm, module, coreModule->moduleVarName.datas[idx].str,
                            strlen(coreModule->moduleVarName.datas[idx].str),
                            coreModule->moduelVarValue.datas[idx]);
            idx ++;
        }

    }

    ObjFn *fn = compileModule(vm, module, moduleCode);
    ObjClosure *objClosure = newObjClosure(vm, fn);
    ObjThread *moduleThread = newObjThread(vm, objClosure);

    return moduleThread;
}

/**
 * 执行模块，目前为空，桩函数
 * @param vm
 * @param moduleName
 * @param moduleCode
 * @return
 */
VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode) {
    ObjThread *objThread = loadModule(vm, moduleName, moduleCode);
    return VM_RESULT_ERROR;
}

/**
 * 确保符号已添加到符号表
 * @param vm
 * @param table
 * @param symbol
 * @param length
 * @return
 */
int ensureSymbolExist(VM *vm, SymbolTable *table, const char *symbol, uint32_t length) {

    int symbolIndex = getIndexFromSymbolTable(table, symbol, length);
    if (symbolIndex == -1) {
        return addSymbol(vm, table, symbol, length);
    }
    return symbolIndex;
}