#ifndef BYTECODEOBJECTMANAGER_H
#define BYTECODEOBJECTMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL

class BytecodeObject;

#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Types/Entity.h>

class BytecodeObjectManager {
public:
    static HashMap<VMValue>*    Globals;
    static HashMap<VMValue>*    Strings;
    static VMThread             Threads[8];
    static Uint32               ThreadCount;
    static char                 CurrentObjectName[256];
    static Uint32               CurrentObjectHash;
    static vector<ObjFunction*> FunctionList;
    static vector<ObjFunction*> AllFunctionList;
    static HashMap<Uint8*>*     Sources;
    static HashMap<char*>*      Tokens;
    static vector<char*>        TokensList;
    static SDL_mutex*           GlobalLock;

    static bool    ThrowError(bool fatal, const char* errorMessage, ...);
    static void    RequestGarbageCollection();
    static void    ForceGarbageCollection();
    static void    ResetStack();
    static void    Init();
    static void    Dispose();
    static void    RemoveGlobalableValue(Uint32 hash, VMValue value);
    static void    RemoveNonGlobalableValue(Uint32 hash, VMValue value);
    static void    FreeGlobalValue(Uint32 hash, VMValue value);
    static void    PrintHashTableValues(Uint32 hash, VMValue value);
    static VMValue CastValueAsString(VMValue v);
    static VMValue CastValueAsInteger(VMValue v);
    static VMValue CastValueAsDecimal(VMValue v);
    static VMValue Concatenate(VMValue va, VMValue vb);
    static bool    ValuesSortaEqual(VMValue a, VMValue b);
    static bool    ValuesEqual(VMValue a, VMValue b);
    static bool    ValueFalsey(VMValue a);
    static VMValue DelinkValue(VMValue val);
    static void    FreeValue(VMValue value);
    static bool    Lock();
    static void    Unlock();
    static void    DefineMethod(int index, Uint32 hash);
    static void    DefineNative(ObjClass* klass, const char* name, NativeFn function);
    static void    GlobalLinkInteger(ObjClass* klass, const char* name, int* value);
    static void    GlobalLinkDecimal(ObjClass* klass, const char* name, float* value);
    static void    GlobalConstInteger(ObjClass* klass, const char* name, int value);
    static void    GlobalConstDecimal(ObjClass* klass, const char* name, float value);
    static void    LinkStandardLibrary();
    static void    LinkExtensions();
    static void    RunFromIBC(Uint8* head);
    static void    SetCurrentObjectHash(Uint32 hash);
    static Entity* SpawnFunction();
    static void*   GetSpawnFunction(Uint32 objectNameHash, char* objectName);
};

#endif /* BYTECODEOBJECTMANAGER_H */