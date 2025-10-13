#include "testEnv.hpp"

inline const std::string &
getStdString(void)
{
    static std::string var = "= Test String =";
    return var;
}

inline const std::string &
echoStdString(const std::string &src)
{
    static std::string var;
    var = src;
    return var;
}

SQPLUS_TEST(Test_STD_String)
{
    SQPLUS_TEST_TRACE();

#if defined(SQUNICODE)
    printf("Skipped (No direct binding of std::string when unicode)\n");
#elif defined(SQPLUS_SUPPORT_STD_STRING)
    SqPlus::RegisterGlobal(getStdString, _SC("getStdString"));
    SqPlus::RegisterGlobal(echoStdString, _SC("echoStdString"));
      
    RUN_SCRIPT(_SC("\n\
local str = getStdString(); \n\
assert(str == \"= Test String =\"); \n\
"));
    
    RUN_SCRIPT(_SC("\n\
local str = echoStdString(\"echo me\"); \n\
assert(str == \"echo me\"); \n\
"));

    // Binding std::string by PallavNawani
    // in http://squirrel-lang.org/forums/2370/ShowThread.aspx
    std::string testStr = "[test string]";
    SqPlus::BindVariable(&testStr, _SC("testStr"));
    
    // c++ -> squirrel
    RUN_SCRIPT(_SC("\n\
assert(testStr == \"[test string]\"); \n\
"));
    
    // squirrel -> c++
    RUN_SCRIPT(_SC("\n\
testStr = \"[modified string]\"; \n\
"));
    CHECK_EQUAL(testStr, "[modified string]");
    
#else
    printf("Skipped (SQPLUS_SUPPORT_STD_STRING is not defined)\n");
#endif // SQUNICODE
}

