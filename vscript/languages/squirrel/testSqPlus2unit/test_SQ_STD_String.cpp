#include "testEnv.hpp"

#ifdef SQPLUS_SUPPORT_SQ_STD_STRING
inline const SqPlus::sq_std_string &
getString(void)
{
    static SqPlus::sq_std_string var = _SC("= Test String =");
    return var;
}

inline const SqPlus::sq_std_string &
echoString(const SqPlus::sq_std_string &src)
{
    static SqPlus::sq_std_string var;
    var = src;
    return var;
}
#endif

SQPLUS_TEST(Test_SQ_STD_String)
{
    SQPLUS_TEST_TRACE();

#ifdef SQPLUS_SUPPORT_SQ_STD_STRING
    SqPlus::RegisterGlobal(getString, _SC("getString"));
    SqPlus::RegisterGlobal(echoString, _SC("echoString"));
      
    RUN_SCRIPT(_SC("\n\
local str = getString(); \n\
assert(str == \"= Test String =\"); \n\
"));
    
    RUN_SCRIPT(_SC("\n\
local str = echoString(\"echo me\"); \n\
assert(str == \"echo me\"); \n\
"));

    // BindVariable of sq_std_string (not yet available)
    //SqPlus::sq_std_string str = _SC("[test string]");
    //SqPlus::BindVariable(&str, _SC("str"));
    
#else
    printf("Skipped (SQPLUS_SUPPORT_SQ_STD_STRING is not defined)\n");
#endif
}
