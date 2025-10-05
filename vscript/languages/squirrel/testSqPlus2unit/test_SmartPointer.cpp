#include "testEnv.hpp"

namespace {

class Data {
public:
	int var;

	Data() {
		var = 5;
	}

	void func() {
		scprintf(_SC("data.func() data.var = %d\n"), var);
	}
};

Data data;

class SmartPointer {
public:
	Data* pointee;

	SmartPointer() {
		pointee = &data;
	}

	void func() {
		scprintf(_SC("pointer.func()\n"));
	}

	Data* get() {
		return pointee;
	}
};

} // namespace

DECLARE_INSTANCE_TYPE(SmartPointer);


SQPLUS_TEST(Test_SmartPointer)
{
	SQPLUS_TEST_TRACE();

	using namespace SqPlus;

#ifdef SQPLUS_SMARTPOINTER_OPT
	// === BEGIN SmartPointer Test ===

	SQClassDef<SmartPointer>(_SC("SmartPointer"))
		.func(&SmartPointer::func, _SC("func"))
		.smartFunc<Data>(&Data::func, _SC("dataFunc"))
		.smartVar<Data>(&Data::var, _SC("dataVar"))
		;

	SQPLUS_TEST_TRACE_SUB(testSmartPointer);
	RUN_SCRIPT(_SC("\
				  local ptr = SmartPointer(); \n\
				  ptr.func(); \n\
				  ptr.dataFunc(); \n\
				  ptr.dataVar = 10; \n\
				  ptr.dataFunc(); \n\
				  "));

	CHECK_EQUAL(data.var, 10);
#else
    printf("Skipped (SQPLUS_SMARTPOINTER_OPT is not defined)\n");
#endif
	// === END SmartPointer Test ===
}

unsigned char* getSmartPointerPointee(unsigned char *up) {
	return reinterpret_cast<unsigned char*>(reinterpret_cast<SmartPointer*>(up)->get());
}

