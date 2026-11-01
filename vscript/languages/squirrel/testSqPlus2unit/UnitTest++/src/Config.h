#ifndef UNITTEST_CONFIG_H
#define UNITTEST_CONFIG_H

// Standard defines documented here: http://predef.sourceforge.net

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(linux) || \
    defined(__APPLE__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (__FreeBSD__)
    #define UNITTEST_POSIX
#endif

// by default, MemoryOutStream is implemented in terms of std::ostringstream.
// uncomment this line to use the custom MemoryOutStream (no deps on std::ostringstream).
//#define UNITTEST_USE_CUSTOM_STREAMS

#endif
