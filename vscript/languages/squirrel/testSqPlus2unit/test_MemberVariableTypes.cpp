#include "testEnv.hpp"

/*
  Support for member variable types by ATS
  - set/get for short and char
  - get for const SQChar* (read only)
  - DECLARE_ENUM_TYPE for enum arguments
  - automated typeof
  
  http://squirrel-lang.org/forums/thread/2147.aspx
 */

namespace {

class VariousMembers {
public:
    VariousMembers(char ch_ = 0,
                   short sh_ = 0,
                   const SQChar *str_ = _SC("default val"))
        
            : m_ch(ch_),
              m_sh(sh_),
              m_str(str_) {}

    enum Mode {Mode1, Mode2};
    int FuncWithEnumArg(Mode m) {return int(m);}

    char m_ch;
    short m_sh;
    const SQChar *m_str;    // Treated like a string by SqPlus get/set
};

}

DECLARE_ENUM_TYPE(VariousMembers::Mode);
DECLARE_INSTANCE_TYPE(VariousMembers);


SQPLUS_TEST(Test_MemberVariableTypes)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;

    SQClassDef<VariousMembers>(_SC("VariousMembers"))
        // enum
        .enumInt(VariousMembers::Mode1, _SC("Mode1"))
        .enumInt(VariousMembers::Mode2, _SC("Mode2"))
        .func(&VariousMembers::FuncWithEnumArg, _SC("FuncWithEnumArg"))
        // char
        .var(&VariousMembers::m_ch, _SC("ch"))
        // short
        .var(&VariousMembers::m_sh, _SC("sh"))
        // const SQChar*
        .var(&VariousMembers::m_str, _SC("str"), VAR_ACCESS_READ_ONLY)
        ;

    // char 
    RUN_SCRIPT(_SC("\n\
a <- VariousMembers(); \n\
a.ch = 111; \n\
assert(a.ch == 111); \n\
a.ch = 260; \n\
assert(a.ch != 260) \n\
"));
    
    // short
    RUN_SCRIPT(_SC("\n\
b <- VariousMembers(); \n\
b.sh = 4441; \n\
assert(b.sh == 4441); \n\
b.sh = 400000; \n\
assert(b.sh != 400000); \n\
"));
    
    // const SQChar*
    RUN_SCRIPT(_SC("\n\
c <- VariousMembers(); \n\
assert(c.str == \"default val\") \n\
"));
    
    // enum arg
    RUN_SCRIPT(_SC("\n\
d <- VariousMembers(); \n\
assert(d.Mode1 == d.FuncWithEnumArg(d.Mode1)); \n\
assert(d.Mode2 == d.FuncWithEnumArg(d.Mode2)); \n\
assert(d.Mode1 != d.FuncWithEnumArg(d.Mode2)); \n\
assert(d.Mode2 != d.FuncWithEnumArg(d.Mode1)); \n\
"));

    
    
#ifdef SQPLUS_ENABLE_TYPEOF
    // Test auto typeof
    RUN_SCRIPT(_SC("t <- \"VariousMembers\";"));
#else
    RUN_SCRIPT(_SC("t <- \"instance\";"));
#endif

    RUN_SCRIPT(_SC("\n\
e <- VariousMembers(); \n\
print(typeof(e));  \n\
assert(typeof(e) == t);  \n\
"));
}

