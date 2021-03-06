#if INTERFACE
need_t BytecodeObject;

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
};
#endif

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/IO/Stream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#include <Engine/Bytecode/Compiler.h>

VMThread             BytecodeObjectManager::Threads[8];
Uint32               BytecodeObjectManager::ThreadCount = 1;

HashMap<VMValue>*    BytecodeObjectManager::Globals = NULL;
HashMap<VMValue>*    BytecodeObjectManager::Strings = NULL;

char                 BytecodeObjectManager::CurrentObjectName[256];
Uint32               BytecodeObjectManager::CurrentObjectHash;
vector<ObjFunction*> BytecodeObjectManager::FunctionList;
vector<ObjFunction*> BytecodeObjectManager::AllFunctionList;

HashMap<Uint8*>*     BytecodeObjectManager::Sources = NULL;
HashMap<char*>*      BytecodeObjectManager::Tokens = NULL;
vector<char*>        BytecodeObjectManager::TokensList;

SDL_mutex*           BytecodeObjectManager::GlobalLock = NULL;

PUBLIC STATIC bool    BytecodeObjectManager::ThrowError(bool fatal, const char* errorMessage, ...) {
    va_list args;
    va_start(args, errorMessage);
    bool result = Threads[0].ThrowError(fatal, errorMessage, args);
    va_end(args);
    return result;
}

PUBLIC STATIC void    BytecodeObjectManager::RequestGarbageCollection() {
    #ifdef DEBUG_STRESS_GC
        ForceGarbageCollection();
    #endif
    if (GarbageCollector::GarbageSize > GarbageCollector::NextGC) {
        size_t startSize = GarbageCollector::GarbageSize;

        ForceGarbageCollection();

        // startSize = GarbageCollector::GarbageSize - startSize;
        // Log::Print(Log::LOG_INFO, "Freed garbage from %u to %u (%d), next GC at %d", (Uint32)startSize, (Uint32)GarbageCollector::GarbageSize, GarbageCollector::GarbageSize - startSize, GarbageCollector::NextGC);
    }
}
PUBLIC STATIC void    BytecodeObjectManager::ForceGarbageCollection() {
    if (BytecodeObjectManager::Lock()) {
        if (BytecodeObjectManager::ThreadCount > 1) {
            BytecodeObjectManager::Unlock();
            return;
        }

        GarbageCollector::Collect();

        BytecodeObjectManager::Unlock();
    }
}

PUBLIC STATIC void    BytecodeObjectManager::ResetStack() {
    Threads[0].ResetStack();
}

