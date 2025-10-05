#include "testEnv.hpp"

/*
  Adding Squirrel member variables from binding script (static or ordinary members)
  (by ATS)
 */
namespace {

class TestClass {
public:
    TestClass( ) {}
    int Func( ){ return 1; }
};

}

DECLARE_INSTANCE_TYPE(TestClass);


SQPLUS_TEST(Test_CppScriptVar)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;

    SQClassDef<TestClass>(_SC("TestClass"))
        .scriptVar(_SC("m_i"), 100, SQFalse)       // Member var
        .scriptVar(_SC("m_st_pi"), 3.14, SQTrue);  // Static member var

    // Test that it works 
    RUN_SCRIPT(_SC("\n\
tcA <- TestClass(); \n\
tcB <- TestClass(); \n\
tcB.m_i = 200; \n\
print(\"tcA.m_i: \"+tcA.m_i); \n\
print(\"tcB.m_i: \"+tcB.m_i); \n\
print(\"tcA.m_st_pi: \"+tcA.m_st_pi); \n\
print(\"TestClass.m_st_pi: \"+TestClass.m_st_pi); \n\
local s = tcA.m_i + tcB.m_i + tcA.m_st_pi + tcB.m_st_pi; \n\
print(\"s: \"+s); \n\
assert(s>306.27 && s<306.29); \n\
"));
}
