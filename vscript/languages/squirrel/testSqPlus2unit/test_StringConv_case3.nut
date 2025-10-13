// (case 3)
// SQChar is char, and 8 bit strings are treated as UTF-8 in conversion.
// This mode could be useful when one knows that all 8-bit strings are
// valid UTF-8.

print("(case 3) !SQUNICODE, UTF8\n");

local test_str = "abc_€_3_£_"; // valid UTF-8
PrintElems8("test_str", test_str);

a3 <- StringConvTest();
a3.AsciiArgFunc(test_str);     // test_str will be stored as it is.
a3.WideArgFunc(test_str);
assert(a3.GetArg1() == test_str);
assert(a3.GetArg2() == test_str);

/*
  Conversion from char => wchar_t fails when the string is not valid UTF-8.
 */

a3b <- StringConvTest();
local broken_str = "abc_\x20\xAC_3_£_"; // broken_str is 8-bit string
                                        //  containing not valid UTF-8
PrintElems8("broken_str", broken_str);

a3b.AsciiArgFunc(broken_str); // broken_str will be stored as it is.
a3b.WideArgFunc(broken_str);  // broken_str will be converted to 16-bit string,
                              //  and broken character should be lost.

assert(a3b.GetArg1() == broken_str);
assert(!(a3b.GetArg2() == broken_str));



/*
 * Local Variables:
 * coding: utf-8
 * End:
 */

