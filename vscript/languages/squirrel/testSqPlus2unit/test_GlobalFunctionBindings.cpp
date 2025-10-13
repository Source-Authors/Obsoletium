#include "testEnv.hpp"

#include "newTestObj.hpp"

// === Standard (non member) function ===
int
testFunc(HSQUIRRELVM v)
{
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    scprintf(_SC("testFunc: numParams[%d]\n"), paramCount);
    for (int i = 1; i <= paramCount; i++) {
        scprintf(_SC("param[%d]: "), i);
        switch(sa.GetType(i)) {
        case OT_TABLE:
            scprintf(_SC("OT_TABLE[%p]\n"), sa.GetObjectHandle(i)._unVal.pTable );
            break;
        case OT_INTEGER:
            scprintf(_SC("OT_INTEGER[%d]\n"),sa.GetInt(i));
            break;
        case OT_FLOAT:
            scprintf(_SC("OT_FLOAT[%f]\n"),sa.GetFloat(i));
            break;
        case OT_STRING:
            scprintf(_SC("OT_STRING[%s]\n"),sa.GetString(i));
            break;
        default:
            scprintf(_SC("TYPEID[%d]\n"),sa.GetType(i));
        } // switch
    } // for
    return SQ_OK;
} // testFunc


void
newtest(void)
{
    scprintf(_SC("NewTest\n"));
}

const SQChar *
newtestR1(const SQChar *inString)
{
    scprintf(_SC("NewTestR1: %s\n"), inString);
    return _SC("Returned String from newtestR1");
}



SQPLUS_TEST(Test_GlobalFunctionBindings)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;
    HSQUIRRELVM v = SquirrelVM::GetVMPtr();
    SquirrelObject root = SquirrelVM::GetRootTable();

    // === BEGIN Global Function binding tests ===

    // Implemented as SquirrelVM::CreateFunction(rootTable,func,name,typeMask).
    // CreateFunctionGlobal() binds a standard SQFUNCTION (stack args).
    SquirrelVM::CreateFunctionGlobal(testFunc, _SC("testFunc0"));
    SquirrelVM::CreateFunctionGlobal(testFunc, _SC("testFuncN"), _SC("n"));
    SquirrelVM::CreateFunctionGlobal(testFunc, _SC("testFuncS"), _SC("s"));

    SQPLUS_TEST_TRACE_SUB(testStandardFuncs);
    RUN_SCRIPT(_SC(" testFunc0(); testFuncN(1.); testFuncS(\"Hello\"); "));

    // === Register Standard Functions using template system
    // (function will be directly called with argument auto-marshaling) ===
    RegisterGlobal(v, newtest, _SC("test"));
    RegisterGlobal(v, newtestR1, _SC("testR1"));

    SQPLUS_TEST_TRACE_SUB(testReg3);
    RUN_SCRIPT(_SC("\n\
test(); \n\
local rv = testR1(\"Hello\"); \n\
print(rv); \n\
assert(rv == \"Returned String from newtestR1\"); \n\
"));


    // === Register Member Functions to existing classes
    // (as opposed to instances of classes) ===
    NewTestObj t1, t2, t3;
    t1.val = 123;
    t2.val = 456;
    t3.val = 789;
    RegisterGlobal(v, t1, &NewTestObj::newtest, _SC("testObj_newtest1"));
    
    // Register newtest() again with different name and object pointer.
    RegisterGlobal(v, t2, &NewTestObj::newtest, _SC("testObj_newtest2"));
    
    // Can be any object supporting closures (functions).
    SquirrelObject tr = SquirrelVM::GetRootTable();
    Register(v,
             tr.GetObjectHandle(),
             t3,
             &NewTestObj::newtestR1,
             _SC("testObj_newtestR1")); // Return value version.

    SQPLUS_TEST_TRACE_SUB(testReg4);
    RUN_SCRIPT(_SC("\n\
print(\"\\nMembers:\"); \n\
testObj_newtest1(); \n\
testObj_newtest2(); \n\
local rv2 = testObj_newtestR1(\"Hello Again\"); \n\
print(rv2); \n\
assert(rv2 == \"Returned String from NewTestObj::newtestR1\"); \n\
"));
    // === END Global Function binding tests ===
}
