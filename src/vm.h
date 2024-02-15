#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#define STACK_MAX 256

typedef struct
{
    Chunk *chunk;
    // instruction pointer or the program counter
    uint8_t *ip;

    // VM stack for executing instructions
    Value stack[STACK_MAX];
    // the top points just after the top element of stack
    // so empty when stackTop points to 0
    Value *stackTop;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(Chunk *chunk);
void push(Value value);
Value pop();

#endif