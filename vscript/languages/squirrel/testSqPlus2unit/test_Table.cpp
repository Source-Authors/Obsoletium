#include "testEnv.hpp"

SQPLUS_TEST(Test_Table)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;
    SquirrelObject root = SquirrelVM::GetRootTable();

    RUN_SCRIPT(_SC("\n\
table1 <- {key1=\"keyVal\", key2 = 123}; \n\
if (\"key1\" in table1) {\n\
  print(\"Sq: Found it\"); \n\
} else {\n\
  print(\"Sq: Not found\");\n\
  assert(false)\n\
}"
                   ));
    
    SquirrelObject table1 = root.GetValue(_SC("table1"));
    if (table1.Exists(_SC("key1"))) {
        scprintf(_SC("C++: Found it.\n"));
    } else {
        scprintf(_SC("C++: Did not find it.\n"));
        CHECK(false);
    } // if
}
    
