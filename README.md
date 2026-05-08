ujson
=====

*ujson* is a tiny C++ library used to read a [JSON](https://www.json.org/) format.
Where `u` stands for µ (micro). Original repository where the code is maintained is
here: https://github.com/bitolabs/ujson.git

Here is a brief example:

~~~~~~~~cpp
#include "ujson.h"
#include <iostream>

void main() {
    ujson::Json json;
    const ujson::Obj& obj = json.parse(
        "{"
        "  \"foo\" : 42,"
        "  \"bar\" : \"baz\","
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
* Compatible with JSON specifaction [RFC8259] and [ECMA-404].
* The API in [ujson.h] is simple, easy to read and self-explanatory, rarely
  requiring additional documentation.
* The input is always a memory buffer. To read a JSON file, the application must load
  the file in the memory as a C string and provide it to `ujson`.
* [In-place parsing] allows referencing the string tokens directly
  from the input buffer rather allocating each such token in the heap.
* The input is in UTF-8 format. The string tokens can contain escape sequences with
  [UTF-16 code points].
* Number values can be fetched as integers (32 or 64 bit) or as 64-bit float values.
* Optional extended syntax that is not part of JSON standard. These can
  be turned ON/OFF:
    - `//` comments.
    - Check that member names are unique within the object and raise
      an error if not. When this feature is OFF, duplicates are allowed
      as per JSON standard.
    - Allow trailing commas in arrays and objects: `{"a": [1, 2, ], "b": 42, }`.
    - Allow no digits after the decimal point: `[0., -1., 2.e10]`.
    - Allow integers in hex format like: `[0x1A, 0X2b]`.
    - Allow object member names as C identifiers without quotes: `{foo: "bar"}`.
* Exceptions are used to handle the errors. See [error handling].
* Value validation is easy and doesn't require a JSON schema. See:
    - [Number range checking].
    - [Optional and default values].
    - [Checking array length].
    - [Rejecting unknown members].
    - [Enumerations].
* On error, `ujson` reports the corresponding line number. This is supported
  for any error, even it happens after parsing, due to `ujson` remembers the line
  number for any loaded value.

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
So we can cast it to `Obj` class by calling `Val::as_obj()`
method. Should this value be of another type, `as_obj()`
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
    for (size_t i = 0; i < menu.get_len(); i++) {
        std::string item = menu.get_str(i);
        ...
    }
    ...
    free(in);
}
~~~~~~~~

<a name="a_lifetime"></a>
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
* `Json::parse()` or `parse_in_place()` is called again.
* `Json::clear()` is called.

If the [in-place parsing] is used, then strings and names are valid until
the application deallocates the input buffer, even `Json` instance is no
longer allocated. However the references to `Val` classes are bound only to
`Json` instance.

<a name="a_errors"></a>
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

### Value validation

<a name="a_range"></a>
#### Number range checking

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

<a name="a_enums"></a>
#### Enumerations

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

<a name="a_optional"></a>
#### Optional and default values

~~~~~~~~cpp
const ujson::Obj& obj = ...

// By default named members are required:
//
int32_t     n = obj.get_i32("n"); // will fail if "n" is absent
std::string s = obj.get_str("s"); // will fail if "s" is absent
const ujson::Obj& x = root.get_obj("x"); // will fail if "x" is absent

// Handling an optional number:
//
int32_t n = obj.get_i32("n", 0, 100, 42); // will return 42 if "n" is absent
                                          // note that (0, 100) is the range

// Handling an optional string:
//
std::string s = obj.get_str("s", "default"); // will return "default" if "s" is absent

// Handling an optional object:
//
if (const ujson::Obj* x = root.get_obj_opt("x")) {
    std::string child = x->get_str("child");
}
~~~~~~~~

<a name="a_require_len"></a>
#### Checking array length

By default an array `Arr` value can have any number of elements.
However, if the application must limit the array length and reject
it if it is not valid, then it can use the `Arr::require_len()` method
as follows:

~~~~~~~~cpp
ujson::Json json;

// Limit the 'rgb' array to 3 elements, otherwise it will throw ErrBadArrLen.
const ujson::Arr& rgb = json.parse("[0, 255, 0]").as_arr().require_len(3);
...

// Bound the 'samples' length to range (0, 16), otherwise it will throw ErrBadArrLen.
const ujson::Arr& samples = json.parse("[0, 1, 2, 7]").as_arr().require_len(0, 16);
...


// The 'unlimted' array has no length limitation.
const ujson::Arr& unlimited = json.parse("[0, 1, 2, 4, 5]").as_arr();
...
~~~~~~~~

<a name="a_unknown"></a>
#### Rejecting unknown members

`ujson` can throw the `ErrUnknownMember` in case the application
doesn't recognize the name of the value. This is a good practice
of error handling, otherwise the JSON file may contain typos that
will be silently ignored, leading to unexpected application behavior.

This is how it should be handled:

* Each time the application interrogates a value (by name, or by
  index), it is marked by ujson as 'used'. Meaning that the application
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
  these array elements could contain inside some objects with named values.

<a name="a_inplace"></a>
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

<a name="a_utf16"></a>
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

[value life time]:             #a_lifetime
[number range checking]:       #a_range
[enumerations]:                #a_enums
[error handling]:              #a_errors
[optional and default values]: #a_optional
[checking array length]:       #a_require_len
[rejecting unknown members]:   #a_unknown
[UTF-16 code points]:          #a_utf16
[in-place parsing]:            #a_inplace
[ujson.h]: ujson.h
[ujson.cpp]: ujson.cpp
[ujson-test]: ../../../ujson-test.git
[RFC8259]: https://tools.ietf.org/html/rfc8259
[ECMA-404]: https://ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf
