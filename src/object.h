#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// must ensure correct type before downcasting
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)

typedef enum
{
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE
} ObjType;

// type punning / struct inheritance
struct Obj
{
    ObjType type;
    // create a linked list for garbage collector
    struct Obj *next;
};

// functions are first class, so they are also objects
typedef struct
{
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

// native functions like 'time'
typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString
{
    // obj is the state shared by all heap allocated objects
    //
    // the pointer to ObjString will always initially point to its first field
    // as a part of C specification
    // therefore, ObjString* can be safely cast to Obj*
    Obj obj;
    int length;
    char *chars;
    // hash for table
    uint32_t hash;
};

ObjFunction *newFunction();

ObjNative *newNative(NativeFn function);

ObjString *copyString(const char *chars, int length);

ObjString *takeString(char *chars, int length);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif