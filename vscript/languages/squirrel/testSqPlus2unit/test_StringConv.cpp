#include "testEnv.hpp"

#ifdef SQPLUS_AUTOCONVERT_OTHER_CHAR

# include "../sqplus/SqPlusOCharBuf.h"

namespace {
class StringConvTest {
public:
    StringConvTest() {}
	int AsciiArgFunc( const char *arg1 ){ 
		m_a1 = arg1;
		return arg1 ? (int)strlen(arg1) : 0;
	}

	int WideArgFunc( const wchar_t *arg2 ){ 
		m_a2 = arg2;
		return arg2 ? (int)wcslen(arg2) : 0;
	}

	const char *GetArg1() { return m_a1; }
	const wchar_t *GetArg2() { return m_a2; }

	SQACharBuf m_a1;
	SQWCharBuf m_a2;
};
}
DECLARE_INSTANCE_TYPE(StringConvTest);

static bool
checkElems_8bit(const char *prefix,
                const char *str, const unsigned char *ref)
{
    bool result = true;
    printf("%s\t[ ", prefix);
    for (const char *p = str; *p; ++p, ++ref) {
        printf("%02x ", *p & 0xff);
        if (*reinterpret_cast<const unsigned char*>(p) != *ref) {
            printf(" (<- expected %02x) ", *ref & 0xff);
            result = false;
        }
    }
    printf("]\n");
    return result;
}

static bool
checkElems_16bit(const char *prefix,
                 const wchar_t *wstr, const unsigned char *ref)
{
    bool result = true;
    printf("%s\t[ ", prefix);
    for (const wchar_t *p = wstr; *p; ++p) {
        unsigned char msb = (*p & 0xff00) >> 8;
        unsigned char lsb = (*p & 0x00ff);
        printf("%02x%02x ", msb, lsb);
        
        if (msb != *(ref + 1) || lsb != *ref) {
            printf(" (<- expected %02x%02x) ", *(ref + 1) & 0xff, *ref & 0xff);
            result = false;
        }
        ref += 2;
    }
    printf("]\n");
    return result;
}

#if 0
static bool
checkElems_32bit(const char *prefix,
                 const wchar_t *wstr, const unsigned char *ref)
{
    bool result = true;
    printf("%s\t[ ", prefix);
    for (const wchar_t *p = wstr; *p; ++p) {
        unsigned char b0 = (*p & 0xff000000) >> 24;
        unsigned char b1 = (*p & 0x00ff0000) >> 16;
        unsigned char b2 = (*p & 0x0000ff00) >> 8;
        unsigned char b3 = (*p & 0x000000ff);
        printf("%02x%02x%02x%02x ", b0, b1, b2, b3);
        
        if (b0 != *(ref + 3)||
            b1 != *(ref + 2) ||
            b2 != *(ref + 1) ||
            b3 != *(ref + 0)) {
            printf(" (<- expected %02x%02x%02x%02x) ",
                   *(ref + 3)& 0xff,
                   *(ref + 2) & 0xff,
                   *(ref + 1) & 0xff,
                   *(ref + 0) & 0xff);
            result = false;
        }
        ref += 4;
    }
    printf("]\n");
    return result;
}
#endif

static const unsigned char testString_utf_8[] = {
    0x61,                       // 'a'
    0x62,                       // 'b'
    0x63,                       // 'c'
    0x5f,                       // '_'
    0xe2, 0x82, 0xac,           // Unicode Character 'EURO SIGN'
    0x5f,                       // '_'
    0x33,                       // '3'
    0x5f,                       // '_'
    0xc2, 0xa3,                 // Unicode Character 'POUND SIGN'
    0x5f,                       // '_'
	0x00                        // NUL
};

static const unsigned char testString_latin_1[] = {
    0x61,                       // 'a'
    0x62,                       // 'b'
    0x63,                       // 'c'
    0x5f,                       // '_'
    0x3f,                       // '?' (implies broken in conversion)
    0x5f,                       // '_'
    0x33,                       // '3'
    0x5f,                       // '_'
    0xa3,                       // Latin-1 Character 'POUND SIGN'
    0x5f,                       // '_'
	0x00                        // NUL
};

static const unsigned char testString_utf_16_le[] = {
    0x61, 0x00,                 // 'a'                           
    0x62, 0x00,                 // 'b'                           
    0x63, 0x00,                 // 'c'                           
    0x5f, 0x00,                 // '_'                           
    0xac, 0x20,                 // Unicode Character 'EURO SIGN' 
    0x5f, 0x00,                 // '_'                           
    0x33, 0x00,                 // '3'                           
    0x5f, 0x00,                 // '_'                           
    0xa3, 0x00,                 // Unicode Character 'POUND SIGN'
    0x5f, 0x00,                 // '_'
	0x00, 0x00                  // NUL
};


static const unsigned char brokenString_utf_8[] = {
    0x61,                       // 'a'
    0x62,                       // 'b'
    0x63,                       // 'c'
    0x5f,                       // '_'
    0x20, 0xac,                 // Invalid UTF-8 Character
    0x5f,                       // '_'
    0x33,                       // '3'
    0x5f,                       // '_'
    0xc2, 0xa3,                 // Unicode Character 'POUND SIGN'
    0x5f,                       // '_'
	0x00                        // NUL
};

static const unsigned char brokenString_utf_16_le[] = {
    0x61, 0x00,                 // 'a'                           
    0x62, 0x00,                 // 'b'                           
    0x63, 0x00,                 // 'c'                           
    0x5f, 0x00,                 // '_'                           
    0x20, 0x00,                 // ' ' first byte of Invalid UTF-8
    0xff, 0xff,                 // broken character
    0x5f, 0x00,                 // '_'                           
    0x33, 0x00,                 // '3'                           
    0x5f, 0x00,                 // '_'                           
    0xa3, 0x00,                 // Unicode Character 'POUND SIGN'
    0x5f, 0x00,                 // '_'
	0x00, 0x00                  // NUL
};


static const unsigned char testString2_latin_1[] = {
    0x61,                       // 'a'
    0x62,                       // 'b'
    0x63,                       // 'c'
    0x5f,                       // '_'
    0xa5,                       // Latin-1 Character 'YEN SIGN'
    0x5f,                       // '_'
    0x33,                       // '3'
    0x5f,                       // '_'
    0xa3,                       // Latin-1 Character 'POUND SIGN'
    0x5f,                       // '_'
    0x00,                       // NUL
};

static const unsigned char testString2_utf_16_le[] = {
    0x61, 0x00,                 // 'a'                           
    0x62, 0x00,                 // 'b'                           
    0x63, 0x00,                 // 'c'                           
    0x5f, 0x00,                 // '_'
    0xa5, 0x00,                 // Unicode Character 'YEN SIGN'
    0x5f, 0x00,                 // '_'                           
    0x33, 0x00,                 // '3'                           
    0x5f, 0x00,                 // '_'                           
    0xa3, 0x00,                 // Unicode Character 'POUND SIGN'
    0x5f, 0x00,                 // '_'
	0x00, 0x00                  // NUL
};



#endif // SQPLUS_AUTOCONVERT_OTHER_CHAR


SQPLUS_TEST(Test_StringConv)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;
    SquirrelObject root = SquirrelVM::GetRootTable();

	// A Squirrel helper function to print an array of elems
# if defined(SQUNICODE)
    RUN_SCRIPT(_SC("\n\
		function PrintElems16(name, v){ \n\
			local s = name + \"\\t[ \" \n\
			foreach( e in v ) s += format(\"%04x \", e & 0xffff) \n\
            s+=\"]\\n\" \n\
			print(s) \n\
		} \n\
		PrintElems16(\"car\", \"car\") \n\
	"));
# else
    RUN_SCRIPT(_SC("\n\
		function PrintElems8(name, v){ \n\
			local s = name + \"\\t[ \" \n\
			foreach( e in v ) s += format(\"%02x \", e & 0xff) \n\
            s+=\"]\\n\" \n\
			print(s) \n\
		} \n\
		PrintElems8(\"car\", \"car\") \n\
	"));
#endif
    

#ifdef SQPLUS_AUTOCONVERT_OTHER_CHAR 

	printf( "SQPLUS_USE_LATIN1:%d\n", SQPLUS_USE_LATIN1 );

    SQClassDef<StringConvTest>(_SC("StringConvTest"))
        .func(&StringConvTest::AsciiArgFunc, _SC("AsciiArgFunc"))
        .func(&StringConvTest::WideArgFunc, _SC("WideArgFunc"))
        .func(&StringConvTest::GetArg1, _SC("GetArg1"))
        .func(&StringConvTest::GetArg2, _SC("GetArg2"))
        ;

    // Test that basic string (only 7-bit chars) can be set and read
    // back for both Ascii and wide strings
    SQPLUS_TEST_TRACE_SUB(7bit_chars);
    RUN_SCRIPT(_SC("\n\
		print(\"StringConvTest - with only ASCII characters\\n\"); \
		local a = StringConvTest(); \n\
		a.AsciiArgFunc(\"Hello\"); \n\
		print(a.GetArg1()); \n\
		a.WideArgFunc(\"World\"); \n\
		print(a.GetArg2()); \n\
		assert(a.GetArg1()==\"Hello\"); \n\
		assert(a.GetArg2()==\"World\"); \n\
	"));

    printf("-- \n\n");
    RUN_SCRIPT(_SC("print(\"StringConvTest - with non-ASCII characters\\n\");"));
    //
	// This script tests conversion with strings that use more than 7 bits.
	// The character \u20AC is the Euro sign (outside of Latin1 code page)
    //
