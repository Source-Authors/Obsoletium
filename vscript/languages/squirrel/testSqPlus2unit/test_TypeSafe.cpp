#include "testEnv.hpp"

namespace {

class Vector3 {
public:
    Vector3(void);
    float x, y, z;
    void set(float x, float y, float z);
    Vector3 add(Vector3*);
};

class Player {
protected:
    Vector3 position;
    Vector3 velocity;
public:
    Player(void) {velocity.set(10.0, 20.0, 30.0);}
    Vector3 getVelocity(void);
};

}

DECLARE_INSTANCE_TYPE(Vector3);
DECLARE_INSTANCE_TYPE(Player);

SQPLUS_TEST(Test_TypeSafe)
{
    SQPLUS_TEST_TRACE();
    using namespace SqPlus;

    SQClassDef<Player>(_SC("Player"))
        .func(&Player::getVelocity, _SC("getVelocity"))
        ;

    SQClassDef<Vector3>(_SC("Vector3"))
        .func(&Vector3::set, _SC("set"))
        .func(&Vector3::add, _SC("_add"))
        .var(&Vector3::x, _SC("x"))
        .var(&Vector3::y, _SC("y"))
        .var(&Vector3::z, _SC("z"))
        ;

    SQPLUS_TEST_TRACE_SUB(detect_invalid_argument_type);
    RUN_SCRIPT_THROW(
        _SC("print(\"An error is expected at the next line:\"); \n\
             local v = Vector3(); \n\
             v.set(1.0, 2.0, 3.0) \n\
             local p = Player(); \n\
             \n\
             local v2 = v + p \n"),
        _SC("arith op + on between 'instance' and 'instance'")
        );

    SQPLUS_TEST_TRACE_SUB(valid_argument_type);
    RUN_SCRIPT(
        _SC("local v = Vector3(); \n\
             v.set(1.0, 2.0, 3.0) \n\
             local p = Player(); \n\
             \n\
             local v2 = v + p.getVelocity() \n\
             assert(v2.x == 11.0) \n\
             assert(v2.y == 22.0) \n\
             assert(v2.z == 33.0) \n")
        );
}




Vector3::Vector3(void) : x(0.0), y(0.0), z(0.0)
{
}

void
Vector3::set(float nx, float ny, float nz)
{
    x = nx;
    y = ny;
    z = nz;
}

Vector3
Vector3::add(Vector3 *vector)
{
    Vector3 v2;
    v2.set(x + vector->x, y + vector->y, z + vector->z);
    return v2;
}

Vector3
Player::getVelocity(void)
{
    return this->velocity;
}
