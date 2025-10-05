// (case 4)
// SQChar is char, and 8 bit strings treated like Latin1.
//
// Conversion wchar_t => char will not work for characters
// outside of latin-1.

print("(case 4) !SQUNICODE, LATIN1\n");
a4 <- StringConvTest();
local test_str2 = "abc_¥_3_£_";
PrintElems8("test_str2", test_str2);

a4.AsciiArgFunc(test_str2);
a4.WideArgFunc(test_str2);
assert(a4.GetArg1() == test_str2);
assert(a4.GetArg2() == test_str2);


// test for conversion
function test4() {
    PrintElems8("a4.GetArg2(): ", a4.GetArg2());
    assert(a4.GetArg2() == "abc_?_3_£_");
}

/*
 * Local Variables:
 * coding: latin-1
 * End:
 */
