//
// Created by ZiXuan on 2022/6/11.
//
#include "obj_map.h"
#include "class.h"
#include "../vm/vm.h"
#include "obj_string.h"
#include "obj_range.h"

/**
 * 创建新map对象
 * @param vm
 * @return
 */
ObjMap* newObjMap(VM *vm) {
    ObjMap *objMap = ALLOCATE(vm, ObjMap);
    initObjHeader(vm, &objMap->objHeader, OT_MAP, vm->mapClass);
    objMap->capacity = objMap->count = 0;
    objMap->entries = NULL;
    return objMap;
}

/**
 * 计算数字的哈希码
 * @param num
 * @return
 */
static uint32_t hashNum(double num) {
    Bits64 bits64;
    bits64.num = num;
    return bits64.bits32[0] ^ bits64.bits32[1];
}

/**
 * 计算对象的哈希码
 * @param objHeader
 * @return
 */
static uint32_t hashObj(ObjHeader *objHeader) {
    switch (objHeader->type) {
        case OT_CLASS:
            return hashString(((Class *)objHeader)->name->value.start,
                              ((Class *)objHeader)->name->value.length);
//            break;
        case OT_RANGE: {
            ObjRange *objRange = (ObjRange *) objHeader;
            return hashNum(objRange->from) ^ hashNum(objRange->to);
//            break;
        }
        case OT_STRING:
            return ((ObjString *)objHeader)->hashCode;
        default:
            RUN_ERROR("the hashable are objstring, objrange and class.");
    }
    return 0;
}

/**
 * 根据value的类型调用相应的哈希函数
 * @param value
 * @return
 */
static uint32_t hashValue(Value value) {
    switch (value.type) {
        case VT_FALSE:
            return 0;
        case VT_NULL:
            return 1;
        case VT_NUM:
            return hashNum(value.num);
        case VT_OBJ:
            return hashObj(value.objHeader);
        default:
            RUN_ERROR("unsupport type hashed!");
    }
    return 0;
}

/**
 * 在entries中添加entry，如果是新的key则返回true
 * @param entries
 * @param capacity
 * @param key
 * @param value
 * @return
 */
static int addEntry(Entry *entries, uint32_t capacity, Value key, Value value) {
    uint32_t index = hashValue(key) % capacity;

    // 通过开放探测法找可用的slot
    while (true) {
        // 找到空闲的slot，说明目前没有此key，直接赋值返回
        if (entries[index].key.type == VT_UNDEFINED) {
            entries[index].key = key;
            entries[index].value = value;
            return true;
        }
        else if (valueIsEqual(entries[index].key, key)) {
            entries[index].value = value;
            return false;
        }
        index = (index + 1) % capacity;
    }
}

/**
 * 使对象objMap的容量调整到capacity
 * @param vm
 * @param objMap
 * @param newCapacity
 */
static void resizeMap(VM *vm, ObjMap *objMap, uint32_t newCapacity) {
    Entry *newEntries = ALLOCATE_ARRAY(vm, Entry, newCapacity);
    uint32_t idx = 0;
    while (idx < newCapacity) {
        newEntries[idx].key = VT_TO_VALUE(VT_UNDEFINED);
        newEntries[idx].value = VT_TO_VALUE(VT_FALSE);
        idx ++;
    }
    // 遍历老的数组，把有值的部分插入到新数组
    if (objMap->capacity > 0) {
        Entry *entryArr = objMap->entries;
        idx = 0;
        while (idx < objMap->capacity) {
            if (entryArr[idx].key.type != VT_UNDEFINED) {
                addEntry(newEntries, newCapacity, entryArr[idx].key, entryArr[idx].value);
            }
            idx ++;
        }
    }
    // 将entry数组空间回收
    DEALLOCATE_ARRAY(vm, objMap->entries, objMap->count);
    objMap->entries = newEntries;
    objMap->capacity = newCapacity;
}

/**
 * 在objmap中查找key对应的entry
 * @param objMap
 * @param value
 * @return
 */
static Entry* findEntry(ObjMap* objMap, Value key) {
    // objmap为空则返回null
    if (objMap->capacity == 0) {
        return NULL;
    }

    // 以下用开放定址法
    // 用哈希值对容量取模计算槽位
    uint32_t index = hashValue(key) % objMap->capacity;
    Entry *entry;
    while (true) {
        entry = &objMap->entries[index];

        if (valueIsEqual(entry->key, key)) {
            return entry;
        }
        if (VALUE_IS_UNDEFINED(entry->key) && VALUE_IS_FALSE(entry->value)) {
            return NULL;
        }
        index = (index + 1) % objMap->capacity;
    }
}

/**
 * 在objmap中实现key与value的关联 objmap[key]=value
 * @param vm
 * @param objMap
 * @param key
 * @param value
 */
void mapSet(VM *vm, ObjMap *objMap, Value key, Value value) {
    if (objMap->count + 1 > objMap->capacity * MAP_LOAD_PERCENT) {
        uint32_t newCapacity = objMap->capacity * CAPACITY_GROW_FACTOR;
        if (newCapacity < MIN_CAPACITY) {
            newCapacity = MIN_CAPACITY;
        }
        resizeMap(vm, objMap, newCapacity);
    }

    if (addEntry(objMap->entries, objMap->capacity, key, value)) {
        objMap->count ++;
    }
}

/**
 * 从map中查找key对应的value
 * @param objMap
 * @param key
 * @return
 */
Value mapGet(ObjMap *objMap, Value key) {
    Entry *entry = findEntry(objMap, key);
    if (entry == NULL) {
        return VT_TO_VALUE(VT_UNDEFINED);
    }
    return entry->value;
}

/**
 * 回收空间
 * @param vm
 * @param objMap
 */
void clearMap(VM *vm, ObjMap *objMap) {
    DEALLOCATE_ARRAY(vm, objMap->entries, objMap->count);
    objMap->entries = NULL;
    objMap->capacity = objMap->count = 0;
}

/**
 * 删除map的key
 * @param vm
 * @param objMap
 * @param key
 * @return
 */
Value removeKey(VM *vm, ObjMap *objMap, Value key) {
    Entry *entry = findEntry(objMap, key);

    if (entry == NULL) {
        return VT_TO_VALUE(VT_NULL);
    }

    Value value = entry->value;
    entry->key = VT_TO_VALUE(VT_UNDEFINED);
    entry->value = VT_TO_VALUE(VT_TRUE);

    objMap->count --;
    if (objMap->count == 0) {
        clearMap(vm, objMap);
    }
    else if (objMap->count < objMap->capacity / (CAPACITY_GROW_FACTOR) * MAP_LOAD_PERCENT
    && objMap->count > MIN_CAPACITY) {
        uintptr_t newCapacity = objMap->capacity;
        if (newCapacity < MIN_CAPACITY) {
            newCapacity = MIN_CAPACITY;
        }
        resizeMap(vm, objMap, newCapacity);
    }
    return value;
}