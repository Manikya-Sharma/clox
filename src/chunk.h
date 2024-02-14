#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum
{
    OP_CONSTANT,
    OP_RETURN
} OpCode;

typedef struct
{
    int count;
    int capacity;
    /* Dynamic array containing all the byte code */
    uint8_t *code;
    /* Store all lines corresponding to bytecode to show line numbers in errors */
    int*lines;
    /* A dynamic array which will store all the compile time constants */
    ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value value);

#endif