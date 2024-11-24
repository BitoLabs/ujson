#pragma once

#include <stdint.h>
#include <stdexcept>
#include <string>
#include <array>

namespace ujson {

enum ValType: uint32_t
{
    vtNone = 0,
    vtNull = 1 << 0,
    vtBool = 1 << 1,
    vtInt  = 1 << 2,
    vtF64  = 1 << 3,
    vtStr  = 1 << 4,
    vtArr  = 1 << 5,
    vtObj  = 1 << 6
};

class Val;
class Bool;
class Int;
class F64;
class Str;
class Arr;
class Obj;

class Json
{
public:
    Json() noexcept = default;
    Json(const Json&) = delete;
    Json(Json&&) = delete;
    ~Json() noexcept { clear(); }
    Json& operator = (const Json&) = delete;
    Json& operator = (Json&&) = delete;
    const Val& parse(const char* str, size_t len = 0); // str must be zero-terminated if len=0
    const Val& parse_in_place(char* str); // str must be zero-terminated and allocated until Json instance is destroyed
    void clear() noexcept;
private:
    void free_root() noexcept;
    void free_buf() noexcept;
private:
    Val*  m_root = nullptr;
    char* m_buf  = nullptr;
};

class Val
{
public:
    ValType get_type() const noexcept;
    int32_t get_idx() const noexcept;
    const char* get_name() const noexcept;
    int32_t get_line() const;
    bool is_num() const noexcept;
    const Bool& as_bool() const;
    const Int& as_int() const;
    const F64& as_f64() const;
    const Str& as_str() const;
    const Arr& as_arr() const;
    const Obj& as_obj() const;
    void reject_unknow_members() const; // throws ErrUnknownMember if any named child value was not accessed
    void ignore_members() const noexcept; // marks recursively all children as accessed
protected:
    Val() = default;
    Val(const Val&) = default;
    ~Val() = default;
};

class Bool: public Val
{
public:
    static constexpr ValType type() { return vtBool; }
    bool get() const noexcept;
protected:
    Bool() = default;
    Bool(const Bool&) = delete;
    ~Bool() = default;
};

class Int: public Val
{
public:
    static constexpr ValType type() { return vtInt; }
    int64_t get() const noexcept;
    int64_t get(int64_t lo, int64_t hi) const;
    int32_t get_i32() const; // checks if it fits in int32_t
    int32_t get_i32(int32_t lo, int32_t hi) const;
protected:
    Int() = default;
    Int(const Int&) = delete;
    ~Int() = default;
};

class F64: public Val
{
public:
    static constexpr ValType type() { return vtF64; }
    double get() const noexcept;
    double get(double lo, double hi) const;
protected:
    F64() = default;
    F64(const F64&) = delete;
    ~F64() = default;
};

class Str: public Val
{
public:
    static constexpr ValType type() { return vtStr; }
    const char* get() const noexcept;
    int32_t get_enum_idx(const char* const str_set[], size_t len) const;
    template <typename T, size_t N>
    T get_enum(
        const std::array<const char*, N>& str_set,
        const std::array<T, N>& val_set) const
    {
        return val_set[get_enum_idx(str_set.data(), str_set.size())];
    }
protected:
    Str() = default;
    Str(const Str&) = delete;
    ~Str() = default;
};

class Arr: public Val
{
public:
    static constexpr ValType type() { return vtArr; }
    int32_t get_len() const noexcept;
    const Val& get_element(int32_t idx) const;
    bool get_bool(int32_t idx) const;
    int32_t get_i32(int32_t idx, int32_t lo = 0, int32_t hi = -1) const;
    int64_t get_i64(int32_t idx, int64_t lo = 0, int64_t hi = -1) const;
    double get_f64(int32_t idx, double lo = 0.0, double hi = -1.0) const;
    const char* get_str(int32_t idx) const;
    const Arr& get_arr(int32_t idx) const;
    const Obj& get_obj(int32_t idx) const;
protected:
    Arr() = default;
    Arr(const Arr&) = delete;
    ~Arr() = default;
};

class Obj: public Arr
{
public:
    static constexpr ValType type() { return vtObj; }
    int32_t get_member_idx(const char* name, bool required=true) const; // -1 if not found
    const char* get_member_name(int32_t idx) const;
    const Val* get_member(const char* name, bool required=true) const;
    bool get_bool(const char* name, const bool* def = nullptr) const;
    bool get_bool(const char* name, bool def) const { return get_bool(name, &def); }
    int32_t get_i32(const char* name, int32_t lo = 0,  int32_t hi = -1, const int32_t* def = nullptr) const;
    int32_t get_i32(const char* name, int32_t lo, int32_t hi, int32_t def) const { return get_i32(name, lo, hi, &def); }
    int64_t get_i64(const char* name, int64_t lo = 0, int64_t hi = -1, const int64_t* def = nullptr) const;
    int64_t get_i64(const char* name, int64_t lo, int64_t hi, int64_t def) const { return get_i64(name, lo, hi, &def); }
    double get_f64(const char* name, double lo = 0.0, double hi = -1.0, const double* def = nullptr) const;
    double get_f64(const char* name, double lo, double hi, double def) const { return get_f64(name, lo, hi, &def); }
    const char* get_str(const char* name, const char* def = nullptr) const;
    int32_t get_str_enum_idx(const char* name, const char* const str_set[], size_t len, bool required = true) const;
    template <typename T, size_t N>
    T get_str_enum(
        const char* name,
        const std::array<const char*, N>& str_set,
        const std::array<T, N>& val_set) const
    {
        return val_set[get_str_enum_idx(name, str_set.data(), str_set.size())];
    }
    template <typename T, size_t N>
    T get_str_enum(
        const char* name,
        const std::array<const char*, N>& str_set,
        const std::array<T, N>& val_set,
        T def) const
    {
        int32_t i = get_str_enum_idx(name, str_set.data(), str_set.size(), false);
        return (i >= 0) ? val_set[i] : def;
    }
    const Arr& get_arr(const char* name) const;
    const Obj& get_obj(const char* name) const;
protected:
    Obj() = default;
    Obj(const Obj&) = delete;
    ~Obj() = default;
};

struct Err : std::runtime_error {
    int32_t line;

