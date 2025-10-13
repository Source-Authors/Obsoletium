#include "testEnv.hpp"

#include "globals.hpp"
#include "newTestObj.hpp"
#include "constants.hpp"


// Using global functions to construct and release classes.
int
releaseNewTestObj(SQUserPointer up, SQInteger size)
{
    SQ_DELETE_CLASS(NewTestObj);
} // releaseNewTestObj

int
constructNewTestObj(HSQUIRRELVM v)
{
    using namespace SqPlus;
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    if (paramCount == 1) {
        return PostConstruct<NewTestObj>(v,
                                         new NewTestObj(),
                                         releaseNewTestObj);
    } else if (paramCount == 4) {
        return PostConstruct<NewTestObj>(v,
                                         new NewTestObj(
                                             sa.GetString(2),
                                             sa.GetInt(3),
                                             sa.GetBool(4)? true: false
                                             ),
                                         releaseNewTestObj);
    } // if
    return sq_throwerror(v, _SC("Invalid Constructor arguments"));
} // constructNewTestObj


// Using fixed args with auto-marshaling. Note that the HSQUIRRELVM
// must be last in the argument list (and must be present to send to
// PostConstruct).
//
// SquirrelVM::GetVMPtr() could also be used with PostConstruct(): no
// HSQUIRRELVM argument would be required.
int
constructNewTestObjFixedArgs(const SQChar *s, int val, bool b, HSQUIRRELVM v)
{
    using namespace SqPlus;
    StackHandler sa(v);
    //int paramCount = sa.GetParamCount();
    return PostConstruct<NewTestObj>(v,
                                     new NewTestObj(s,val,b),
                                     releaseNewTestObj);
} // constructNewTestObj



struct CustomTestObj {
    SqPlus::ScriptStringVar128 name;
    int val;
    bool state;
    CustomTestObj() : val(0), state(false) {name = _SC("empty");}
    CustomTestObj(const SQChar *_name, int _val, bool _state) : val(_val), state(_state) {
        name = _name;
    }

    // Custom variable argument constructor
    static int construct(HSQUIRRELVM v) {
        using namespace SqPlus;
        StackHandler sa(v);
        int paramCount = sa.GetParamCount();
        if (paramCount == 1) {
            return PostConstruct<CustomTestObj>(v,
                                                new CustomTestObj(),
                                                release);
        } if (paramCount == 4) {
            return PostConstruct<CustomTestObj>(
                v,
                new CustomTestObj(sa.GetString(2),
                                  sa.GetInt(3),
                                  sa.GetBool(4) ? true : false),
                release
                );
        } // if
        return sq_throwerror(v,_SC("Invalid Constructor arguments"));
    } // construct

    SQ_DECLARE_RELEASE(CustomTestObj); // Required when using a custom
                                       // constructor.
    
    // Member function that handles variable types.
    int varArgTypes(HSQUIRRELVM v) {
        StackHandler sa(v);
        //int paramCount = sa.GetParamCount();
        if (sa.GetType(2) == OT_INTEGER) {
            val = sa.GetInt(2);
        } // if
        if (sa.GetType(2) == OT_STRING) {
            name = sa.GetString(2);
        } // if
        if (sa.GetType(3) == OT_INTEGER) {
            val = sa.GetInt(3);
        } // if
        if (sa.GetType(3) == OT_STRING) {
            name = sa.GetString(3);
        } // if
        //return sa.ThrowError(_SC("varArgTypes() error"));
        return 0;
    } // varArgTypes

    // Member function that handles variable types and has variable
    // return types+count.
    int varArgTypesAndCount(HSQUIRRELVM v) {
        StackHandler sa(v);
        int paramCount = sa.GetParamCount();
        SQObjectType type1 = (SQObjectType)sa.GetType(1); // Always OT_INSTANCE
        SQObjectType type2 = (SQObjectType)sa.GetType(2);
        SQObjectType type3 = (SQObjectType)sa.GetType(3);
        SQObjectType type4 = (SQObjectType)sa.GetType(4);
        printf( "typesum: %d\n", type1+type2+type3+type4 ); // Make compiler quiet
        int returnCount = 0;
        if (paramCount == 3) {
            sq_pushinteger(v, val);
            returnCount = 1;
        } else if (paramCount == 4) {
            sq_pushinteger(v, val);
            sq_pushstring(v, name, -1);
            returnCount = 2;
        } // if
        return returnCount;
    } // varArgTypesAndCount

