#include "memory.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{

    // keep track of bytes allocated for GC
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize)
    {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
        if (vm.bytesAllocated > vm.nextGC)
        {
            collectGarbage();
        }
    }

    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}

void markObject(Obj *object)
{
    if (object == NULL)
        return;
    // don't add already gray object to avoid indefinite cycles
    if (object->isMarked)
        return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    // add all gray objects to worklist
    if (vm.grayCapacity < vm.grayCount + 1)
    {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        // reallocate manually beacuse we dont want GC to collect this dynamic
        // memory
        vm.grayStack =
            (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);

        // we are directly aborting if GC can't allocate
        if (vm.grayStack == NULL)
            exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value)
{
    if (IS_OBJ(value))
        markObject(AS_OBJ(value));
}

static void freeObject(Obj *object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->type);
#endif

    switch (object->type)
    {
    case OBJ_STRING:
    {
        ObjString *string = (ObjString *)object;
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjString, object);
        break;
    }
    case OBJ_FUNCTION:
    {
        ObjFunction *function = (ObjFunction *)object;
        freeChunk(&function->chunk);
        FREE(ObjFunction, object);
        break;
    }
    case OBJ_NATIVE:
    {
        FREE(ObjNative, object);
        break;
    }
    case OBJ_CLOSURE:
    {
        ObjClosure *closure = (ObjClosure *)object;
        FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
        // we do not free the function because it might be referenced by
        // multiple functions
        // GC will free it
        FREE(ObjClosure, object);
        break;
    }
    case OBJ_UPVALUE:
    {
        FREE(ObjUpvalue, object);
        break;
    }
    }
}

// mark the root values which are always accessible
static void markRoots()
{
    // values in stack
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
    {
        markValue(*slot);
    }

    // call stacks
    for (int i = 0; i < vm.frameCount; i++)
    {
        markObject((Obj *)vm.frames[i].closure);
    }

    // open upvalues
    for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
         upvalue = upvalue->next)
    {
        markObject((Obj *)upvalue);
    }

    // the globals
    markTable(&vm.globals);

    markCompilerRoots();
}

static void markArray(ValueArray *array)
{
    for (int i = 0; i < array->count; i++)
    {
        markValue(array->values[i]);
    }
}

static void blackenObject(Obj *object)
{

#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type)
    {
    case OBJ_NATIVE:
    case OBJ_STRING:
        break;
    case OBJ_UPVALUE:
        markValue(((ObjUpvalue *)object)->closed);
        break;
    case OBJ_FUNCTION:
    {

        ObjFunction *function = (ObjFunction *)object;
        markObject((Obj *)function->name);
        markArray(&function->chunk.constants);
        break;
    }
    case OBJ_CLOSURE:
    {
        ObjClosure *closure = (ObjClosure *)object;
        markObject((Obj *)closure->function);
        for (int i = 0; i < closure->upvalueCount; i++)
        {
            markObject((Obj *)closure->upvalues[i]);
        }
        break;
    }
    }
}

// white objects: Not visited by GC
// gray objects: Visited by GC but their connections are not
// back objects: Visited by GC and their connections have been made gray
static void traceReferences()
{
    while (vm.grayCount > 0)
    {
        Obj *object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

// free the unreached objects
static void sweep()
{
    Obj *previous = NULL;
    Obj *object = vm.objects;
    while (object != NULL)
    {
        if (object->isMarked)
        {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else
        {
            Obj *unreached = object;
            object = object->next;
            if (previous != NULL)
            {
                previous->next = object;
            } else
            {
                vm.objects = object;
            }
            freeObject(unreached);
        }
    }
}

// the following definition is used to identify which memory can still be
// potentially freed
/*
1. Root objects can be accessed
2. Anything accessible from root element can be accessed
 */
void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    // special treatment to strings due to interning
    tableRemoveWhite(&vm.strings);
    sweep();

    // schedule next iteration of GC
    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

void freeObjects()
{
    Obj *object = vm.objects;
    while (object != NULL)
    {
        Obj *next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.grayStack);
}
