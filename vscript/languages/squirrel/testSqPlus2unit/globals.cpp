#include <sqplus.h>
#include <stdio.h>

#include "globals.hpp"

int globalVar = 5551234;

GlobalClass globalClass;

void
globalFunc(const SQChar *s, int val)
{
    scprintf(_SC("globalFunc(s, val): s: %s val: %d\n"), s, val);
}

