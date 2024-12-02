ujson
=====

*ujson* is a tiny C++ library used to read a [JSON](https://www.json.org/) format.
Where `u` stands for µ (micro). Original repository: https://github.com/bitolabs/ujson.git

Here is a brief example:

~~~~~~~~cpp
#include "ujson.h"
#include <iostream>

void main() {
    ujson::Json json;
    const ujson::Obj& obj = json.parse(
        "{"
        "  \"foo\" : 42,
        "  \"bar\" : \"baz\",
        "}").as_obj();

    int32_t     foo = obj.get_i32("foo");
    const char* bar = obj.get_str("bar");

    std::cout << foo << '\n';
    std::cout << bar << '\n';
}
~~~~~~~~

Summary of features
-------------------

* Requires C++17 or later. No other dependency.
* Just two files: [ujson.h], [ujson.cpp]. The tests are in a separate [ujson-test]
  repo, so no junk is included in the project that uses `ujson`.
* Compatible with JSON specifaction [RFC7159] and [ECMA-404].
* The API in [ujson.h] is simple, easy to read and self-explanatory, rarely
  requiring additional documentation.
* The input is always a memory buffer. To read a JSON file, the application must load the
  the file in the memory as a C string and provide it to `ujson`.
* [In-place parsing] allows referencing the string tokens directly
  from the input buffer rather allocating each such token in the heap.
* The input is in UTF-8 format. The string tokens can contain escape sequences with
  [UTF-16 code points].
* `//` comments are supported. Note that these are not part of the JSON standard.
* Number values can be fetched as signed integers (32 or 64 bit) or as 64-bit float values.
* Exceptions are used to handle the errors. See [error handling].
* Value validation is easy and doesn't require a JSON schema. See:
    - [Number range checking].
    - [Rejecting unknown members].
    - [Enumerations].
* If a named value is absent, it can be optionally replaced by a default value provided by the
  application.

User Guide
----------

### General steps to read a JSON

#### 1) Load JSON file into memory

The application must provide the JSON content as a memory buffer
of `char` elements in UTF-8 format. Usually the JSON data resides
in a file, so the application must load the whole content into a buffer.
The application can use various APIs to read from file, there `ujson`
doesn't have a function to do that as to not impose a particular API.

The input buffer must be zero-terminated in case [in-place parsing]
will be used. Otherwise the application can provide the length of the buffer.

Here is an example using C standard library:

~~~~~~~~cpp
char* str_read_file(const char* fname)
{
    char* str = NULL;
    FILE* f = fopen(fname, "rb");
    if (NULL == f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    str = (char*)malloc(size + 1);
    if (NULL == str) goto cleanup;
    size_t n = fread(str, 1, size, f);
    str[size] = 0;
    if (n != size) {
        free(str);
        str = NULL;
    }
 cleanup:
    fclose(f);
    return str;
}

void main()
{
    char* in = str_read_file("my.json");
    ...
    free(in);
}
~~~~~~~~

#### 2) Parse the JSON buffer

Next we have to create a variable of `Json` type.
This variable must be allocated until we finished with
getting all the JSON values. We must not use any references
to values after we deallocate the `Json` variable.

Then we call `Json::parse()` or `Json::parse_in_place()`, see [in-place parsing].
Note, in case of `Json::parse()` we can optionally provide
the buffer length, so that it doesn't have to be zero-terminated.

Either function returns a `const ujson::Val&` which is the root value.
The `Val` class represents a JSON value of any type.

In majority of cases the a JSON's root value is an object.
So we can cast it to `Obj` class by calling Val::as_obj()
method. Should this value be of another type, as_obj()
will raise `ErrBadType` exception. Check other `Val::as_*()`
methods for different types.

If we use the `parse()` method, the input buffer can be freed
by the application immediately after the call. If we use
`parse_in_place()`, we must keep the input buffer allocated
until we are done fetching JSON values.

~~~~~~~~cpp
void main()
{
    char* in = str_read_file("my.json");
    ujson::Json json;
    const ujson::Obj& root = json.parse(in).as_obj();
    ...
    free(in);
}
~~~~~~~~

#### 3) Fetch JSON values

We can read values via following classes derived from `Val` class:

* `Obj`: contains named values of any type. Use the get methods:
    - `get_xxx(name)`: where xxx is a value type such as `i32`, etc.
    - `get_member(name)`: provides a reference to a `Val` that can be cast
      to a particular type.
* `Arr`: contains an array of values of any type. Use get methods:
    - `get_xxx(idx)`: where xxx is a value type such as `i32`, etc.
    - `get_element(idx)`: provides a reference to a `Val` that can be cast
      to a particular type.
* `Str`: use its `get()` method to get the pointer to a C string.
* `F64`: use its `get()` methods to get a floating point value of a `double` type.
  Can be used on any numbers, integers or floating point.
* `Int`: use its `get()` methods to get a int64_t value, or `get_i32()` to restrict
  values to 32-bit. Can be used only with integers.
* `Bool`: use its `get()` method to obtain a value of a `bool` type.

A `null` value can be determined by verifying if `Val::get_type()` returns `vtNull`.

Note that value references are valid until the `Json` instance is allocated. See
more in [value life time] section.

Below is an example of fetching JSON values:

~~~~~~~~~json
{
    "name"  : "Main Window",
    "width" : 640,
    "height": 480,
    "on_top": false,
    "opacity": 0.9,
    "menu"  : ["Open", "Save", "Exit"],
}
~~~~~~~~~

~~~~~~~~cpp
void main()
{
    char* in = str_read_file("my.json");
    ujson::Json json;
    const ujson::Obj& root = json.parse(in).as_obj();

    std::string name = root.get_str ("name");
    int32_t width    = root.get_i32 ("width",  100, 4000); // restrict to range (100,4000)
    int32_t height   = root.get_i32 ("height", 100, 4000);
    bool on_top      = root.get_bool("on_top", false); // default: false
    double opacity   = root.get_f64 ("opacity", 0.0, 1.0, 1.0); // range (0,1), default: 1

    const ujson::Arr& menu = root.get_arr("menu");
    for (int32_t i = 0; i < menu.get_len(); i++) {
        std::string item = arr.get_str(i);
        ...
    }
    ...
    free(in);
}
~~~~~~~~

### Value life time

The `ujson` API provides references/pointers to objects such as:

* `Val` derived classes. Provided for example by:
  - `Json::parse()` and `Json::parse_in_place()`
  - `Arr::get_element()`
  - `Obj::get_member()`
  - etc.
* C strings of `Str` values. Provided for example by:
  - `Str::get()`
  - `Arr::get_str()`
  - `Obj::get_str()`
* C string names of values. Provided for example by:
  - `Val::get_name()`
  - `Obj::get_member_name()`

These references are valid as long as the `Json` instance is allocated,
and become invalid when any of the below occurs:

* `Json` instance is deallocated.
* `Json::parse[_in_place]()` is called again.
* `Json::clear()` is called.

If the [in-place parsing] is used, then strings and names are valid until
the application deallocates the input buffer, even `Json` instance is no
longer allocated. However the references to `Val` classes are bound only to
`Json` instance.

### Number range checking

When fetching number values, the application can specify a range.
If the value is not in range, then `ErrBadIntRange` or `ErrBadF64Range`
exception is thrown.

Example of methods performing range checking:

* `Int::get(lo, hi)`
* `Int::get_i32()`
* `Int::get_i32(lo, hi)`
* `F64::get(lo, hi)`
* `Arr::get_i32(idx, lo, hi)`
* `Arr::get_i64(idx, lo, hi)`
* `Arr::get_f64(idx, lo, hi)`
* `Obj::get_i32(name, lo, hi)`
* `Obj::get_i64(name, lo, hi)`
* `Obj::get_f64(name, lo, hi)`

If (`lo > hi`), then the range is ignored.

The `get_i32()` methods implicitly check that the number fits in
32-bit integer range.

### Enumerations

`Str` values can be restricted to a set that in the application
corresponds to an enum. If the value is not in the set, `ErrBadEnum`
is thrown. Example of enum methods:

* `Str::get_enum_idx(str_set, len)`
* `Str::get_enum(str_set, val_set)`
* `Obj::get_str_enum_idx(name, str_set, len, required=true)`
* `Obj::get_str_enum(name, str_set, val_set[, def])`

Example:

~~~~~~~~cpp
enum Color { red, green, blue };
ujson::Json json;
auto& obj = json.parse(R"({"foo": "green"})").as_obj();
Color color = obj.get_str_enum("foo",
    std::array{"red", "green", "blue"},
    std::array{red, green, blue});
~~~~~~~~

### Error handling

Error handling is done through exceptions:

* `Err` is the base `ujson` exception. It holds the corresponding 
  `line` number in JSON text. Even if the error occurs after parsing,
  each `Val` instance remembers the corresponding line number where
  it was specified. The application can also call `Val::get_line()`
  to obtain it.
* `ErrSyntax` is the exception that can occur during `Json::parse*()`
  call. It indicates that JSON text is malformed.
* `ErrValue` is the exception that can occur after parsing, while the
  application is fetching the parsed values. It indicates that the
  value doesn't pass the validation imposed by the application (bad
  type, bad number range, etc).
  This exception is split in sub-classes, each indicating a special
  type of validation. `ErrValue` contains following common attributes
  (sub-exceptions may contain other specific attributes):
    - `val_name` - name of the failed value. Empty if value is not
      an object member.
    - `val_idx` - index in the array or object. -1 if this is a root
      value.
    - `val_type` - the actual type of the value.

The `Err::what()` method return a short message. To get more details
that includes the line number and other attributes, call `Err::get_err_str()`.

### Rejecting unknown members

`ujson` can throw the `ErrUnknownMember` in case the application
doesn't recognize the name of the value. This is a good practice
of error handling, otherwise the JSON file may contain typos that
will be silently ignored, leading to unexpected application behavior.

This how this should be handled:

* Each time the application interrogates a value (by name, or by
  index), it is marked as 'used'. Meaning that the application
  expects such a value.
* When the application finishes fetching all the values, it calls
  `Val::reject_unknown_members()` on the root object. This will check
  recursively if there's any named value that is left 'unused'.
  If so, it will raise the exception.
* For this to work correctly, the application must interrogate every
  known value. Normally this is what the application does, otherwise
  it may indicate that it never reads some values. However there
  may be conditions when the application ignores some values,
  for example depending on the content of the other values.
* If the application needs to ignore a branch of sub-values,
  it can call the `Val::ignore_members` on any of the parent value
  of this branch. This will mark all the children as 'used'.
* Note that, even if the parent value is an `Arr` meaning that it
  contains un-named values which are not checked by `reject_unknown_members()`,
  these could contain inside `Obj` objects with named values.

### In-place parsing

When calling `Json::parse_in_place()`, the application provides a zero-terminated
input buffer that is modified by the library so that the string tokens are made
zero-terminated.

For example, the following input

~~~~~~~~
{"foo": "bar"}\0
~~~~~~~~

will be modified like:
~~~~~~~~
{"foo\0: "bar\0}\0
~~~~~~~~

This allows the library to provide pointers to "foo" and "bar" strings
instead of allocating and copying each string in the heap.

When calling `Json::parse()`, the application provides a constant buffer which
doesn't need to be zero-terminated if the length is provided. In this case
the library will allocate an internal zero-terminated copy for the whole input
and will call `Json::parse_in_place()`.

### Unicode code points

It is possible to specify in strings escape sequence with UTF-16 code-points.
For example:

~~~~~~~~cpp
const char str[] =
    R"(
      {
          "foo" : "\u00B5",       // µ (MICRO SIGN) U+00B5
          "bar" : "\uD83D\uDE02", // (FACE WITH TEARS OF JOY) U+1F602
      }
    )";
~~~~~~~~

The second example has two UTF-16 code points (high surrogate and low surrogate) that
form one emoji character [FACE WITH TEARS OF JOY](https://en.wikipedia.org/wiki/Face_with_Tears_of_Joy_emoji).

Unit tests
----------

Unit tests are in a separate repo: [ujson-test].

[value life time]:           #markdown-header-value-life-time
[number range checking]:     #markdown-header-number-range-checking
[enumerations]:              #markdown-header-enumerations
[error handling]:            #markdown-header-error-handling
[rejecting unknown members]: #markdown-header-rejecting-unknown-members
[UTF-16 code points]:        #markdown-header-unicode-code-points
[in-place parsing]:          #markdown-header-in-place-parsing
[ujson.h]: ujson.h
[ujson.cpp]: ujson.cpp
[ujson-test]: ../../../ujson-test.git
[RFC7159]: https://tools.ietf.org/html/rfc7159
[ECMA-404]: https://ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf
