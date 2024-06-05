// Copyright Valve Corporation, All rights reserved.
//
// This should contain ONLY general purpose macros that are
// appropriate for use in engine/launcher/all tools.

#ifndef TIER0_COMMONMACROS_H_
#define TIER0_COMMONMACROS_H_

#include <cassert>  // assert
#include <cstddef>  // memcmp
#include <cstring>  // strlen
#include <utility>  // to_underlying

#if defined(__x86_64__) || defined(_WIN64)
/**
 * @brief Defined when x86-64 CPU architecture used.
 */
#define PLATFORM_64BITS 1
#endif

#if defined(__GCC__) || defined(__GNUC__)
/**
 * @brief Defined when GCC compiler used.
 */
#define COMPILER_GCC 1
#endif

#ifdef __clang__
/**
 * @brief Defined when clang compiler used.
 */
#define COMPILER_CLANG 1
#endif

#if defined(_MSC_VER) && !defined(COMPILER_MSVC)
/**
 * @brief Defined when MSVC compiler used.
 */
#define COMPILER_MSVC 1
#endif

/**
 * @brief Makes a signed 4-byte "packed ID" int out of 4 characters.
 * @param d
 * @param c
 * @param b
 * @param a
 * @return Packed ID.
 */
constexpr inline int MAKEID(char d, char c, char b, char a) noexcept {
  return (static_cast<int>(a) << 24) | (static_cast<int>(b) << 16) |
         (static_cast<int>(c) << 8) | static_cast<int>(d);
}

/**
 * @brief Makes a unsigned 4-byte "packed ID" int out of 4 characters.
 * @param d
 * @param c
 * @param b
 * @param a
 * @return Packed ID.
 */
constexpr inline unsigned MAKEUID(char d, char c, char b, char a) noexcept {
  return (static_cast<unsigned>(a) << 24) | (static_cast<unsigned>(b) << 16) |
         (static_cast<unsigned>(c) << 8) | static_cast<unsigned>(d);
}

/**
 * @brief Compares a string with a 4-byte packed signed ID constant.
 * @param s String.
 * @param id Packed ID.
 * @return true if string is packed signed ID.
 */
inline bool STRING_MATCHES_ID(const char* s, int id) noexcept {
  assert(strlen(s) >= sizeof(id));
  return memcmp(s, &id, sizeof(id)) == 0;
}

/**
 * @brief Compares a string with a 4-byte packed unsigned ID constant.
 * @param s String.
 * @param id Packed ID.
 * @return true if string is packed unsigned ID.
 */
inline bool STRING_MATCHES_ID(const char* s, unsigned id) noexcept {
  assert(strlen(s) >= sizeof(id));
  return memcmp(s, &id, sizeof(id)) == 0;
}

/**
 * @brief Convert ID to string. Deprecated.
 * @tparam T
 * @param id Id.
 * @param p String.
 */
template <typename T>
[[deprecated("Unsafe buffer access.")]] void ID_TO_STRING(
    T id, const char* p) noexcept {
  p[3] = (id >> 24) & 0xFF;
  p[2] = (id >> 16) & 0xFF;
  p[1] = (id >> 8) & 0xFF;
  p[0] = (id >> 0) & 0xFF;
}

/**
 * @brief Set bits in bit vector.
 * @tparam T
 * @tparam Y
 * @param vector Vector.
 * @param bits Bits.
 * @return Bit vector with set bits.
 */
template <typename T, typename Y>
constexpr inline auto& SETBITS(T& vector, Y bits) noexcept {
  return vector |= bits;
}

/**
 * @brief Clear bits in vector.
 * @tparam T
 * @tparam Y
 * @param vector Vector.
 * @param bits Bits.
 * @return Bit vector with cleared bits.
 */
template <typename T, typename Y>
constexpr inline auto& CLEARBITS(T& vector, Y bits) noexcept {
  return vector &= ~bits;
}

/**
 * @brief Check bits set in vector.
 * @tparam T
 * @tparam Y
 * @param vector Vector.
 * @param bits Bits.
 * @return Bit mask with set bits.
 */
template <typename T, typename Y>
constexpr inline auto FBitSet(T vector, Y bits) noexcept {
  return vector & bits;
}

