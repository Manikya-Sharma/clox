#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// a tagged union to store the data type for vm
typedef enum
{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER
} ValueType;

// including 4 bit padding, it takes 16 bytes
typedef struct
{
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;
} Value;

// macros to cast types

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

bool valuesEqual(Value a, Value b);

typedef struct
{
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif