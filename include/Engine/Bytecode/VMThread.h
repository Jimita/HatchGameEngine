#ifndef ENGINE_BYTECODE_VMTHREAD_H
#define ENGINE_BYTECODE_VMTHREAD_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

class VMThread {
public:
    struct WithIter {
    void* entity;
    void* entityNext;
    int   index;
    void* list;
    }; 
    VMValue   Stack[STACK_SIZE_MAX];
    VMValue*  StackTop = Stack;
    VMValue   RegisterValue;
    CallFrame Frames[FRAMES_MAX];
    Uint32    FrameCount;
    Uint32    ReturnFrame;
    VMValue   WithReceiverStack[16];
    VMValue*  WithReceiverStackTop = WithReceiverStack;
    WithIter  WithIteratorStack[16];
    WithIter* WithIteratorStackTop = WithIteratorStack;
    enum ThreadState {
    CREATED = 0,
    RUNNING = 1,
    WAITING = 2,
    CLOSED = 3,
    }; 
    char      Name[THREAD_NAME_MAX];
    Uint32    ID;
    Uint32    State;
    bool      DebugInfo;

    bool    ThrowError(bool fatal, const char* errorMessage, ...);
    void    PrintStack();
    void    Push(VMValue value);
    VMValue Pop();
    VMValue Peek(int offset);
    Uint32  StackSize();
    void    ResetStack();
    Uint8   ReadByte(CallFrame* frame);
    Uint16  ReadUInt16(CallFrame* frame);
    Uint32  ReadUInt32(CallFrame* frame);
    Sint16  ReadSInt16(CallFrame* frame);
    Sint32  ReadSInt32(CallFrame* frame);
    VMValue ReadConstant(CallFrame* frame);
    int     RunInstruction();
    void    RunInstructionSet();
    void    RunValue(VMValue value, int argCount);
    void    RunFunction(ObjFunction* func, int argCount);
    void    RunInvoke(Uint32 hash, int argCount);
    bool    BindMethod(ObjClass* klass, Uint32 hash);
    bool    CallValue(VMValue callee, int argCount);
    bool    Call(ObjFunction* function, int argCount);
    bool    InvokeFromClass(ObjClass* klass, Uint32 hash, int argCount);
    bool    Invoke(Uint32 hash, int argCount);
    VMValue Values_Multiply();
    VMValue Values_Division();
    VMValue Values_Modulo();
    VMValue Values_Plus();
    VMValue Values_Minus();
    VMValue Values_BitwiseLeft();
    VMValue Values_BitwiseRight();
    VMValue Values_BitwiseAnd();
    VMValue Values_BitwiseXor();
    VMValue Values_BitwiseOr();
    VMValue Values_LogicalAND();
    VMValue Values_LogicalOR();
    VMValue Values_LessThan();
    VMValue Values_GreaterThan();
    VMValue Values_LessThanOrEqual();
    VMValue Values_GreaterThanOrEqual();
    VMValue Values_Increment();
    VMValue Values_Decrement();
    VMValue Values_Negate();
    VMValue Values_LogicalNOT();
    VMValue Values_BitwiseNOT();
};

#endif /* ENGINE_BYTECODE_VMTHREAD_H */
