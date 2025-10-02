# C++ style guide

## Table of contents

* [Guides and references](#guides-and-references)
* [Formatting](#formatting)
  * [Use `nullptr` instead of `NULL` or `0`](#use-nullptr-instead-of-null-or-0)
  * [Ownership and smart pointers](#ownership-and-smart-pointers)
  * [Avoid non-const references](#avoid-non-const-references)
* [Others](#others)
  * [Type casting](#type-casting)
  * [Using `auto`](#using-auto)
  * [Do not include `*.h` if `*-inl.h` has already been included](#do-not-include-h-if--inlh-has-already-been-included)

## Guides and references

The Obsoletium C++ codebase strives to be consistent in its use of language
features and idioms, as well as have some specific guidelines for the use of
runtime features.

Coding guidelines are based on the following guides (highest priority first):

1. This document.
2. The ISO [C++ Core Guidelines][].

In general, code should follow the C++ Core Guidelines, unless overridden by this document. At the moment these guidelines are
checked manually by reviewers with the goal to validate this with automatic
tools.

## Formatting

Obsoletium uses [clang-format][] to format the sources. Unfortunately, the clang-format formatting is not applied consistently. Follow local formatting of the header or source file.

clang-format doesn't check rules besides formatting. This document explains the most
common rules you should follow:

### Use `nullptr` instead of `NULL` or `0`

Further reading in the [C++ Core Guidelines][ES.47].

### Ownership and smart pointers

* [R.20][]: Use `std::unique_ptr` or `std::shared_ptr` to represent ownership
* [R.21][]: Prefer `unique_ptr` over `shared_ptr` unless you need to share
  ownership

Use `std::unique_ptr` to make ownership transfer explicit. For example:

```cpp
std::unique_ptr<Foo> FooFactory();
void FooConsumer(std::unique_ptr<Foo> ptr);
```

Since `std::unique_ptr` has only move semantics, passing one by value transfers
ownership to the callee and invalidates the caller's instance.

Don't use `std::auto_ptr`, it is deprecated ([Reference][cppref_auto_ptr]).

### Avoid non-const references

Using non-const references often obscures which values are changed by an
assignment. Consider using a pointer instead, which requires more explicit
syntax to indicate that modifications take place.

```cpp
class ExampleClass {
 public:
  explicit ExampleClass(OtherClass* other_ptr) : pointer_to_other_(other_ptr) {}

  void SomeMethod(const std::string& input_param,
                  std::string* in_out_param);  // Pointer instead of reference

  const std::string& get_foo() const { return foo_string_; }
  void set_foo(const std::string& new_value) { foo_string_ = new_value; }

  void ReplaceCharacterInFoo(char from, char to) {
    // A non-const reference is okay here, because the method name already tells
    // users that this modifies 'foo_string_' -- if that is not the case,
    // it can still be better to use an indexed for loop, or leave appropriate
    // comments.
    for (char& character : foo_string_) {
      if (character == from)
        character = to;
    }
  }

 private:
  std::string foo_string_;
  // Pointer instead of reference. If this object 'owns' the other object,
  // this should be a `std::unique_ptr<OtherClass>`; a
  // `std::shared_ptr<OtherClass>` can also be a better choice.
  OtherClass* pointer_to_other_;
};
```

## Others

### Type casting

* Use `static_cast<T>` if casting is required, and it is valid.
* Use `reinterpret_cast` only when it is necessary.
* Avoid C-style casts (`(type)value`).

Further reading:

* [ES.48][]: Avoid casts
* [ES.49][]: If you must use a cast, use a named cast

### Using `auto`

Being explicit about types is usually preferred over using `auto`.

Use `auto` to avoid type names that are noisy, obvious, or unimportant. When
doing so, keep in mind that explicit types often help with readability and
verifying the correctness of code.

```cpp
for (const auto& item : some_map) {
  const KeyType& key = item.first;
  const ValType& value = item.second;
  // The rest of the loop can now just refer to key and value,
  // a reader can see the types in question, and we've avoided
  // the too-common case of extra copies in this iteration.
}
```

### Do not include `*.h` if `*-inl.h` has already been included

Do:

```cpp
#include "util-inl.h"  // already includes util.h
```

Instead of:

```cpp
#include "util.h"
#include "util-inl.h"
```

[C++ Core Guidelines]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
[ES.47]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-nullptr
[ES.48]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-casts
[ES.49]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-casts-named
[clang-format]: https://clang.llvm.org/docs/ClangFormat.html
[R.20]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-owner
[R.21]: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-unique
[cppref_auto_ptr]: https://en.cppreference.com/w/cpp/memory/auto_ptr
