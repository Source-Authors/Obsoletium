#include <UnitTest++.h>
#include "testEnv.hpp"

int
main(void)
{
    int result = UnitTest::RunAllTests();

    scprintf(_SC("Press RETURN to exit."));
    getchar();

    return result;
}