    int noArgsVariableReturn(HSQUIRRELVM v) {
        if (val == 123) {
            val++;
            return 0; // This will print (null).
        } else if (val == 124) {
            sq_pushinteger(v, val); // Will return int:124.
            val++;
            return 1;
        } else if (val == 125) {
            sq_pushinteger(v, val);
            name = _SC("Case 125");
            sq_pushstring(v, name, -1);
            val = 123; // reset
            return 2;
        } // if
        return 0;
    } // noArgsVariableReturn

    // Registered with func() instead of funcVarArgs(): fixed (single)
    // return type.
    const SQChar *variableArgsFixedReturnType(HSQUIRRELVM v) {
        StackHandler sa(v);
        int paramCount = sa.GetParamCount();
        SQObjectType type1 = (SQObjectType)sa.GetType(1); // Always OT_INSTANCE
        SQObjectType type2 = (SQObjectType)sa.GetType(2);
        SQObjectType type3 = (SQObjectType)sa.GetType(3);
        printf( "typesum: %d\n", type1+type2+type3 ); // Make compiler quiet
        if (paramCount == 1) {
            return _SC("No Args");
        } else if (paramCount == 2) {
            return _SC("One Arg");
        } else if (paramCount == 3) {
            return _SC("Two Args");
        } // if
        return _SC("More than two args");
    } // variableArgsFixedReturnType

    void manyArgs(int i, float f, bool b, const SQChar *s) {
        scprintf(_SC("CustomTestObj::manyArgs() i: %d, f: %f, b: %s, s: %s\n"),
                 i, f, b ?_SC("true") : _SC("false"), s);
    } // manyArgs

    float manyArgsR1(int i, float f, bool b, const SQChar *s)  {
        manyArgs(i, f, b, s);
        return i + f;
    } // manyArgsR1
}; // struct CustomTestObj


class TestBase {
public:
    int x;
    TestBase() : x(0) {
        printf("Constructing TestBase[0x%x]\n", (size_t)this);
    }
    void print(void) {
        printf("TestBase[0x%x], x[%d]\n", (size_t)this, x);
    }
}; // class TestBase
DECLARE_INSTANCE_TYPE(TestBase);

class TestDerivedCPP : public TestBase {
public:
    int y;
    TestDerivedCPP() {
        x = 121;
    }
}; // class TestDerivedCPP

typedef void (TestDerivedCPP::*TestDerivedCPP_print)(void);




SQPLUS_TEST(Test_ClassInstance)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;
    //HSQUIRRELVM v = SquirrelVM::GetVMPtr();
    SquirrelObject root = SquirrelVM::GetRootTable();


    // === BEGIN Class Instance tests ===

    // Example showing two methods for registration.
    SQClassDef<NewTestObj>(_SC("NewTestObj"))
        // If a special constructor+destructor are not needed, the
        // auto-generated versions can be used.
        
        // Example methods for custom constructors:
        
        /* Using a global constructor: useful in cases where a custom
           constructor/destructor are required and the original class
           is not to be modified. */
        .staticFuncVarArgs(constructNewTestObj, _SC("constructor"), _SC("*"))

        /* Using a global constructor: useful in cases where a custom
           constructor/destructor are required and the original class
           is not to be modified. */
        //.staticFunc(constructNewTestObjFixedArgs, _SC("constructor"))

        /* Using a static member constructor. */
        //.staticFuncVarArgs(NewTestObj::construct, _SC("constructor"))

        // Any global function can be registered in a class namespace
        // (no 'this' pointer will be passed to the function).
        .staticFunc(globalFunc, _SC("globalFunc"))
        
        .staticFunc(globalClass, &GlobalClass::func, _SC("globalClassFunc"))
        .func(&NewTestObj::newtestR1, _SC("newtestR1"))
        .var(&NewTestObj::val, _SC("val"))
        .var(&NewTestObj::s1, _SC("s1"))
        .var(&NewTestObj::s2, _SC("s2"))
        .var(&NewTestObj::c1, _SC("c1"), VAR_ACCESS_READ_ONLY)
        .var(&NewTestObj::c2, _SC("c2"), VAR_ACCESS_READ_ONLY)
        .funcVarArgs(&NewTestObj::multiArgs, _SC("multiArgs"))
        ;

    BindConstant(SQ_PI * 2, _SC("SQ_PI_2"));
    BindConstant(SQ_10 * 2, _SC("SQ_10_2"));
    BindConstant(_SC("Global String"), _SC("GLOBAL_STRING"));

    SQPLUS_TEST_TRACE_SUB(testConstants0);
    RUN_SCRIPT(_SC(" \n\
print(\"SQ_PI*2: \" + SQ_PI_2 + \" SQ_10_2: \" + SQ_10_2 + \" GLOBAL_STRING: \" + GLOBAL_STRING); \n\
assert(SQ_PI_2 == 6.283185307179586); \n\
assert(SQ_10_2 == 20); \n\
assert(GLOBAL_STRING == \"Global String\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(scriptedBase);
    RUN_SCRIPT(_SC("\n\
class ScriptedBase { \n\
  sbval = 5551212;\n\
  function multiArgs(a, ...) { \n\
    print(\"ScriptedBase::multiArgs() \" + a + val); \n\
  } \n\
} \n\
"));                            // Note val does not exist in base.


#ifdef SQ_USE_CLASS_INHERITANCE
    /* Base class constructors, if registered, must use
       this form: static int construct(HSQUIRRELVM v). */
    SQClassDef<CustomTestObj> customClass(_SC("CustomTestObj"),
                                          _SC("ScriptedBase"));
    
    /* MUST use this form (or no args) if CustomTestObj will be used
       as a base class.
       Using the "*" form will allow a single constructor to be used
       for all cases. */
    customClass.staticFuncVarArgs(&CustomTestObj::construct,
                                  _SC("constructor"), _SC("*"));
    
    /* This form is also OK if used as a base class. */
    // customClass.staticFuncVarArgs(CustomTestObj::construct,
    //                               _SC("constructor"));
    

    // "*": no type or count checking.
    customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,
                            _SC("multiArgs"), _SC("*"));
    // "*": no type or count checking.    
    customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,
                            _SC("varArgTypesAndCount"), _SC("*"));
    
