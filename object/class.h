#ifndef __OBJECT_CLASS_H__

#define __OBJECT_CLASS_H__

#include "../include/common.h"
#include "../include/utils.h"
#include "header_obj.h"
#include "obj_string.h"
#include "obj_fn.h"
#include "obj_range.h"
#include "obj_list.h"
#include "obj_map.h"

typedef enum {
    MT_NONE, // 空方法类型，并不等同于undefined
    MT_PRIMITIVE, // 在vm中实现c的原生方法
    MT_SCRIPT, // 脚本中定义的方法
    MT_FN_CALL // 有关函数对象的调用方法，用来实现函数重载
} MethodType;  // 方法类型

#define VT_TO_VALUE(vt) ((Value){vt, {0}})

#define BOOL_TO_VALUE(boolean) (boolean ? VT_TO_VALUE(VT_TRUE) : VT_TO_VALUE(vt_false))
#define VALUE_TO_BOOL(value) ((value).type == VT_TRUE ? true: false)

#define NUM_TO_VALUE(num) ((Value){VT_NUM, {num}})
#define VALUE_TO_NUM(value) value.num

#define OBJ_TO_VALUE(objPtr) ({ \
    Value value; \
    value.type = VT_OBJ; \
    value.objHeader = (ObjHeader *)(objPtr); \
    value; \
})

#define VALUE_TO_OBJ(value) (value.objHeader)
#define VALUE_TO_OBJSTR(value) ((ObjString *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJFN(value) ((ObjFn *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJRANGE(value) ((ObjRange *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJINSTANCE(value) ((ObjInstance *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJLIST(value) ((ObjList *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJMAP(value) ((ObjMap *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJCLOSURE(value) ((ObjClosure *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJTHREAD(value) ((ObjThread *)VALUE_TO_OBJ(value))
#define VALUE_TO_OBJMODULE(value) ((ObjModule *)VALUE_TO_OBJ(value))
#define VALUE_TO_CLASS(value) ((Class *)VALUE_TO_OBJ(value))

#define VALUE_IS_UNDEFINED(value) ((value).type == VT_UNDEFINEED)
#define VALUE_IS_NULL(value) ((value).type == VT_NULL)
#define VALUE_IS_TRUE(value) ((value).type == VT_TRUE)
#define VALUE_IS_FALSE(value) ((value).type == VT_FALSE)
#define VALUE_IS_NUM(value) ((value).type == VT_NUM)
#define VALUE_IS_OBJ(value) ((value).type == VT_OBJ)
#define VALUE_IS_CREATIN_OBJ(value, objType) (VALUE_IS_OBJ(value) && VALUE_TO_OBJ(value)->type == objType)
#define VALUE_IS_OBJSTR(value) ((value).type == OT_STRING)
#define VALUE_IS_OBJINSTANCE(value) ((value).type == OT_INSTANCE)
#define VALUE_IS_OBJCLOSURE(value) ((value).type == OT_CLOSURE)
#define VALUE_IS_OBJRANGE(value) ((value).type == OT_RANGE)
#define VALUE_IS_OBJCLASS(value) ((value).type == OT_CLASS)
#define VALUE_IS_0(value) (VALUE_IS_NUM(value) && (value).num == 0)

// 原生方法指针
typedef int (*Primitive)(VM *vm, Value *value);

typedef struct {
    MethodType type;
    union {
        Primitive primFn;  // 指向脚本方法所关联的C实现
        ObjClosure *obj;
    };
} Method;

DECLARE_BUFFER_TYPE(Method)

struct class {
    ObjHeader objHeader;
    struct class *superClass; // 父类
    uint32_t fieldNum; // 本类的字段数，包括基类的字段数
    MethodBuffer methods;
    ObjString *name; // 类名
};  // 对象类

typedef union {
    uint64_t bits64;
    uint32_t bits32[2];
    double num;
} Bits64;

#define CAPACITY_GROW_FACTOR 4
#define MIN_CAPACITY 64

#endif //!__OBJECT_CLASS_H__