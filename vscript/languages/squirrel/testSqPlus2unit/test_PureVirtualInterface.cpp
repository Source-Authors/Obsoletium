#include "testEnv.hpp"

class PureInterface {
public:
    virtual void pureFunc1(void) = 0;
    virtual void pureFunc2(void) = 0;
};

class MyImp : public PureInterface {
public:
    PureInterface *getInterface(void) {
        return static_cast<PureInterface*>(this);
    }
    virtual void pureFunc1(void) {
        scprintf(_SC("PureFunc1 called [0x%p].\n"), this);
    }
    virtual void pureFunc2(void) {
        scprintf(_SC("PureFunc2 called [0x%p].\n"), this);
    }
};

class InterfaceHolder {
public:
  PureInterface *theInterface;
  void setInterface(PureInterface *pureInterface) {
      theInterface = pureInterface;
  }
  PureInterface *getInterface(void) {
      return theInterface;
  }
};

DECLARE_INSTANCE_TYPE(PureInterface);
DECLARE_INSTANCE_TYPE(MyImp);
DECLARE_INSTANCE_TYPE(InterfaceHolder);


SQPLUS_TEST(Test_PureVirtualInterface)
{
    SQPLUS_TEST_TRACE();
    
    using namespace SqPlus;
    
    SQClassDefNoConstructor<PureInterface>(_SC("PureInterface"))
        .func(&PureInterface::pureFunc1, _SC("pureFunc1"))
        .func(&PureInterface::pureFunc2, _SC("pureFunc2"))
        ;
    SQClassDef<InterfaceHolder>(_SC("InterfaceHolder"))
        .func(&InterfaceHolder::setInterface, _SC("setInterface"))
        .func(&InterfaceHolder::getInterface, _SC("getInterface"))
        ;
    SQClassDef<MyImp>(_SC("MyImp"))
        .func(&MyImp::getInterface, _SC("getInterface"))
        ;

    MyImp myImp;

    RUN_SCRIPT(_SC("ih <- InterfaceHolder();"));

    SquirrelObject root = SquirrelVM::GetRootTable();
    SquirrelObject ih = root.GetValue(_SC("ih"));
    InterfaceHolder *ihp = reinterpret_cast<InterfaceHolder*>(
        ih.GetInstanceUP(ClassType<InterfaceHolder>::type())
        );
    ihp->setInterface(&myImp);

    RUN_SCRIPT(_SC("\
      ih.getInterface().pureFunc1(); \n\
      ih.getInterface().pureFunc2(); \n\
      ihp <- ih.getInterface(); \n\
      ihp.pureFunc1(); \n\
      ihp.pureFunc2(); \n\
      myIh <- MyImp(); \n\
      ih.setInterface(myIh.getInterface()); \n\
      ih.getInterface().pureFunc1(); \n\
      ih.getInterface().pureFunc2(); \n\
    "));
}
