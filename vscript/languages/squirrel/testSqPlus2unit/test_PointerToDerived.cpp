#include "testEnv.hpp"

/*
  http://squirrel-lang.org/forums/thread/1875.aspx
  http://wiki.squirrel-lang.org/default.aspx/SquirrelWiki/SqPlusNativeCreatedInstancesWithCorrectAncestry.html
*/

namespace {

class cBase {
public:
    cBase(void) : myId(id++) {}
    virtual ~cBase() {}

    void echo(void) {printf("cBase(id = %d)\n", myId);}
    const SQChar *Name(void) {return _SC("cBase");}
    const SQChar *BaseSpecific(void) {return _SC("BaseSpecific(void)");}

    int myId;
    static int id;
};

class cDerived : public cBase {
public:
    cDerived(void) : cBase(), myId(id++) {}
    virtual ~cDerived() {}

    void echo(void) {
        printf("cDerived(id = %d), cBase(id = %d)\n", myId, cBase::myId);
    }
    const SQChar *Name(void) {return _SC("cDerived");}
    const SQChar *DerivedSpecific(void) {return _SC("DerivedSpecific(void)");}

    int myId;
    static int id;
};

int cBase::id = 0;
int cDerived::id = 1000;

}

DECLARE_INSTANCE_TYPE(cBase);
DECLARE_INSTANCE_TYPE(cDerived);


cBase b;
cDerived d;

cBase &GetBase() {return b;}
cDerived &GetDerived() {return d;}
cBase *GetBasePtr() {return &b;}
cDerived *GetDerivedPtr() {return &d;}


SQPLUS_TEST(Test_PointerToDerived)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;
    
#ifdef SQ_USE_CLASS_INHERITANCE

    SQClassDef<cBase>(_SC("cBase"))
        .func(&cBase::echo, _SC("echo"))
        .func(&cBase::Name, _SC("Name"))
        .func(&cBase::BaseSpecific, _SC("BaseSpecific"));

    SQClassDef<cDerived>(_SC("cDerived"), _SC("cBase"))
        .func(&cDerived::echo, _SC("echo"))
        .func(&cDerived::Name, _SC("Name"))
        .func(&cDerived::DerivedSpecific, _SC("DerivedSpecific"));

    RegisterGlobal(&GetBase, _SC("GetBase"));
    RegisterGlobal(&GetDerived, _SC("GetDerived"));
    RegisterGlobal(&GetBasePtr, _SC("GetBasePtr"));
    RegisterGlobal(&GetDerivedPtr, _SC("GetDerivedPtr"));


    SQPLUS_TEST_TRACE_SUB(b <- cBase());
    RUN_SCRIPT(_SC("\n\
b <- cBase(); \n\
b.echo() \n\
assert(b.Name() == \"cBase\"); \n\
assert(b.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(b <- GetBase());
    RUN_SCRIPT(_SC("\n\
b <- GetBase(); \n\
b.echo() \n\
assert(b.Name() == \"cBase\"); \n\
assert(b.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(b <- GetBasePtr());
    RUN_SCRIPT(_SC("\n\
b <- GetBasePtr(); \n\
b.echo() \n\
assert(b.Name() == \"cBase\"); \n\
assert(b.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(d <- cDerived());
    RUN_SCRIPT(_SC("\n\
d <- cDerived(); \n\
d.echo() \n\
assert(d.Name() == \"cDerived\"); \n\
assert(d.DerivedSpecific() == \"DerivedSpecific(void)\"); \n\
assert(d.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(d <- GetDerived());
    RUN_SCRIPT(_SC("\n\
d <- GetDerived(); \n\
d.echo() \n\
assert(d.Name() == \"cDerived\"); \n\
assert(d.DerivedSpecific() == \"DerivedSpecific(void)\"); \n\
assert(d.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));

    SQPLUS_TEST_TRACE_SUB(d <- GetDerivedPtr());
    RUN_SCRIPT(_SC("\n\
d <- GetDerivedPtr(); \n\
d.echo() \n\
assert(d.Name() == \"cDerived\"); \n\
assert(d.DerivedSpecific() == \"DerivedSpecific(void)\"); \n\
assert(d.BaseSpecific() == \"BaseSpecific(void)\"); \n\
"));
#endif
}
