#include "vsq_vec3.h"

#include "squirrel.h"
#include "sqplus.h"

#include "tier1/fmtstr.h"

namespace {

// Vector
SQInteger vsq_constructvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = new (std::nothrow) Vector;
  if (!vec) {
    return sq_throwerror(hVM, "out of memory when allocating Vector");
  }

  int i;
  for (i = 0; i < 3 && i < sa.GetParamCount() - 1; i++) {
    (*vec)[i] = sa.GetFloat(i + 2);
  }

  for (; i < 3; i++) {
    (*vec)[i] = 0.0f;
  }

  if (const auto rc = sq_setinstanceup(hVM, 1, vec); SQ_FAILED(rc)) {
    return rc;
  }

  sq_setreleasehook(hVM, 1, &vsq_releasevec3);
  return 0;
}

SQInteger vsq_getvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  if (const char *key = sa.GetString(2); !Q_isempty(key) && !*(key + 1)) {
    int index = *key - 'x';

    if (index >= 0 && index <= 2) {
      sq_pushfloat(hVM, (*vec)[index]);
      return 1;
    }

    char err[64];
    V_sprintf_safe(err, "vector index %c should be x, y or z",
                   static_cast<char>(index));
    return sq_throwerror(hVM, err);
  }

  return sq_throwerror(hVM,
                       "vector index is null, empty or not a single character");
}

SQInteger vsq_setvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  if (const char *key = sa.GetString(2); !Q_isempty(key) && !*(key + 1)) {
    int index = *key - 'x';

    if (index >= 0 && index <= 2) {
      (*vec)[index] = sa.GetFloat(3);
      sq_pushfloat(hVM, (*vec)[index]);

      return 0;
    }

    char err[64];
    V_sprintf_safe(err, "vector index %c should be x, y or z",
                   static_cast<char>(index));
    return sq_throwerror(hVM, err);
  }

  return sq_throwerror(hVM,
                       "vector index is null, empty or not a single character");
}

SQInteger vsq_iteratevec3(HSQUIRRELVM hVM) {
  static constexpr char *results[] = {"x", "y", "z"};

  StackHandler sa(hVM);

  if (const char *key = sa.GetType(2) == OT_NULL ? "w" : sa.GetString(2);
      !Q_isempty(key) && !*(key + 1)) {
    int index = (*key - 'x') + 1;

    if (index >= 0 && index <= 2) {
      sa.Return(results[index]);
      return 1;
    }

    sq_pushnull(hVM);
    return 1;
  }

  return sq_throwerror(hVM,
                       "vector index is null, empty or not a single character");
}

SQInteger vsq_tostringvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  sa.Return((static_cast<const char *>(
      CFmtStr("(vector : (%f, %f, %f))", vec->x, vec->y, vec->z))));
  return 1;
}

SQInteger vsq_typeofvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);
  sa.Return("Vector");
  return 1;
}

SQInteger vsq_tokvstringvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  sa.Return((static_cast<const char *>(
      CFmtStr("%f %f %f))", vec->x, vec->y, vec->z))));
  return 1;
}

SQInteger vsq_addvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *left = (Vector *)sa.GetInstanceUp(1, 0);
  auto *right = (Vector *)sa.GetInstanceUp(2, 0);

  if (!left) {
    return sq_throwerror(hVM, "operator + has null left vector");
  }

  if (!right) {
    return sq_throwerror(hVM, "operator + has null right vector");
  }

  auto *vec = new Vector;
  *vec = *left + *right;

  sq_getclass(hVM, -1);
  sq_createinstance(hVM, -1);
  sq_setinstanceup(hVM, -1, vec);
  sq_setreleasehook(hVM, -1, &vsq_releasevec3);
  sq_remove(hVM, -2);

  return 1;
}

SQInteger vsq_subtractvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *left = (Vector *)sa.GetInstanceUp(1, 0);
  auto *right = (Vector *)sa.GetInstanceUp(2, 0);

  if (!left) {
    return sq_throwerror(hVM, "operator - has null left vector");
  }

  if (!right) {
    return sq_throwerror(hVM, "operator - has null right vector");
  }

  auto *vec = new Vector;
  *vec = *left - *right;

  sq_getclass(hVM, -1);
  sq_createinstance(hVM, -1);
  sq_setinstanceup(hVM, -1, vec);
  sq_setreleasehook(hVM, -1, &vsq_releasevec3);
  sq_remove(hVM, -2);

  return 1;
}

SQInteger vsq_scalevec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *src = (Vector *)sa.GetInstanceUp(1, 0);
  if (!src) {
    return sq_throwerror(hVM, "null vector");
  }

  float scale = sa.GetFloat(2);

  auto *vec = new Vector;
  *vec = *src * scale;

  sq_getclass(hVM, -2);
  sq_createinstance(hVM, -1);
  sq_setinstanceup(hVM, -1, vec);
  sq_setreleasehook(hVM, -1, &vsq_releasevec3);
  sq_remove(hVM, -2);

  return 1;
}

SQInteger vsq_lengthvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  float flLength = vec->Length();
  sa.Return(flLength);

  return 1;
}

SQInteger vsq_lengthsqvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  float flLength = vec->LengthSqr();
  sa.Return(flLength);

  return 1;
}

SQInteger vsq_length2dvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  float flLength = vec->Length2D();
  sa.Return(flLength);

  return 1;
}

SQInteger vsq_length2dsqrvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "null vector");
  }

  float flLength = vec->Length2DSqr();
  sa.Return(flLength);

  return 1;
}