/**
 * @brief Check value is power of two.
 * @tparam T
 * @param value Value.
 * @return true if value is power of two.
 */
template <typename T>
constexpr inline bool IsPowerOfTwo(T value) noexcept {
  return (value & (value - static_cast<T>(1))) == T{};
}

// dimhotepus: ssize support.
#ifdef __cpp_lib_ssize
// C++20 ssize
using std::ssize;
#else
#include <type_traits>

template <class C>
constexpr auto ssize(const C& c) noexcept(noexcept(c.size()))
    -> std::common_type_t<std::ptrdiff_t,
                          std::make_signed_t<decltype(c.size())>> {
  using R = std::common_type_t<std::ptrdiff_t,
                               std::make_signed_t<decltype(c.size())>>;
  return static_cast<R>(c.size());
}

template <class T, std::ptrdiff_t N>
constexpr std::ptrdiff_t ssize(const T (&)[N]) noexcept {
  return N;
}
#endif

#ifdef __cpp_lib_to_underlying
// C++23 to_underlying
using std::to_underlying;
#else
template <typename T>
[[nodiscard]] constexpr std::enable_if_t<std::is_enum_v<T>,
                                         std::underlying_type_t<T>>
to_underlying(T value) noexcept {
  return static_cast<std::underlying_type_t<T>>(value);
}
#endif

#ifndef REFERENCE
#define REFERENCE(arg) ((void)arg)
#endif

// Wraps the integer in quotes, allowing us to form constant strings with it.
#define CONST_INTEGER_AS_STRING(x) #x

//__LINE__ can only be converted to an actual number by going through
// this, otherwise the output is literally "__LINE__".
#define __HACK_LINE_AS_STRING__(x) CONST_INTEGER_AS_STRING(x)

// Gives you the line number in constant string form.
#define __LINE__AS_STRING __HACK_LINE_AS_STRING__(__LINE__)

// Using ARRAYSIZE implementation from winnt.h:
#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif

// Return the number of elements in a statically sized array.
//   DWORD Buffer[100];
//   RTL_NUMBER_OF(Buffer) == 100
// This is also popularly known as: NUMBER_OF, ARRSIZE, _countof, NELEM, etc.
//
#ifndef RTL_NUMBER_OF_V0
#define RTL_NUMBER_OF_V0(A) (sizeof(A) / sizeof((A)[0]))
#endif

#if defined(__cplusplus) && !defined(MIDL_PASS) && !defined(RC_INVOKED) && \
    (_MSC_FULL_VER >= 13009466) && !defined(SORTPP_PASS)

// From crtdefs.h
#if !defined(UNALIGNED)
#if defined(_M_IA64) || defined(_M_AMD64)
#define UNALIGNED __unaligned
#else
#define UNALIGNED
#endif
#endif

// RtlpNumberOf is a function that takes a reference to an array of N Ts.
//
// typedef T array_of_T[N];
// typedef array_of_T &reference_to_array_of_T;
//
// RtlpNumberOf returns a pointer to an array of N chars.
// We could return a reference instead of a pointer but older compilers do not
// accept that.
//
// typedef char array_of_char[N];
// typedef array_of_char *pointer_to_array_of_char;
//
// sizeof(array_of_char) == N
// sizeof(*pointer_to_array_of_char) == N
//
// pointer_to_array_of_char RtlpNumberOf(reference_to_array_of_T);
//
// We never even call RtlpNumberOf, we just take the size of dereferencing its
// return type. We do not even implement RtlpNumberOf, we just decare it.
//
// Attempts to pass pointers instead of arrays to this macro result in compile
// time errors. That is the point.
extern "C++"  // templates cannot be declared to have 'C' linkage
    template <typename T, size_t N>
    char (*RtlpNumberOf(UNALIGNED T (&)[N]))[N];

#ifdef _PREFAST_
// The +0 is so that we can go:
// size = ARRAYSIZE(array) * sizeof(array[0]) without triggering a /analyze
// warning about multiplying sizeof.
#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)) + 0)
#else
#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)))
#endif