// #region Life Cycle
PUBLIC STATIC void    BytecodeObjectManager::Init() {
    if (Globals == NULL)
        Globals = new HashMap<VMValue>(NULL, 8);
    if (Sources == NULL)
        Sources = new HashMap<Uint8*>(NULL, 8);
    if (Strings == NULL)
        Strings = new HashMap<VMValue>(NULL, 8);
    if (Tokens == NULL)
        Tokens = new HashMap<char*>(NULL, 64);

    GarbageCollector::RootObject = NULL;
    GarbageCollector::NextGC = 1024 * 1024;

    GlobalLock = SDL_CreateMutex();

    memset(Threads, 0x00, sizeof(Threads));
    for (Uint32 i = 0; i < sizeof(Threads) / sizeof(VMThread); i++) {
        Threads[i].ID = i;
        Threads[i].StackTop = Threads[i].Stack;
        Threads[i].WithReceiverStackTop = Threads[i].WithReceiverStack;
        Threads[i].WithIteratorStackTop = Threads[i].WithIteratorStack;
    }
    ThreadCount = 1;
}
PUBLIC STATIC void    BytecodeObjectManager::Dispose() {
    if (Globals) {
        // NOTE: Remove GCable values from table so it may be cleaned up.
        Globals->ForAll(RemoveNonGlobalableValue);
    }

    Threads[0].FrameCount = 0;
    Threads[0].ResetStack();
    ForceGarbageCollection();

    for (size_t i = 0; i < AllFunctionList.size(); i++) {
        FreeGlobalValue(0, OBJECT_VAL(AllFunctionList[i]));
    }
    AllFunctionList.clear();

    if (Globals) {
        Log::Print(Log::LOG_VERBOSE, "Freeing values in Globals list...");
        Globals->ForAll(FreeGlobalValue);
        Log::Print(Log::LOG_VERBOSE, "Done!");
        Globals->Clear();
        delete Globals;
        Globals = NULL;
    }
    if (Sources) {
        Sources->WithAll([](Uint32 hash, Uint8* ptr) -> void {
            Memory::Free(ptr);
        });
        Sources->Clear();
        delete Sources;
        Sources = NULL;
    }
    if (Strings) {
        // Strings->WithAll([](Uint32 hash, VMValue* ptr) -> void {
        //     Memory::Free(ptr);
        // });
        Strings->Clear();
        delete Strings;
        Strings = NULL;
    }
    if (Tokens) {
        for (size_t i = 0, iSz = TokensList.size(); i < iSz; i++) {
            Memory::Free(TokensList[i]);
        }
        TokensList.clear();
        Tokens->Clear();
        delete Tokens;
        Tokens = NULL;
    }

    SDL_DestroyMutex(GlobalLock);
}
PUBLIC STATIC void    BytecodeObjectManager::RemoveGlobalableValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS:
            case OBJ_FUNCTION:
            case OBJ_NATIVE: {
                if (hash)
                    Globals->Remove(hash);
                break;
            }
            default:
                break;
        }
    }
}
PUBLIC STATIC void    BytecodeObjectManager::RemoveNonGlobalableValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS:
            case OBJ_FUNCTION:
            case OBJ_NATIVE:
                break;
            default:
                if (hash)
                    Globals->Remove(hash);
                break;
        }
    }
}
PUBLIC STATIC void    BytecodeObjectManager::FreeGlobalValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(value);
                if (klass->Name != NULL)
                    FreeValue(OBJECT_VAL(klass->Name));

                klass->Methods->ForAll(FreeGlobalValue);
                delete klass->Methods;

                GarbageCollector::GarbageSize -= sizeof(ObjClass);
                Memory::Free(klass);
                break;
            }
            case OBJ_FUNCTION: {
                ObjFunction* function = AS_FUNCTION(value);
                if (function->Name != NULL)
                    FreeValue(OBJECT_VAL(function->Name));

                for (size_t i = 0; i < function->Chunk.Constants->size(); i++)
                    FreeValue((*function->Chunk.Constants)[i]);

                GarbageCollector::GarbageSize -= sizeof(ObjFunction);
                Memory::Free(function);
                break;
            }
            case OBJ_NATIVE: {
                GarbageCollector::GarbageSize -= sizeof(ObjNative);
                Memory::Free(AS_OBJECT(value));
                break;
            }
            default:
                break;
        }
    }
}
PUBLIC STATIC void    BytecodeObjectManager::PrintHashTableValues(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS: {
                printf("OBJ_CLASS: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_FUNCTION: {
                printf("OBJ_FUNCTION: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_NATIVE: {
                printf("OBJ_NATIVE: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_BOUND_METHOD: {
                printf("OBJ_BOUND_METHOD: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_CLOSURE: {
                printf("OBJ_CLOSURE: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_INSTANCE: {
                printf("OBJ_INSTANCE: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_STRING: {
                printf("OBJ_STRING: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_ARRAY: {
                printf("OBJ_ARRAY: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_UPVALUE: {
                printf("OBJ_UPVALUE: %p\n", AS_OBJECT(value));
                break;
            }
            default:
                break;
        }
    }
}
// #endregion

// #region ValueFuncs
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsString(VMValue v) {
    if (IS_STRING(v))
        return v;

    char* buffer = (char*)malloc(512);
    int   buffer_info[2] = { 0, 512 };
    Compiler::PrintValue(&buffer, buffer_info, v);
    v = OBJECT_VAL(CopyString(buffer, buffer_info[0]));
    free(buffer);
    return v;
}
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsInteger(VMValue v) {
    float a;
    switch (v.Type) {
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            a = AS_DECIMAL(v);
            return INTEGER_VAL((int)a);
        case VAL_INTEGER:
            return v;
        case VAL_LINKED_INTEGER:
            return INTEGER_VAL(AS_INTEGER(v));
        default:
            // NOTE: Should error here instead.
            break;
    }
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsDecimal(VMValue v) {
    int a;
    switch (v.Type) {
        case VAL_DECIMAL:
            return v;
        case VAL_LINKED_DECIMAL:
            return DECIMAL_VAL(AS_DECIMAL(v));
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            a = AS_INTEGER(v);
            return DECIMAL_VAL((float)a);
        default:
            // NOTE: Should error here instead.
            break;
    }
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObjectManager::Concatenate(VMValue va, VMValue vb) {
    ObjString* a = AS_STRING(va);
    ObjString* b = AS_STRING(vb);

    int length = a->Length + b->Length;
    ObjString* result = AllocString(length);

    memcpy(result->Chars, a->Chars, a->Length);
    memcpy(result->Chars + a->Length, b->Chars, b->Length);
    result->Chars[length] = 0;
    return OBJECT_VAL(result);
}

PUBLIC STATIC bool    BytecodeObjectManager::ValuesSortaEqual(VMValue a, VMValue b) {
    if ((a.Type == VAL_DECIMAL && b.Type == VAL_INTEGER) ||
        (a.Type == VAL_INTEGER && b.Type == VAL_DECIMAL)) {
        float a_d = AS_DECIMAL(CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(CastValueAsDecimal(b));
        return (a_d == b_d);
    }

    if (IS_STRING(a) && IS_STRING(b)) {
        ObjString* astr = AS_STRING(a);
        ObjString* bstr = AS_STRING(b);
        return astr->Length == bstr->Length && !memcmp(astr->Chars, bstr->Chars, astr->Length);
    }

    if (IS_BOUND_METHOD(a) && IS_BOUND_METHOD(b)) {
        ObjBoundMethod* abm = AS_BOUND_METHOD(a);
        ObjBoundMethod* bbm = AS_BOUND_METHOD(b);
        return ValuesEqual(abm->Receiver, bbm->Receiver) && abm->Method == bbm->Method;
    }

    return BytecodeObjectManager::ValuesEqual(a, b);
}
PUBLIC STATIC bool    BytecodeObjectManager::ValuesEqual(VMValue a, VMValue b) {
    if (a.Type == VAL_LINKED_INTEGER) goto SKIP_CHECK;
    if (a.Type == VAL_LINKED_DECIMAL) goto SKIP_CHECK;
    if (b.Type == VAL_LINKED_INTEGER) goto SKIP_CHECK;
    if (b.Type == VAL_LINKED_DECIMAL) goto SKIP_CHECK;

    if (a.Type != b.Type) return false;

    SKIP_CHECK:

    switch (a.Type) {
        case VAL_LINKED_INTEGER:
        case VAL_INTEGER: return AS_INTEGER(a) == AS_INTEGER(b);

        case VAL_LINKED_DECIMAL:
        case VAL_DECIMAL: return AS_DECIMAL(a) == AS_DECIMAL(b);

        case VAL_OBJECT:  return AS_OBJECT(a) == AS_OBJECT(b);

        case VAL_NULL:  return true;
    }
    return false;
}
PUBLIC STATIC bool    BytecodeObjectManager::ValueFalsey(VMValue a) {
    if (a.Type == VAL_NULL) return true;

    switch (a.Type) {
        case VAL_LINKED_INTEGER:
        case VAL_INTEGER: return AS_INTEGER(a) == 0;
        case VAL_LINKED_DECIMAL:
        case VAL_DECIMAL: return AS_DECIMAL(a) == 0.0f;
        case VAL_OBJECT:  return false;
    }
    return false;
}

PUBLIC STATIC VMValue BytecodeObjectManager::DelinkValue(VMValue val) {
    if (IS_LINKED_DECIMAL(val))
        return DECIMAL_VAL(AS_DECIMAL(val));
    if (IS_LINKED_INTEGER(val))
        return INTEGER_VAL(AS_INTEGER(val));

    return val;
}

PUBLIC STATIC void    BytecodeObjectManager::FreeValue(VMValue value) {
    if (IS_OBJECT(value)) {
        Obj* objectPointer = AS_OBJECT(value);
        switch (OBJECT_TYPE(value)) {
            case OBJ_BOUND_METHOD: {
                GarbageCollector::GarbageSize -= sizeof(ObjBoundMethod);
                Memory::Free(objectPointer);
                break;
            }
            case OBJ_INSTANCE: {
                ObjInstance* instance = AS_INSTANCE(value);

                // An instance does not own it's values, so it's not allowed
                // to free them.

                delete instance->Fields;

                GarbageCollector::GarbageSize -= sizeof(ObjInstance);
                Memory::Free(instance);
                break;
            }
            case OBJ_STRING: {
                ObjString* string = AS_STRING(value);
                if (string->Chars != NULL)
                    Memory::Free(string->Chars);
                string->Chars = NULL;

                GarbageCollector::GarbageSize -= sizeof(ObjString);
                Memory::Free(string);
                break;
            }
            case OBJ_ARRAY: {
                ObjArray* array = AS_ARRAY(value);

                // An array does not own it's values, so it's not allowed
                // to free them.

                array->Values->clear();
                delete array->Values;

                GarbageCollector::GarbageSize -= sizeof(ObjArray);
                Memory::Free(array);
                break;
            }
            case OBJ_MAP: {
                ObjMap* map = AS_MAP(value);
                // Free keys
                map->Keys->WithAll([](Uint32, char* ptr) -> void {
                    free(ptr);
                });
                // Free Keys table
                map->Keys->Clear();
                delete map->Keys;
                // Free Values table
                map->Values->Clear();
                delete map->Values;
                //
                GarbageCollector::GarbageSize -= sizeof(ObjMap);
                Memory::Free(map);
                break;
            }
            default:
                break;
        }
    }
}
// #endregion

// #region GlobalFuncs
PUBLIC STATIC bool    BytecodeObjectManager::Lock() {
    return (SDL_LockMutex(GlobalLock) == 0);
}
PUBLIC STATIC void    BytecodeObjectManager::Unlock() {
    SDL_UnlockMutex(GlobalLock);
}

PUBLIC STATIC void    BytecodeObjectManager::DefineMethod(int index, Uint32 hash) {
    VMValue method = OBJECT_VAL(FunctionList[index]);
    ObjClass* klass = AS_CLASS(Threads[0].Peek(0)); // AS_CLASS(Peek(1));
    klass->Methods->Put(hash, method);
    Threads[0].Pop();
    // Pop();
}
PUBLIC STATIC void    BytecodeObjectManager::DefineNative(ObjClass* klass, const char* name, NativeFn function) {
    if (function == NULL) return;
    if (klass == NULL) return;
    if (name == NULL) return;

    klass->Methods->Put(name, OBJECT_VAL(NewNative(function)));
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalLinkInteger(ObjClass* klass, const char* name, int* value) {
    if (name == NULL) return;

    if (klass == NULL) {
        Globals->Put(name, INTEGER_LINK_VAL(value));
    }
    else {
        klass->Methods->Put(name, INTEGER_LINK_VAL(value));
    }
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalLinkDecimal(ObjClass* klass, const char* name, float* value) {
    if (name == NULL) return;

    if (klass == NULL) {
        Globals->Put(name, DECIMAL_LINK_VAL(value));
    }
    else {
        klass->Methods->Put(name, DECIMAL_LINK_VAL(value));
    }
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalConstInteger(ObjClass* klass, const char* name, int value) {
    if (name == NULL) return;
    if (klass == NULL)
        Globals->Put(name, INTEGER_VAL(value));
    else
        klass->Methods->Put(name, INTEGER_VAL(value));
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalConstDecimal(ObjClass* klass, const char* name, float value) {
    if (name == NULL) return;
    if (klass == NULL)
        Globals->Put(name, DECIMAL_VAL(value));
    else
        klass->Methods->Put(name, DECIMAL_VAL(value));
}

PUBLIC STATIC void    BytecodeObjectManager::LinkStandardLibrary() {
    StandardLibrary::Link();
}
PUBLIC STATIC void    BytecodeObjectManager::LinkExtensions() {
    /*
    XMLNode* extensionHCEX = XMLParser::ParseFromResource("DiscordRPC.hcex");
    if (!extensionHCEX) return;

    XMLNode* extension = extensionHCEX->children[0];

    Token title;
    if (extension->attributes.Exists("title"))
        title = extension->attributes.Get("title");

    void* sharedObject = NULL;

    for (size_t i = 0; i < extension->children.size(); i++) {
        if (XMLParser::MatchToken(extension->children[i]->name, "sofiles")) {
            XMLNode* sofiles = extension->children[i];
            for (size_t j = 0; j < sofiles->children.size(); j++) {
                if (Application::Platform == Platforms::Windows && XMLParser::MatchToken(sofiles->children[i]->name, "win")) {
                    Token sofile = sofiles->children[i]->children[0]->name;

                    char* sofile_filename = (char*)malloc(sofile.Length + 1);
                    memcpy(sofile_filename, sofile.Start, sofile.Length);
                    sofile_filename[sofile.Length] = 0;

                    sharedObject = SDL_LoadObject(sofile_filename);
                }
            }
            // Token value = configItem->children[0]->name;
            // memcpy(StartingScene, value.Start, value.Length);
            // StartingScene[value.Length] = 0;
        }
        else if (XMLParser::MatchToken(extension->children[i]->name, "import")) {
            XMLNode* import = extension->children[i];
        }
        else if (XMLParser::MatchToken(extension->children[i]->name, "functions")) {
            XMLNode* functions = extension->children[i];
        }
    }

    XMLParser::Free(extensionHCEX);
    */
}
// #endregion

// #region ObjectFuncs
PUBLIC STATIC void    BytecodeObjectManager::RunFromIBC(Uint8* head, size_t size) {
    FunctionList.clear();

    Uint8* headEnd = head + size;

    if (memcmp(Compiler::Magic, head, 4) != 0) {
        printf("Incorrect magic!\n");
        return;
    }
    head += 4;

    bool doLineNumbers;

    Uint8 opts;
    opts = *head++;
    opts = *head++;
    doLineNumbers = opts;
    opts = *head++;
    opts = *head++;

    int chunkCount = *(int*)head; head += 4;
    for (int i = 0; i < chunkCount; i++) {
        int   count = *(int*)head; head += 4;
        int   arity = *(int*)head; head += 4;
        Uint32 hash = *(int*)head; head += 4;

        ObjFunction* function = NewFunction();
        function->Arity = arity;
        function->NameHash = hash;
        function->Chunk.Count = count;

        size_t srcFnLen = strlen(CurrentObjectName);
        strcpy(function->SourceFilename, CurrentObjectName);
        function->SourceFilename[srcFnLen] = 0;

        function->Chunk.Code = head;
        head += count * 1;

        if (doLineNumbers) {
            function->Chunk.Lines = (int*)head;
            head += count * 4;
        }

        int constantCount = *(int*)head; head += 4;
        for (int c = 0; c < constantCount; c++) {
            Uint8 type = *(Uint8*)head; head++;
            switch (type) {
                case VAL_INTEGER:
                    ChunkAddConstant(&function->Chunk, INTEGER_VAL(*(int*)head));
                    head += sizeof(int);
                    break;
                case VAL_DECIMAL:
                    ChunkAddConstant(&function->Chunk, DECIMAL_VAL(*(float*)head));
                    head += sizeof(float);
                    break;
                case VAL_OBJECT:
                    // if (OBJECT_TYPE(constt) == OBJ_STRING) {
                        Uint8* headCheese = head;
                        while (*headCheese++ && (headCheese - head < 4092));

                        size_t len = headCheese - head - 1;
                        ChunkAddConstant(&function->Chunk, OBJECT_VAL(CopyString((char*)head, len)));
                        head = headCheese;
                    // }
                    // else {
                    //     printf("Unsupported object type...Chief.\n");
                    // }
                    break;
            }
        }

        FunctionList.push_back(function);
        if (i == 0)
            AllFunctionList.push_back(function);
    }

    if (doLineNumbers && Tokens) {
        int tokenCount = *(int*)head; head += 4;
        for (int t = 0; t < tokenCount; t++) {
            Uint8* headCheese = head;
            while (*headCheese++ && (headCheese - head < 4092));

            size_t len = headCheese - head - 1;

            Uint32 hash = FNV1A::EncryptData(head, len);
            char* string = (char*)Memory::Malloc(len + 1);
            memcpy(string, head, len);
            string[len] = 0;

            Tokens->Put(hash, string);
            TokensList.push_back(string);

            head = headCheese;
        }
    }

    Threads[0].RunFunction(FunctionList[0], 0);
}
PUBLIC STATIC void    BytecodeObjectManager::SetCurrentObjectHash(Uint32 hash) {
    CurrentObjectHash = hash;
}
PUBLIC STATIC Entity* BytecodeObjectManager::SpawnFunction() {
    BytecodeObject* object = new BytecodeObject;
    ObjClass* klass = AS_CLASS(Globals->Get(CurrentObjectHash));
    if (!klass) {
        if (Tokens && Tokens->Exists(CurrentObjectHash))
            Log::Print(Log::LOG_ERROR, "No class! Can't find: %s\n", Tokens->Get(CurrentObjectHash));
        else
            Log::Print(Log::LOG_ERROR, "No class! Can't find: %X\n", CurrentObjectHash);
        exit(0);
    }

    ObjInstance* instance = NewInstance(klass);
    object->Link(instance);

    char* badbadbadbad = (char*)malloc(256);
    if (Tokens && Tokens->Exists(CurrentObjectHash))
        sprintf(badbadbadbad, "BytecodeObject::Instance [%s]", Tokens->Get(CurrentObjectHash));
    else
        sprintf(badbadbadbad, "BytecodeObject::Instance [%08X]", CurrentObjectHash);

    Memory::Track(instance, badbadbadbad);
    // Memory::Track(instance, "BytecodeObject::Instance");
    Memory::Track(instance->Fields->Data, "BytecodeObject::Instance::Fields::Data");
    Memory::Track(object->Properties->Data, "BytecodeObject::Properties::Data");

    return object;
}
PUBLIC STATIC void*   BytecodeObjectManager::GetSpawnFunction(Uint32 objectNameHash, char* objectName) {
    Uint8* bytecode;
    if (*objectName == 0)
        return NULL;

    memset(CurrentObjectName, 0, 256);
    strncpy(CurrentObjectName, objectName, 255);

    if (!SourceFileMap::ClassMap->Exists(objectName))
        return NULL;

    // On first load:
    vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(objectName);
    for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
        Uint32 filenameHash = (*filenameHashList)[fn];
        // Uint32 filenameHash = objectNameHash;

        if (!Sources->Exists(filenameHash)) {
            char filename[64];
            sprintf(filename, "Objects/%08X.ibc", filenameHash);

            if (!ResourceManager::ResourceExists(filename)) {
                Log::Print(Log::LOG_WARN, "Object \"%s\" does not exist!", objectName);
                return NULL;
            }

            ResourceStream* stream = ResourceStream::New(filename);
            if (!stream) {
                // Object doesn't exist?
                return NULL;
            }

            size_t size = stream->Length();
            bytecode = (Uint8*)Memory::Malloc(size + 1); bytecode[size] = 0;
            stream->ReadBytes(bytecode, size);
            stream->Close();

            Sources->Put(filenameHash, bytecode);

            // Load the object class
            #if defined(MACOSX) || defined(LINUX) || defined(UBUNTU)
            Log::Print(Log::LOG_VERBOSE, "Loading the object \x1b[1;93m%s\x1b[m class...", objectName);
            #else
            Log::Print(Log::LOG_VERBOSE, "Loading the object %s class...", objectName);
            #endif

            RunFromIBC(bytecode, size);

            // Set native functions for that new object class
            // Log::Print(Log::LOG_VERBOSE, "Setting native functions for that new object class...");
            ObjClass* klass = AS_CLASS(Globals->Get(objectName));
            if (!klass) {
                Log::Print(Log::LOG_ERROR, "Could not find class of: %s", objectName);
                return NULL;
            }
            if (klass->Extended == 0) {
                BytecodeObjectManager::DefineNative(klass, "InView", BytecodeObject::VM_InView);
                BytecodeObjectManager::DefineNative(klass, "Animate", BytecodeObject::VM_Animate);
                BytecodeObjectManager::DefineNative(klass, "SetAnimation", BytecodeObject::VM_SetAnimation);
                BytecodeObjectManager::DefineNative(klass, "ResetAnimation", BytecodeObject::VM_ResetAnimation);
                BytecodeObjectManager::DefineNative(klass, "GetHitboxFromSprite", BytecodeObject::VM_GetHitboxFromSprite);
                BytecodeObjectManager::DefineNative(klass, "AddToRegistry", BytecodeObject::VM_AddToRegistry);
                BytecodeObjectManager::DefineNative(klass, "RemoveFromRegistry", BytecodeObject::VM_RemoveFromRegistry);
                BytecodeObjectManager::DefineNative(klass, "CollidedWithObject", BytecodeObject::VM_CollidedWithObject);
                BytecodeObjectManager::DefineNative(klass, "CollideWithObject", BytecodeObject::VM_CollideWithObject);
                BytecodeObjectManager::DefineNative(klass, "SolidCollideWithObject", BytecodeObject::VM_SolidCollideWithObject);
                BytecodeObjectManager::DefineNative(klass, "TopSolidCollideWithObject", BytecodeObject::VM_TopSolidCollideWithObject);
                BytecodeObjectManager::DefineNative(klass, "PropertyGet", BytecodeObject::VM_PropertyGet);
                BytecodeObjectManager::DefineNative(klass, "PropertyExists", BytecodeObject::VM_PropertyExists);
            }
        }
    }
    // else {
    //     bytecode = Sources->Get(objectNameHash);
    // }

    BytecodeObjectManager::SetCurrentObjectHash(Globals->HashFunction(objectName));
    return (void*)BytecodeObjectManager::SpawnFunction;
}
// #endregion
