#define SQPLUS_OVERLOAD_OPT
#define SQPLUS_CONST_OPT

#include "testEnv.hpp"


/*
  Overloaded functions
 */
void overloadedFunc(void){scprintf(_SC("void overloadedFunc(void)\n"));}
int overloadedFunc(int)  {scprintf(_SC("int overloadedFunc(int)\n")); return 1;}
void overloadedFunc(float){scprintf(_SC("void overloadedFunc(float)\n"));}

/*
  `Tuple3' class with overloaded member functions
 */
struct Tuple3 {
    float x, y, z;

    // constructors
    Tuple3(void) {x = 1.0f; y = 2.0f; z = 3.0f; scprintf(_SC("%p - Tuple3(void)\n"),(void*)this); }
    Tuple3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) { scprintf(_SC("%p - Tuple3(x,y,z)\n"),(void*)this); }

    // destructor
    virtual ~Tuple3() {scprintf(_SC("%p - ~Tuple3()\n"),(void*)this);}

    // overloaded member functions
    void set(const Tuple3 &src) {x = src.x; y = src.y; z = src.z;}
    float set(float xyz) {x = y = z = xyz; return xyz;}
    void set(float x_, float y_, float z_) {x = x_; y = y_; z = z_;}

    /* overloaded static member functions */
    static float staticVar;
    static void setStaticVar(void) {staticVar = 0.0;}
    static void setStaticVar(float f) {staticVar = f;}

    Tuple3 operator+(const Tuple3 &d) const {
        return Tuple3(x + d.x, y + d.y, z + d.z);
    }
    Tuple3 operator+(const float d) const {
        return Tuple3(x + d, y + d, z + d);
    }
};

DECLARE_INSTANCE_TYPE(Tuple3);

/* static */ float Tuple3::staticVar;


struct Point3 : public Tuple3 {
    Point3(void) : Tuple3() {}
    Point3(float _x, float _y, float _z) : Tuple3(_x, _y, _z) {}
    virtual ~Point3() {scprintf(_SC("~Point3()\n"));}
    float getX(void) {return this->x;}
    float getX(float offset) {return offset + this->x;}
    void set(void) {Tuple3::set(0.0, 0.0, 0.0);}
};
DECLARE_INSTANCE_TYPE(Point3);


SQPLUS_TEST(Test_FunctionOverloading)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;

