#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// must ensure correct type before downcasting
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod *)AS_OBJ(value))

typedef enum
{
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
} ObjType;

// type punning / struct inheritance
struct Obj
{
    ObjType type;
    // marking for GC
    bool isMarked;
    // create a linked list for garbage collector
    struct Obj *next;
};

// functions are first class, so they are also objects
typedef struct
{
    Obj obj;
    int arity;
    int upvalueCount;
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

// upvalues need to love longer than their function, so they have to be
// dynamically allocated
typedef struct ObjUpvalue
{
    Obj obj;
    Value *location;
    // it stores the value transferred from stack to heap
    Value closed;
    // linked list which will store the open upvalues slots
    struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct
{
    Obj obj;
    ObjFunction *function;
    // dynamic array of upvalues beacuse different closures have different
    // number of upvalues
    ObjUpvalue **upvalues;
    int upvalueCount;

} ObjClosure;

typedef struct
{
    Obj obj;
    ObjString *name;
    Table methods;
} ObjClass;

typedef struct
{
    Obj obj;
    ObjClass *klass;
    Table fields;
} ObjInstance;

// the separate struct for class methods which can bound ot 'this'
typedef struct
{
    Obj obj;
    Value receiver;
    ObjClosure *method;
} ObjBoundMethod;

ObjFunction *newFunction();

ObjNative *newNative(NativeFn function);

ObjClosure *newClosure(ObjFunction *function);

ObjUpvalue *newUpvalue(Value *slot);

ObjString *copyString(const char *chars, int length);

ObjString *takeString(char *chars, int length);

ObjClass *newClass(ObjString *name);

ObjInstance *newInstance(ObjClass *klass);

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif