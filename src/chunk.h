#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum
{
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    // these comparisions are enough for all 6 possible comparisions
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    //
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    // unconditional jump
    OP_JUMP,
    // function call
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY,
    OP_METHOD,
    // invoke a method directly, instead of late bound
    OP_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
} OpCode;

typedef struct
{
    int count;
    int capacity;
    /* Dynamic array containing all the byte code */
    uint8_t *code;
    /* Store all lines corresponding to bytecode to show line numbers in errors
     */
    int *lines;
    /* A dynamic array which will store all the compile time constants */
    ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value value);

#endif