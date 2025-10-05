#include "testEnv.hpp"

template<typename T>
struct Point {
    Point() {}
    Point(T X, T Y) : X(X), Y(Y) {}
    T X, Y;
};

template<typename T>
struct Box {
    Box() {}
    Box(Point<T> UpperLeft, Point<T> LowerRight) :
            UpperLeft(UpperLeft), LowerRight(LowerRight) {}
    Point<T> UpperLeft, LowerRight;
    void print(void) {
        scprintf(_SC("UL.X %f UL.Y %f LR.X %f LR.Y %f\n"),
                 UpperLeft.X, UpperLeft.Y, LowerRight.X, LowerRight.Y);
    }
};

template<typename T>
struct Window {
    int id;
    Box<T> box;
};

typedef Point<float> Pointf;
typedef Box<float> Boxf;
typedef Window<float> Windowf;

int
constructPointf(float X, float Y, HSQUIRRELVM v)
{
    using namespace SqPlus;
    return PostConstruct<Pointf>(v,
                                 new Pointf(X,Y),
                                 ReleaseClassPtr<Pointf>::release);
}

// Must pass by reference or pointer (not copy)
int
constructBoxf(Pointf &UpperLeft, Pointf &LowerRight, HSQUIRRELVM v)
{
    using namespace SqPlus;
    return PostConstruct<Boxf>(v,
                               new Boxf(UpperLeft, LowerRight),
                               ReleaseClassPtr<Boxf>::release);
}

struct WindowHolder {
    static Windowf *currentWindow;
    static Windowf *getWindow(void) {
        return currentWindow;
    }
};
Windowf * WindowHolder::currentWindow = 0;

DECLARE_INSTANCE_TYPE(Pointf);
DECLARE_INSTANCE_TYPE(Boxf);
DECLARE_INSTANCE_TYPE(Windowf);

SQPLUS_TEST(Test_PointBoxf)
{
    SQPLUS_TEST_TRACE();
    
    using namespace SqPlus;

    SQClassDef<Pointf>()
        .staticFunc(constructPointf, _SC("constructor"))
        .var(&Pointf::X, _SC("X"))
        .var(&Pointf::Y, _SC("Y"))
        ;
    SQClassDef<Boxf>()
        .staticFunc(constructBoxf, _SC("constructor"))
        .func(&Boxf::print, _SC("print"))
        .var(&Boxf::UpperLeft, _SC("UpperLeft"))
        .var(&Boxf::LowerRight, _SC("LowerRight"))
        ;
    SQClassDef<Windowf>()
        .var(&Windowf::id, _SC("Id"))
        .var(&Windowf::box, _SC("Box"))
        ;
    
    RegisterGlobal(WindowHolder::getWindow, _SC("getWindow"));
    
    Windowf myWindow;
    myWindow.id = 42;
    myWindow.box = Boxf(Pointf(1.f, 2.f), Pointf(3.f, 4.f));
    WindowHolder::currentWindow = &myWindow;

    // The createWindow() function below creates a new instance on the
    // root table.  The instance data is a pointer to the C/C++
    // instance, and will not be freed or otherwise managed.
    RUN_SCRIPT(_SC("\
    local MyWindow = Windowf(); \n\
    MyWindow.Box = Boxf(Pointf(11., 22.), Pointf(33., 44.)); \n\
    MyWindow.Box.print(); \n\
    print(MyWindow.Box.LowerRight.Y); \n\
    assert(MyWindow.Box.LowerRight.Y == 44.);\n\
    MyWindow.Box.LowerRight.Y += 1.; \n\
    local MyWindow2 = Windowf(); \n\
    MyWindow2 = MyWindow; \n\
    print(MyWindow2.Box.LowerRight.Y); \n\
    assert(MyWindow2.Box.LowerRight.Y == 45.); \n\
    local MyBox = Boxf(Pointf(10., 20.), Pointf(30., 40.)); \n\
    MyBox.UpperLeft = Pointf(1000., 1000.); \n\
    MyBox.UpperLeft.X = 5000. \n\
    MyBox.print(); \n\
    print(MyBox.UpperLeft.X) \n\
    print(MyBox.UpperLeft.Y) \n\
    assert(MyBox.UpperLeft.X == 5000.) \n\
    assert(MyBox.UpperLeft.Y == 1000.) \n\
") _SC(" \n\
    MyWindow2.Box = MyBox; \n\
    assert(MyWindow2.Box.UpperLeft.X == 5000.); \n\
    assert(MyWindow2.Box.UpperLeft.Y == 1000.); \n\
    assert(MyWindow2.Box.LowerRight.X == 30.); \n\
    assert(MyWindow2.Box.LowerRight.Y == 40.); \n\
    MyWindow2 = getWindow(); \n\
    print(\"MyWindow2: \"+MyWindow2.Id); \n\
    assert(MyWindow2.Id == 42); \n\
    MyWindow2.Box.print(); \n\
    assert(MyWindow2.Box.UpperLeft.X == 1.); \n\
    assert(MyWindow2.Box.UpperLeft.Y == 2.); \n\
    assert(MyWindow2.Box.LowerRight.X == 3.); \n\
    assert(MyWindow2.Box.LowerRight.Y == 4.); \n\
    function createWindow(name, instance) { \n\
      ::rawset(name, instance); \n\
    } \n\
"));

    Windowf window = myWindow;
    window.id = 54;
    window.box.UpperLeft.X  += 1;
    window.box.UpperLeft.Y  += 1;
    window.box.LowerRight.X += 1;
    window.box.LowerRight.Y += 1;
    
    // Create a new Window instance "NewWindow" on the root table.
    SquirrelFunction<void>(_SC("createWindow"))(_SC("NewWindow"), &window);

    RUN_SCRIPT(_SC("\n\
    print(\"NewWindow: \"+NewWindow.Id); \n\
    assert(NewWindow.Id == 54); \n\
    NewWindow.Box.print(); \n\
    assert(NewWindow.Box.UpperLeft.X == 2.); \n\
    assert(NewWindow.Box.UpperLeft.Y == 3.); \n\
    assert(NewWindow.Box.LowerRight.X == 4.); \n\
    assert(NewWindow.Box.LowerRight.Y == 5.); \n\
  "));
}

