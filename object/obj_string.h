#ifndef SPARROW_OBJ_STRING_H
#define SPARROW_OBJ_STRING_H

#include "header_obj.h"

typedef struct {
    ObjHeader objHeader;
    uint32_t  hashCode;
    CharValue value;
} ObjString;

uint32_t hashString(char *str, uint32_t length);
void hashObjString(ObjString *objString);
ObjString* newObjString(VM *vm, const char *str, uint32_t length);

#endif //SPARROW_OBJ_STRING_H