# if defined(SQUNICODE) && SQPLUS_USE_LATIN1 == 0
    // (case 1)
    // SQChar is wchar_t, and 8 bit strings are UTF-8.
	// Conversions should work.
    SQPLUS_TEST_TRACE_SUB(With_SQUNICODE);
    RUN_SCRIPT(L"dofile(\"test_StringConv_case1.nut\", true)");

    SquirrelObject a1 = root.GetValue(_SC("a1"));
    StringConvTest *a1p = static_cast<StringConvTest*>(
        a1.GetInstanceUP(ClassType<StringConvTest>::type())
        );
    CHECK(checkElems_8bit("a1p->GetArg1()",
                          a1p->GetArg1(), testString_utf_8));
    CHECK(checkElems_16bit("a1p->GetArg2()",
                           a1p->GetArg2(), testString_utf_16_le));
    
# elif defined(SQUNICODE) && SQPLUS_USE_LATIN1 == 1
    // (case 2)
    // SQChar is wchar_t, and 8 bit strings are Latin1.
	// Going from wchar_t => char => wchar_t will not work.
    SQPLUS_TEST_TRACE_SUB(With_SQUNICODE_LATIN1);
    RUN_SCRIPT(L"dofile(\"test_StringConv_case2.nut\", true)");

    SquirrelObject a2 = root.GetValue(_SC("a2"));
    StringConvTest *a2p = static_cast<StringConvTest*>(
        a2.GetInstanceUP(ClassType<StringConvTest>::type())
        );
    CHECK(checkElems_8bit("a2p->GetArg1()",
                          a2p->GetArg1(), testString_latin_1));
    CHECK(checkElems_16bit("a2p->GetArg2()",
                           a2p->GetArg2(), testString_utf_16_le));
    
# elif !defined(SQUNICODE) && SQPLUS_USE_LATIN1 == 0
    // (case 3)
    // SQChar is char, and 8 bit strings are treated as UTF-8 in conversion.
    // Conversion from char=>wchar_t fails when the string is not valid UTF-8.
	// This mode could be useful when one knows that all 8-bit strings are
    // valid UTF-8.
    SQPLUS_TEST_TRACE_SUB(Without_SQUNICODE);
    RUN_SCRIPT("dofile(\"test_StringConv_case3.nut\", true);");

    SquirrelObject a3 = root.GetValue(_SC("a3"));
    StringConvTest *a3p = static_cast<StringConvTest*>(
        a3.GetInstanceUP(ClassType<StringConvTest>::type())
        );
    CHECK(checkElems_8bit("a3p->GetArg1()",
                          a3p->GetArg1(), testString_utf_8));
    
    CHECK(checkElems_16bit("a3p->GetArg2()",
                           a3p->GetArg2(), testString_utf_16_le));
    


    SquirrelObject a3b = root.GetValue(_SC("a3b"));
    StringConvTest *a3bp = static_cast<StringConvTest*>(
        a3b.GetInstanceUP(ClassType<StringConvTest>::type())
        );
    CHECK(checkElems_8bit("a3bp->GetArg1()",
                           a3bp->GetArg1(), brokenString_utf_8));
    
    CHECK(checkElems_16bit("a3bp->GetArg2()",
                           a3bp->GetArg2(), brokenString_utf_16_le));
    
    
# elif !defined(SQUNICODE) && SQPLUS_USE_LATIN1 == 1
    // (case 4)
    // SQChar is char, and 8 bit strings treated like Latin1.
    SQPLUS_TEST_TRACE_SUB(Without_SQUNICODE_LATIN1);
    RUN_SCRIPT("dofile(\"test_StringConv_case4.nut\", true);");

    SquirrelObject a4 = root.GetValue(_SC("a4"));
    StringConvTest *a4p = static_cast<StringConvTest*>(
        a4.GetInstanceUP(ClassType<StringConvTest>::type())
        );
    CHECK(checkElems_8bit("a4p->GetArg1()",
                          a4p->GetArg1(), testString2_latin_1));
    
    CHECK(checkElems_16bit("a4p->GetArg2()",
                           a4p->GetArg2(), testString2_utf_16_le));


	// Conversion wchar_t => char won't work for characters outside of latin-1.
    a4p->WideArgFunc((wchar_t*)testString_utf_16_le); // containing 'EURO SIGN'
    RUN_SCRIPT("test4();");
# endif

    
#else // !defined(SQPLUS_AUTOCONVERT_OTHER_CHAR)
    printf("Skipped (SQPLUS_AUTOCONVERT_OTHER_CHAR is not defined)\n");
#endif 
}
