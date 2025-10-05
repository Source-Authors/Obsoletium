#include "testEnv.hpp"

// Example from forum post question:
class STestScripts {}; // Proxy class
class TestScripts {
public:
    int Var_ToBind1, Var_ToBind2;

    void InitScript1(void) {
        using namespace SqPlus;
        Var_ToBind1 = 808;
        RegisterGlobal(*this, &TestScripts::Test1, _SC("Test1"));
        RegisterGlobal(*this, &TestScripts::Test2, _SC("Test2"));
        BindVariable(&Var_ToBind1, _SC("Var_ToBind1"));
    } // InitScript1

    void InitScript2(void) {
        using namespace SqPlus;
        Var_ToBind2 = 909;
        SQClassDef<STestScripts>(_SC("STestScripts"))
            .staticFunc(*this,&TestScripts::Test1, _SC("Test1"))
            .staticFunc(*this,&TestScripts::Test2, _SC("Test2"))
            .staticVar(&Var_ToBind2, _SC("Var_ToBind2"))
            ;
    } // InitScript2

    void Test1(void) {
        scprintf(_SC("TestScripts::Test1() called.\n"));
    }
    void Test2(void) {
        scprintf(_SC("TestScripts::Test2() called.\n"));
    }
} testScripts;


SQPLUS_TEST(Test_Scripts)
{
    SQPLUS_TEST_TRACE();
    
    testScripts.InitScript1();
    testScripts.InitScript2();
    
    RUN_SCRIPT(_SC("\
        local testScripts = STestScripts(); \n\
        testScripts.Test1(); \n\
        testScripts.Test2(); \n\
        print(testScripts.Var_ToBind2); \n\
        assert(testScripts.Var_ToBind2 == 909); \n\
        Test1(); \n\
        Test2(); \n\
        print(Var_ToBind1); \n\
        assert(Var_ToBind1 == 808); \n\
      "));
}