    explicit Err(const char* msg, int32_t line_no) noexcept;
    virtual std::string get_err_str() const;
};

struct ErrSyntax : Err // errors that occur during parsing due to invalid JSON syntax
{
    explicit ErrSyntax(const char* msg, int32_t line_no) noexcept : Err(msg, line_no) {}
};

struct ErrValue : Err // errors that occur when value validation fails (after it was successfully parsed)
{
    std::string val_name;
    int32_t     val_idx;
    ValType     val_type;

    explicit ErrValue(const char* msg, const Val& v) noexcept;
    std::string get_err_str() const override;
};

struct ErrBadType : ErrValue
{
    ValType expected_type;

    explicit ErrBadType(const Val& v, ValType expected) noexcept;
    std::string get_err_str() const override;
};

struct ErrBadIntRange : ErrValue
{
    int64_t lo;
    int64_t hi;

    explicit ErrBadIntRange(const Val& v, int64_t lo, int64_t hi) noexcept;
    std::string get_err_str() const override;
};

struct ErrBadF64Range : ErrValue
{
    double lo;
    double hi;

    explicit ErrBadF64Range(const Val& v, double lo, double hi) noexcept;
    std::string get_err_str() const override;
};

struct ErrMemberNotFound : ErrValue
{
    explicit ErrMemberNotFound(const Obj& v, const char* name) noexcept;
};

struct ErrUnknownMember : ErrValue
{
    explicit ErrUnknownMember(const Val& v) noexcept;
};

struct ErrBadEnum : ErrValue
{
    explicit ErrBadEnum(const Val& v) noexcept;
};

}; // namespace ujson