#else
    SQClassDef<CustomTestObj> customClass(_SC("CustomTestObj"));

    // string, number, bool (all types must match).      
    customClass.staticFuncVarArgs(&CustomTestObj::construct,
                                  _SC("constructor"),_SC("snb"));
    // "*": no type or count checking.      
    customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,
                            _SC("varArgTypesAndCount"),_SC("*"));
    
#endif // SQ_USE_CLASS_INHERITANCE
      
      
    // string or number + string or number.
    customClass.funcVarArgs(&CustomTestObj::varArgTypes,
                            _SC("varArgTypes"),_SC("s|ns|ns|ns|n"));
    // No type string means no arguments allowed.    
    customClass.funcVarArgs(&CustomTestObj::noArgsVariableReturn,
                            _SC("noArgsVariableReturn"));
    // Variables args, fixed return type.    
    customClass.func(&CustomTestObj::variableArgsFixedReturnType,
                     _SC("variableArgsFixedReturnType"));
    // Many args, type checked.    
    customClass.func(&CustomTestObj::manyArgs,_SC("manyArgs"));

    // Many args, type checked, one return value.    
    customClass.func(&CustomTestObj::manyArgsR1,_SC("manyArgsR1"));

    
#ifdef SQ_USE_CLASS_INHERITANCE
    printf("=== BEGIN INHERITANCE ===\n");

    SQClassDef<TestBase>(_SC("TestBase"))
        .var(&TestBase::x, _SC("x"))
        .func(&TestBase::print, _SC("print"))
        ;
    SQClassDef<TestDerivedCPP>(_SC("TestDerivedCPP"))
        .func((TestDerivedCPP_print)&TestDerivedCPP::print, _SC("print"))
        ;
    
    // Note that the constructor definition and call below is not
    // required for this example.
    // (The C/C++ constructor will be called automatically).

    SQPLUS_TEST_TRACE_SUB(testInheritance2);
    RUN_SCRIPT(_SC("\
class TestDerived extends TestBase { \n\
  function print() {                 \n\
    ::TestBase.print();              \n\
    ::print(\"TestDerived::print() \" + x); \n\
  }                                  \n\
  constructor() {                    \n\
    TestBase.constructor();          \n\
  }                                  \n\
}                                    \n\
local a = TestDerived();             \n\
local b = TestDerived();             \n\
a.x = 1;                             \n\
b.x = 2;                             \n\
print(\"a.x = \" + a.x);             \n\
print(\"b.x = \" + b.x);             \n\
a.print();                           \n\
b.print();                           \n\
local c = TestDerivedCPP();          \n\
c.print();                           \n\
           "));

    SQPLUS_TEST_TRACE_SUB(testInheritance);
    RUN_SCRIPT(_SC("\n\
class Derived extends CustomTestObj { \n\
  val = 888; \n\
  function multiArgs(a, ...) { \n\
    print(\"Derived::multiArgs() \" + a + val); \n\
    print(sbval); \n\
    ::CustomTestObj.multiArgs(4); \n\
    ::ScriptedBase.multiArgs(5, 6, 7); \n\
  } \n\
} \n\
local x = Derived(); \n\
print(\"x.val \" + x.val); \n\
x.multiArgs(1, 2, 3); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg0);
    RUN_SCRIPT(_SC(" \n\
co <- CustomTestObj(\"hello\", 123, true); \n\
co.varArgTypes(\"str\", 123, 123, \"str\"); \n\
co.varArgTypes(123, \"str\", \"str\", 123); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg0a);
    RUN_SCRIPT(_SC(" \n\
print(co.varArgTypesAndCount(1, true)); \n\
print(co.varArgTypesAndCount(2, false, 3.)); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg0b);
    RUN_SCRIPT(_SC(" \n\
print(co.noArgsVariableReturn()); \n\
print(co.noArgsVariableReturn()); \n\
print(co.noArgsVariableReturn()); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg0c);
    RUN_SCRIPT(_SC(" \n\
print(co.variableArgsFixedReturnType(1)); \n\
print(co.variableArgsFixedReturnType(1,2)); \n\
print(co.variableArgsFixedReturnType(1,2,3)); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg0d);
    RUN_SCRIPT(_SC(" \n\
co.manyArgs(111, 222.2, true, \"Hello\"); \n\
print(co.manyArgsR1(333, 444.3, false, \"World\")); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg1a);
    RUN_SCRIPT(_SC(" \n\
co <- CustomTestObj(\"hello\", 123, true); \n\
co.noArgsVariableReturn(); \n\
local t = NewTestObj(\"S1in\", 369, true); \n\
print(\"C1: \" + t.c1); \n\
print(\"C2: \" + t.c2); // t.c1 = 123; \n\
"));
      
    printf("===  END INHERITANCE  ===\n");
#endif // SQ_USE_CLASS_INHERITANCE


    SQPLUS_TEST_TRACE_SUB(Constant test (read only var));
    // Var can change on C++ side, but not on script side.
    RUN_SCRIPT_THROW(_SC("local t = NewTestObj(); t.c1 = 123;"),
                     _SC("setVar(): Cannot write to constant: c1"));
      
    SQPLUS_TEST_TRACE_SUB(testReg1);
    RUN_SCRIPT(_SC(" \n\
local t = NewTestObj(); \n\
t.newtestR1(\"Hello\"); \n\
t.val = 789; \n\
print(t.val); \n\
print(t.s1); \n\
print(t.s2); \n\
t.s1 = \"New S1\"; \n\
print(t.s1); \n\
"));

    SQPLUS_TEST_TRACE_SUB(testReg2);
    RUN_SCRIPT(_SC(" \n\
local t = NewTestObj(); \n\
t.val = 789; \n\
print(t.val); \n\
t.val = 876; \n\
print(t.val); \n\
t.multiArgs(1, 2, 3); \n\
t.multiArgs(1, 2, 3, 4); \n\
t.globalFunc(\"Hola\", 5150, false); \n\
t.globalClassFunc(\"Bueno\", 5151, true); \n\
"));


    SQPLUS_TEST_TRACE_SUB(defCallFunc);
    RUN_SCRIPT(_SC(" \n\
function callMe(var) { \n\
  print(\"callMe(var): I was called by: \" + var); \n\
  return 123; \n\
} \n\
"));

    SQPLUS_TEST_TRACE_SUB(defCallFuncStrRet);
    RUN_SCRIPT(_SC(" \n\
function callMeStrRet(var) { \n\
  print(\"callMeStrRet(var): I was called by: \" + var); \n\
  return var; \n\
} \n\
"));


    
    SQPLUS_TEST_TRACE_SUB(SquirrelFunction);
    
    // direct function call: callMe("Squirrel 1")
    SquirrelFunction<void>(_SC("callMe"))(_SC("Squirrel 1"));

    // Get a function from the root table and call it.
    SquirrelFunction<int> callFunc(_SC("callMe"));
    int ival = callFunc(_SC("Squirrel"));
    scprintf(_SC("IVal: %d\n"), ival);
    CHECK_EQUAL(ival, 123);

    SquirrelFunction<const SQChar *> callFuncStrRet(_SC("callMeStrRet"));
    const SQChar *sval = callFuncStrRet(_SC("Squirrel StrRet"));
    scprintf(_SC("SVal: %s\n"), sval);
    CHECK_EQUAL(sval, _SC("Squirrel StrRet"));

    // Argument count is checked; type is not.    
    ival = callFunc(456); 
    
    // Get a function from any table.
    SquirrelFunction<int> callFunc2(root.GetObjectHandle(), _SC("callMe"));
    CHECK_EQUAL(ival, callFunc2(789));

    // === END Class Instance tests ===
}
