#include "cli.h"
#include <stdio.h>
#include <string.h>

#include "../parser/parser.h"
#include "../vm/vm.h"
#include "../vm/core.h"
#include "../object/class.h"


static void runFile(const char *path) {
    const char *lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {  // 设置脚本文件的根目录
        char *root = (char *)malloc(lastSlash - path + 2);
        memcpy(root, path, lastSlash - path + 1);
        root[lastSlash - path + 1] = '\0';
        rootDir = root;
    }

    VM *vm = newVM();
    const char *sourceCode = readFile(path);

    executeModule(vm, OBJ_TO_VALUE(newObjString(vm, path, strlen(path))), sourceCode);

    // struct parser parser;
    // initParser(vm, &parser, path, sourceCode, NULL);

    // #include "../parser/token.list"

    // while (parser.curToken.type != TOKEN_EOF) {
    //     getNextToken(&parser);
    //     printf("%dL: %s [", parser.curToken.lineNo, tokenArray[parser.curToken.type]);
    //     uint32_t idx = 0;
    //     while (idx < parser.curToken.length) {
    //         printf("%c", *(parser.curToken.start + idx ++));
    //     }
    //     printf("]\n");
    // }
}

int main(int argc, const char **argv) {
    if (argc == 1) {
        ;
    }
    else {
        printf("%s\n", argv[0]);
        printf("%s\n", argv[1]);
        runFile(argv[1]);
    }
    return 0;
}
