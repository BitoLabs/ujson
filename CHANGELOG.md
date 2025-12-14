Change log
==========

This project follows [Semantic Versioning](http://semver.org/).

1.3.0 (IN PROGRESS)
==================

### New features

* Added parsing options (see ujson::Options). Now the caller
  can specify these options when calling parse() method. Following
  options are implemented:

  - optUniqueMembers (default): disallow duplicate member names
    within the object. This is how ujson behaved before, but now
    this can be switched off. Apparently standard JSON allows (but
    not recommends) duplicates, see https://datatracker.ietf.org/doc/html/rfc8259#section-4

### Changes

* Raise ErrSyntax if a null character is found before the text ends,
  when the length of input is passed explicitly to parse() method.

### Fixes

* Avoid crash due to stack overflow when the nested level of values
  is too high. Raise ErrSyntax if nested level is 512 or higher.
  This issue was caught thanks to https://github.com/nst/JSONTestSuite
* Fix string parsing when \" escape was present.
  This issue was caught thanks to https://github.com/nst/JSONTestSuite
* Fix string parsing when character code is greater than 0x7F. Basically
  parsing failed for all unescaped non-ASCII characters.
  This issue was caught thanks to https://github.com/nst/JSONTestSuite

1.2.0 (2025-11-23)
==================

### New features

* Add Arr::require_len()

### Changes

* Change Arr index and length type from int32_t to size_t.

1.1.1 (2025-10-02)
==================

### Fixes

* Fix "unreachable code" compiler warning.

1.1.0 (2025-07-26)
==================

### New features

* Add Obj::get_arr_opt(name), Obj::get_obj_opt(name).
* Add u32 support.

### Changes

* Change ErrBadEnum to improve the error message.

1.0.3 (2025-04-04)
==================

### Fixes

* Fix an error when compiling with GCC due to strncpy_s().
* Fix links to headings in README.md not working in GitHub.
  Now these do not work in Bitbucket as Markdown format is
  not compatible.

1.0.2 (2024-12-02)
==================

### Fixes

* Make README.md compatible with BitBucket.

1.0.1 (2024-11-29)
==================

### Fixes

* Minor corrections in README.md

1.0.0 (2024-11-23)
==================

### New features

* First version
