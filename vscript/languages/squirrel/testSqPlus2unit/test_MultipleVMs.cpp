#include "testEnv.hpp"
#include <iostream>

SQPLUS_TEST(Test_MultipleVMs)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;

    struct SquirrelVMSys vms1;
    SquirrelVM::GetVMSys(vms1); // Get Original VMsys

    SquirrelVM::Init();
    struct SquirrelVMSys vms2;
    SquirrelVM::GetVMSys(vms2); // Get new VMsys

    
    // Run script in original VMsys
    SquirrelVM::SetVMSys(vms1);
    RUN_SCRIPT(_SC("\
x <- 1.0; \n\
"));

    // Run script in new VMsys
    SquirrelVM::SetVMSys(vms2);
    RUN_SCRIPT(_SC("\
assert(!(\"x\" in getroottable())); \n\
x <- 2.0; \n\
"));
    
    // Run script in original VMsys
    SquirrelVM::SetVMSys(vms1);
    RUN_SCRIPT(_SC("\
assert(x == 1.0);\n\
"));

    // # Change: SquirrelVMSys releases ref on VM in destructor
    // Shutdown new VMsys
    // (Original VMsys will be terminated by testing framework)
    /*
    SquirrelVM::SetVMSys(vms2);
    SquirrelVM::Shutdown();
    */
}