#ifdef SQPLUS_OVERLOAD_OPT
    OverloadGlobal<void(*)(void)>(&overloadedFunc, _SC("overloadedFunc"));
    OverloadGlobal<int(*)(int)>(&overloadedFunc, _SC("overloadedFunc"));
    OverloadGlobal<void(*)(float)>(&overloadedFunc, _SC("overloadedFunc"));
    
    SQPLUS_TEST_TRACE_SUB(overload_global_functions);
    RUN_SCRIPT(_SC("\
overloadedFunc(); \n\
overloadedFunc(100); \n\
overloadedFunc(2.0); \n\
"));
    
    RUN_SCRIPT_THROW(
        _SC("print(\"An error is expected at the next line:\"); \n\
            overloadedFunc(\"foo\");"),
        _SC("No match for given arguments")
        );

    
    SQPLUS_TEST_TRACE_SUB(overload_member_functions_and_constructors);

    SQClassDef<Tuple3>(_SC("Tuple3"))
        // bind member variables
        .var(&Tuple3::x, _SC("x"))
        .var(&Tuple3::y, _SC("y"))
        .var(&Tuple3::z, _SC("z"))

        // overload constructors
        .overloadConstructor<Tuple3(*)(void)>()
        .overloadConstructor<Tuple3(*)(float, float, float)>()
        .overloadConstructor<Tuple3(*)(Tuple3&)>()

        // overload member functions
        .overloadFunc<void(Tuple3::*)(const Tuple3&)>(&Tuple3::set, _SC("set"))
        .overloadFunc<float(Tuple3::*)(float)>(&Tuple3::set, _SC("set"))
        .overloadFunc<void(Tuple3::*)(float, float, float)>(&Tuple3::set,
                                                            _SC("set"))
        // overload static member functions
        .overloadStaticFunc<void(*)(void)>(&Tuple3::setStaticVar,
                                           _SC("setStaticVar"))
        .overloadStaticFunc<void(*)(float)>(&Tuple3::setStaticVar,
                                            _SC("setStaticVar"))

        // overload operators
        .overloadOperator<Tuple3(Tuple3::*)(const Tuple3&)const>(&Tuple3::operator+, _SC("_add"))
        .overloadOperator<Tuple3(Tuple3::*)(float)const>(&Tuple3::operator+, _SC("_add"))

        // static variable
        .staticVar(&Tuple3::staticVar, _SC("staticVar"))
        ;


    RUN_SCRIPT(_SC("\
local t3v = Tuple3(); \n\
assert(t3v.x == 1.0); \n\
assert(t3v.y == 2.0); \n\
assert(t3v.z == 3.0); \n\
\n\
local t3f = Tuple3(-11.0, 22.0, -33.0); \n\
assert(t3f.x == -11.0); \n\
assert(t3f.y == 22.0); \n\
assert(t3f.z == -33.0); \n\
\n\
local t3c = Tuple3(t3f); \n\
assert(t3c.x == t3f.x); \n\
assert(t3c.y == t3f.y); \n\
assert(t3c.z == t3f.z); \n\
\n\
t3v.set(333.0); \n\
assert(t3v.x == 333.0); \n\
assert(t3v.y == 333.0); \n\
assert(t3v.z == 333.0); \n\
\n\
t3v.set(444.0, 555.0, 666.0); \n\
assert(t3v.x == 444.0); \n\
assert(t3v.y == 555.0); \n\
assert(t3v.z == 666.0); \n\
\n\
t3v.set(t3f); \n\
assert(t3v.x == t3f.x); \n\
assert(t3v.y == t3f.y); \n\
assert(t3v.z == t3f.z); \n\
\n\
t3v.setStaticVar(); \n\
assert(t3c.staticVar == 0.0); \n\
\n\
t3f.setStaticVar(2.0); \n\
assert(t3f.staticVar == 2.0); \n\
assert(t3c.staticVar == 2.0); \n\
"));

    // overloaded operator+
    RUN_SCRIPT(_SC("\
local t3o1 = Tuple3(1111.0, 3333.0, 6666.0); \n\
local t3o2 = t3o1 + Tuple3(2222.0, 5555.0, 7777.0);\n\
assert(t3o2.x == t3o1.x + 2222.0);\n\
assert(t3o2.y == t3o1.y + 5555.0);\n\
assert(t3o2.z == t3o1.z + 7777.0);\n\
\n\
local t3o3 = t3o1 + (-1111.0);\n\
assert(t3o3.x == t3o1.x - 1111.0);\n\
assert(t3o3.y == t3o1.y - 1111.0);\n\
assert(t3o3.z == t3o1.z - 1111.0);\n\
"));
    


# ifdef SQ_USE_CLASS_INHERITANCE
    SQClassDef<Point3>(_SC("Point3"), _SC("Tuple3"))
        .overloadConstructor<Point3(*)(void)>()
        .overloadConstructor<Point3(*)(float, float, float)>()
        .overloadFunc<float (Point3::*)(void)>(&Point3::getX, _SC("getX"))
        .overloadFunc<float (Point3::*)(float)>(&Point3::getX, _SC("getX"))
        .overloadFunc<void (Point3::*)(void)>(&Point3::set, _SC("set"))
        ;

    RUN_SCRIPT(_SC("\
local p3v = Point3(); \n\
assert(p3v.x == 1.0); \n\
assert(p3v.y == 2.0); \n\
assert(p3v.z == 3.0); \n\
\n\
assert(p3v.getX() == 1.0); \n\
assert(p3v.getX(-2.0) == -1.0); \n\
\n\
p3v.set(); \n\
assert(p3v.x == 0.0); \n\
assert(p3v.y == 0.0); \n\
assert(p3v.z == 0.0); \n\
"));

    // Point3::set() shadows Tuple3::set
    RUN_SCRIPT_THROW(
        _SC("print(\"An errorr is expected at the next line:\"); \n\
            local p3v = Point3(); p3v.set(p3v);"),
        _SC("No match for given arguments")
        );
# endif // SQ_USE_CLASS_INHERITANCE
    
#else  //SQPLUS_OVERLOAD_OPT
    printf("Skipped (SQPLUS_OVERLOAD_OPT is not defined)\n");
#endif //SQPLUS_OVERLOAD_OPT
}

