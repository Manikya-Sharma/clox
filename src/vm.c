#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

VM vm;

static Value clockNative(int argCount, Value *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack()
{
    // the stack is not allocated and need not be freed
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // show call stack
    for (int i = vm.frameCount - 1; i >= 0; i--)
    {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        } else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char *name, NativeFn function)
{
    // push and then pop beacuse of GC
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM()
{
    resetStack();
    vm.objects = NULL;

    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initTable(&vm.strings);
    initTable(&vm.globals);

    // we cant let GC run at initial string allocation, so first chagge to NULL
    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    defineNative("clock", clockNative);
}

void freeVM()
{
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    // peek instead of pop to avoid GC freeing it just now
    ObjString *b = AS_STRING(peek(0));
    ObjString *a = AS_STRING(peek(1));
    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    ObjString *result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static bool call(ObjClosure *closure, int argCount)
{
    if (argCount != closure->function->arity)
    {
        runtimeError("Expected %d arguments but got %d",
                     closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    // -1 because slot 0 is for methods
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            // the runtime will never invoke bare OBJ_FUNCTION
        case OBJ_CLOSURE:
        {
            return call(AS_CLOSURE(callee), argCount);
        }
        case OBJ_NATIVE:
        {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }
        case OBJ_CLASS:
        {
            ObjClass *klass = AS_CLASS(callee);
            vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));

            // init() method
            Value initializer;
            if (tableGet(&klass->methods, vm.initString, &initializer))
            {
                return call(AS_CLOSURE(initializer), argCount);
            } else if (argCount != 0)
            {
                runtimeError("Expected 0 arguments but got %d", argCount);
                return false;
            }
            return true;
        }
        case OBJ_BOUND_METHOD:
        {
            ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
            // put the 'this' variable at slot zero
            vm.stackTop[-argCount - 1] = bound->receiver;
            return call(bound->method, argCount);
        }
        default:
            // object cannot be called
            break;
        }
    }
    runtimeError("Can only call funcitons and classes");
    return false;
}

static ObjUpvalue *captureUpvalue(Value *local)
{

    // look for existing upvalue
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    // create new upvalue
    ObjUpvalue *createdUpvalue = newUpvalue(local);
    // insert the upvalue in the sorted manner
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL)
    {
        vm.openUpvalues = createdUpvalue;
    } else
    {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

// at this instruction, free up the stack and put the upvalues on heap
static void closeUpvalues(Value *last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
    {
        ObjUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjString *name)
{
    Value method = peek(0);
    ObjClass *klass = AS_CLASS(peek(1));
    // add the closure to methods table
    tableSet(&klass->methods, name, method);
    // remove the closure
    pop();
}

static bool bindMethod(ObjClass *klass, ObjString *name)
{
    Value method;
    if (!tableGet(&klass->methods, name, &method))
    {
        runtimeError("Undefined property '%s'", name->chars);
        return false;
    }
    ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount)
{
    Value method;
    if (!tableGet(&klass->methods, name, &method))
    {
        runtimeError("Undefined property '%s'", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString *name, int argCount)
{
    Value receiver = peek(argCount);
    if (!IS_INSTANCE(receiver))
    {
        runtimeError("Only instances have methods");
        return false;
    }
    ObjInstance *instance = AS_INSTANCE(receiver);

    // before looking for a method, look for a field
    Value value;
    if (tableGet(&instance->fields, name, &value))
    {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    return invokeFromClass(instance->klass, name, argCount);
}

static InterpretResult run()
{
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
    // macros
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
// do while loop is to ensure macro is hygenic
#define BINARY_OP(valueType, op)                                               \
    do                                                                         \
    {                                                                          \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))                        \
        {                                                                      \
            runtimeError("Operands must be numbers");                          \
            return INTERPRET_RUNTIME_ERROR;                                    \
        }                                                                      \
        double b = AS_NUMBER(pop());                                           \
        double a = AS_NUMBER(pop());                                           \
        push(valueType(a op b));                                               \
    } while (false)

    // main loop
    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
        {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(
            &frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        // dispatchinig the instruction
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_PRINT:
        {
            printValue(pop());
            // we dont push back value in statement
            // statement has total stack effect fo zero
            printf("\n");
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
                frame->ip += offset;
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CLOSURE:
        {
            ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
            ObjClosure *closure = newClosure(function);
            push(OBJ_VAL(closure));

            for (int i = 0; i < closure->upvalueCount; i++)
            {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal)
                {
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                } else
                {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }

            break;
        }
        case OP_CLOSE_UPVALUE:
        {
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        }
        case OP_CALL:
        {
            // the frames for the parent calle and the function called are
            // overlapping so, same value is being reused
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_RETURN:
        {
            Value result = pop();
            closeUpvalues(frame->slots);
            vm.frameCount--;
            if (vm.frameCount == 0)
            {
                pop();
                return INTERPRET_OK;
            }
            vm.stackTop = frame->slots;
            push(result);
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_POP:
            pop();
            break;
        case OP_GET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0)))
            {
                // tableSet automatically sets the variable so we need to ermove
                // it for REPL
                tableDelete(&vm.globals, name);
                runtimeError("Undefined Variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value))
            {
                runtimeError("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL:
        {
            ObjString *name = READ_STRING();
            tableSet(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_EQUAL:
        {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_ADD:
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else
            {
                runtimeError("Operands must be two numbers or two string");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
            break;
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_NOT:
            push(BOOL_VAL(isFalsey(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_CLASS:
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        case OP_GET_PROPERTY:
        {
            if (!IS_INSTANCE(peek(0)))
            {
                runtimeError("Only instances have properties");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance *instance = AS_INSTANCE(peek(0));
            ObjString *name = READ_STRING();
            Value value;
            if (tableGet(&instance->fields, name, &value))
            {
                pop(); // instance
                push(value);
                break;
            }
            if (!bindMethod(instance->klass, name))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SET_PROPERTY:
        {
            if (!IS_INSTANCE(peek(1)))
            {
                runtimeError("Only instances have fields");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance *instance = AS_INSTANCE(peek(1));
            tableSet(&instance->fields, READ_STRING(), peek(0));
            Value value = pop();
            pop();
            push(value);
            break;
        }
        case OP_METHOD:
            defineMethod(READ_STRING());
            break;
        case OP_INVOKE:
        {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            if (!invoke(method, argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_INHERIT:
        {
            Value superclass = peek(1);

            if (!IS_CLASS(superclass))
            {
                runtimeError("Superclass must be a class");
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjClass *subclass = AS_CLASS(peek(0));
            // methods in subclass will override the methods copied from
            // superclass
            tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
            pop(); // subclass
            break;
        }
        case OP_GET_SUPER:
        {
            ObjString *name = READ_STRING();
            ObjClass *superclass = AS_CLASS(pop());
            // superclass is sitting on top of the stack
            if (!bindMethod(superclass, name))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUPER_INVOKE:
        {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            ObjClass *superclass = AS_CLASS(pop());
            if (!invokeFromClass(superclass, method, argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
}

InterpretResult interpret(const char *source)
{
    ObjFunction *function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    // push and pop function for GC
    push(OBJ_VAL(function));

    // initiate the CallFrame for global script
    ObjClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}