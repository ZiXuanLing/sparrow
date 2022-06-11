#ifndef __SPARROW_CORE_H__

#define __SPARROW_CORE_H__

#include "vm.h"

extern char *rootDir;
char *readFile(const char *sourceFile);

VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode);
void buildCore(VM *vm);

#endif // !__SPARROW_CORE_H__