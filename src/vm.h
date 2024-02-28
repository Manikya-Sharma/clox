#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjClosure *closure;
    // function implementations have their own pointer so that we can return
    // back to original control flow
    uint8_t *ip;
    // slots points to the VM's value stack at the slot
    // that this function can use
    Value *slots;

} CallFrame;

typedef struct
{
    // frames are not heap allocated because they should be fast
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    // VM stack for executing instructions
    Value stack[STACK_MAX];
    // the top points just after the top element of stack
    // so empty when stackTop points to 0
    Value *stackTop;
    Table globals;
    Table strings;
    // linked list of open upValues owned by VM
    ObjUpvalue *openUpvalues;
    // pointer to head of ll of objects
    Obj *objects;

    // gray stack for GC
    int grayCount;
    int grayCapacity;
    Obj **grayStack;

    // how frequently GC should run
    size_t bytesAllocated;
    size_t nextGC;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// expos the global vm object
extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

#endif