SQInteger vsq_crossvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *left = (Vector *)sa.GetInstanceUp(1, 0);
  auto *right = (Vector *)sa.GetInstanceUp(2, 0);

  if (!left) {
    return sq_throwerror(hVM, "Cross has null left vector");
  }

  if (!right) {
    return sq_throwerror(hVM, "Cross has null right vector");
  }

  auto *vec = new Vector;
  *vec = left->Cross(*right);

  sq_getclass(hVM, -1);
  sq_createinstance(hVM, -1);
  sq_setinstanceup(hVM, -1, vec);
  sq_setreleasehook(hVM, -1, &vsq_releasevec3);
  sq_remove(hVM, -2);

  return 1;
}

SQInteger vsq_dotvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *left = (Vector *)sa.GetInstanceUp(1, 0);
  auto *right = (Vector *)sa.GetInstanceUp(2, 0);

  if (!left) {
    return sq_throwerror(hVM, "Dot has null left vector");
  }

  if (!right) {
    return sq_throwerror(hVM, "Dot has null right vector");
  }

  float flResult = (*left).Dot(*right);
  sa.Return(flResult);

  return 1;
}

SQInteger vsq_normvec3(HSQUIRRELVM hVM) {
  StackHandler sa(hVM);

  auto *vec = (Vector *)sa.GetInstanceUp(1, 0);
  if (!vec) {
    return sq_throwerror(hVM, "Norm has null vector");
  }

  float flLength = vec->NormalizeInPlace();
  sa.Return(flLength);

  return 1;
}

static constexpr SQRegFunction Vec3Functions[] = {
    {"constructor", vsq_constructvec3, 0, nullptr},
    {"_get", vsq_getvec3, 2, ".."},
    {"_set", vsq_setvec3, 3, "..n"},
    {"_tostring", vsq_tostringvec3, 0, nullptr},
    {"_typeof", vsq_typeofvec3, 0, nullptr},
    {"_nexti", vsq_iteratevec3, 0, nullptr},
    {"_add", vsq_addvec3, 2, nullptr},
    {"_sub", vsq_subtractvec3, 2, nullptr},
    {"_mul", vsq_scalevec3, 2, nullptr},
    {"ToKVString", vsq_tokvstringvec3, 0, nullptr},
    {"Length", vsq_lengthvec3, 0, nullptr},
    {"LengthSqr", vsq_lengthsqvec3, 0, nullptr},
    {"Length2D", vsq_length2dvec3, 0, nullptr},
    {"Length2DSqr", vsq_length2dsqrvec3, 0, nullptr},
    {"Dot", vsq_dotvec3, 2, nullptr},
    {"Cross", vsq_crossvec3, 2, nullptr},
    {"Norm", vsq_normvec3, 0, nullptr},
};

}  // namespace

SQInteger vsq_openvec3(HSQUIRRELVM hVM, HSQOBJECT *hExternalClass) {
  SQInteger top = sq_gettop(hVM);

  sq_pushroottable(hVM);
  sq_pushstring(hVM, TYPENAME_VECTOR, -1);

  if (const auto rc = sq_newclass(hVM, 0); SQ_FAILED(rc)) {
    sq_settop(hVM, top);
    DMsg("vscript:squirrel", 0, "Unable to register %s class in %s VM.\n",
         TYPENAME_VECTOR, SQUIRREL_VERSION);
    return rc;
  }

  HSQOBJECT hClass;
  if (const auto rc = sq_getstackobj(hVM, -1, &hClass); SQ_FAILED(rc)) {
    sq_settop(hVM, top);
    DMsg("vscript:squirrel", 0, "Unable to register %s class in %s VM.\n",
         TYPENAME_VECTOR, SQUIRREL_VERSION);
    return rc;
  }
  if (const auto rc = sq_settypetag(hVM, -1, TYPETAG_VECTOR); SQ_FAILED(rc)) {
    sq_settop(hVM, top);
    DMsg("vscript:squirrel", 0, "Unable to register %s class in %s VM.\n",
         TYPENAME_VECTOR, SQUIRREL_VERSION);
    return rc;
  }

  sq_createslot(hVM, -3);

  sq_pushobject(hVM, hClass);
  for (auto &func : Vec3Functions) {
    sq_pushstring(hVM, func.name, -1);
    sq_newclosure(hVM, func.f, 0);

    if (func.nparamscheck)
      sq_setparamscheck(hVM, func.nparamscheck, func.typemask);

    sq_setnativeclosurename(hVM, -1, func.name);
    sq_createslot(hVM, -3);
  }

  sq_pop(hVM, 1);

  sq_settop(hVM, top);

  {
    sq_pushroottable(hVM);
    sq_pushstring(hVM, TYPENAME_VECTOR, -1);

    Assert(hExternalClass);

    sq_get(hVM, -2);  // get the function from the root table
    sq_getstackobj(hVM, -1, hExternalClass);
    sq_addref(hVM, hExternalClass);
    sq_pop(hVM, 2);
  }

  return SQ_OK;
}

SQInteger vsq_releasevec3(SQUserPointer p, SQInteger size) {
  delete (Vector *)p;
  return SQ_OK;
}

SQInteger vsq_getvec3(HSQUIRRELVM hVM, StackHandler &sh, intp idx, Vector &vec) {
  auto *instance = (Vector *)sh.GetInstanceUp(idx, TYPETAG_VECTOR);
  if (instance) {
    vec = *instance;
    return SQ_OK;
  }

  return sq_throwerror(hVM, "Vector argument expected");
}