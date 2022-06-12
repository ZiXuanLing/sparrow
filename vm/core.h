#ifndef __SPARROW_CORE_H__

#define __SPARROW_CORE_H__

#include "vm.h"
#include "../object/class.h"

extern char *rootDir;
char *readFile(const char *sourceFile);

VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode);
void buildCore(VM *vm);
static bool primObjectNot(VM *vm UNUSED, Value *args);
static bool primObjectEqual(VM *vm, Value *args);
static bool primObjectNotEqual(VM *vm UNUSED, Value *args);
static bool primObjectIs(VM *vm, Value *args);
static bool primObjectToString(VM *vm UNUSED, Value *args);
static bool primObjectType(VM *vm, Value *args);
static bool primClassName(VM *vm UNUSED, Value *args);
static bool primClassSupertype(VM *vm UNUSED, Value *args);
static bool primClassToString(VM *vm UNUSED, Value *args);
static bool primObjectmetaSame(VM *vm UNUSED, Value *args);
int getIndexFromSymbolTable(SymbolTable *table, const char *symbol, uint32_t length);
int addSymbol(VM *vm, SymbolTable *table, const char *symbol, uint32_t length);
static Class* defineClass(VM *vm, ObjModule *objModule, const char *name);
void bindMethod(VM *vm, Class *class, uint32_t index, Method method);
void bindSuperClass(VM *vm, Class *subClass, Class *superClass);
static ObjModule* getModule(VM *vm, Value moduleName);
static ObjThread* loadModule(VM *vm, Value moduleName, const char *moduleCode);

#endif // !__SPARROW_CORE_H__