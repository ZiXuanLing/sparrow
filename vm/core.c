#include "core.h"

#include <string.h>
#include <sys/stat.h>

#include "../include/utils.h"
#include "../object/class.h"
#include "vm.h"

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

VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode) {
    return VM_RESULT_ERROR;
}

/**
 * @brief 编译核心模块
 * 
 * @param vm 
 */
void buildCore(VM *vm) {
    // 创建核心模块 录入到vm->allModules
    ObjModule *coreModule = newObjModule(vm, NULL);
    mapSet(vm, vm->allModules, CORE_MODULE, OBJ_TO_VALUE(coreModule));
}