// This does not work with:
//
// void Foo()
// {
//    struct { int x; } y[2];
//    RTL_NUMBER_OF_V2(y); // illegal use of anonymous local type in template
//    instantiation
// }
//
// You must instead do:
//
// struct Foo1 { int x; };
//
// void Foo()
// {
//    Foo1 y[2];
//    RTL_NUMBER_OF_V2(y); // ok
// }
//
// OR
//
// void Foo()
// {
//    struct { int x; } y[2];
//    RTL_NUMBER_OF_V0(y); // ok
// }
//
// OR
//
// void Foo()
// {
//    struct { int x; } y[2];
//    _ARRAYSIZE(y); // ok
// }

#else
#define RTL_NUMBER_OF_V2(A) RTL_NUMBER_OF_V1(A)
#endif

// ARRAYSIZE is more readable version of RTL_NUMBER_OF_V2.
#define ARRAYSIZE(A) RTL_NUMBER_OF_V2(A)

// dimhotepus: Try to hide this one. Looks like not used anymore.
// _ARRAYSIZE is a version useful for anonymous types.
// #define _ARRAYSIZE(A) RTL_NUMBER_OF_V0(A)

// dimhotepus: Deprecated.
// #define Q_ARRAYSIZE(p) ARRAYSIZE(p)

// dimhotepus: Deprecated.
// #define V_ARRAYSIZE(p) ARRAYSIZE(p)

/**
 * @brief Clamp array index to be in bounds.
 * @tparam IndexType
 * @tparam T
 * @tparam N Array size.
 * @param buffer Array.
 * @param index Index to clamp.
 * @return Clamped array index.
 */
template <typename IndexType, typename T, size_t N>
constexpr IndexType ClampedArrayIndex([[maybe_unused]] const T (&buffer)[N],
                                      IndexType index) noexcept {
  return clamp(index, 0, static_cast<IndexType>(N) - 1);
}

/**
 * @brief Get array element by index. Clamp index if out of range.
 * @tparam T
 * @tparam N Array size.
 * @param buffer Array.
 * @param index Index to clamp.
 * @return Array element.
 */
template <typename T, size_t N>
constexpr T ClampedArrayElement(const T (&buffer)[N], size_t index) noexcept {
  // Put index in an unsigned type to halve the clamping.
  if (index >= N) index = N - 1;

  return buffer[index];
}

// MSVC specific.
#ifdef COMPILER_MSVC
/*
 * @brief Begins MSVC warning override scope.
 */
#define MSVC_BEGIN_WARNING_OVERRIDE_SCOPE() __pragma(warning(push))

/*
 * @brief Disables MSVC warning.
 */
#define MSVC_DISABLE_WARNING(warning_level) \
  __pragma(warning(disable : warning_level))

/*
 * @brief Ends MSVC warning override scope.
 */
#define MSVC_END_WARNING_OVERRIDE_SCOPE() __pragma(warning(pop))

/*
 * @brief Disable MSVC warning for code.
 */
#define MSVC_SCOPED_DISABLE_WARNING(warning_level, code) \
  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()                    \
  MSVC_DISABLE_WARNING(warning_level)                    \
  code MSVC_END_WARNING_OVERRIDE_SCOPE()
#endif

#if defined(__clang__) || defined(__GCC__)
/*
 * @brief Begins GCC / Clang warning override scope.
 */
#define SRC_GCC_BEGIN_WARNING_OVERRIDE_SCOPE() _Pragma("GCC diagnostic push")

/*
 * @brief Disables GCC / Clang overloaded-virtual.
 */
#define SRC_GCC_DISABLE_OVERLOADED_VIRTUAL_WARNING() \
  _Pragma("GCC diagnostic ignored \"-Woverloaded-virtual\"")

/*
 * @brief Ends GCC / Clang warning override scope.
 */
#define SRC_GCC_END_WARNING_OVERRIDE_SCOPE() _Pragma("GCC diagnostic pop")
#else
/*
 * @brief Do nothing.
 */
#define SRC_GCC_BEGIN_WARNING_OVERRIDE_SCOPE()

/*
 * @brief Do nothing.
 */
#define SRC_GCC_DISABLE_OVERLOADED_VIRTUAL_WARNING()

/*
 * @brief Do nothing.
 */
#define SRC_GCC_END_WARNING_OVERRIDE_SCOPE()
#endif

#endif  // TIER0_COMMONMACROS_H_
