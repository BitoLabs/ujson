#if defined(_MSC_VER)
#  define UJSON_CPLUSPLUS _MSVC_LANG
#else
#  define UJSON_CPLUSPLUS __cplusplus
#endif
#if (UJSON_CPLUSPLUS < 201703L)
#  error "C++17 is required"
#endif

#include "ujson.h"
#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <stdexcept>
#include <cstdlib>
#include <utility>

namespace ujson {

const uint32_t vtUsedBit = 1U << 31; // Bit in m_type indicating that the value was accessed by the application.

class ArrImpl;
class ObjImpl;

class ValImpl: public Val
{
public:
    struct List {
        std::vector<ValImpl> values;
        virtual ~List() = default;
    };
    struct Dict : public List {
        std::unordered_map<std::string_view, int32_t> map;
    };
public:
    
    void clear()
    {
        if (m_type & (vtArr | vtObj)) {
            for (auto& v : m_data.list->values) {
                v.clear();
            }
            delete m_data.list;
            m_data.list = nullptr;
        }
        m_type = vtNull;
    }

    ValImpl& init_bool(bool b)
    {
        m_type = vtBool;
        m_data.b = b;
        return *this;
    }

    ValImpl& init_int(int64_t n)
    {
        m_type = vtInt;
        m_data.i64 = n;
        return *this;
    }

    ValImpl& init_f64(double n)
    {
        m_type = vtF64;
        m_data.f64 = n;
        return *this;
    }

    ValImpl& init_str(const char* str)
    {
        m_type = vtStr;
        m_data.str = str;
        return *this;
    }

    ArrImpl& init_arr()
    {
        m_type = vtArr;
        m_data.list = new List;
        return *reinterpret_cast<ArrImpl*>(this);
    }

    ObjImpl& init_obj()
    {
        m_type = vtObj;
        m_data.list = new Dict;
        return *reinterpret_cast<ObjImpl*>(this);
    }

    void mark_as_used() const noexcept
    {
        m_type |= vtUsedBit;
    }

    static const ValImpl& from(const Val* v)
    {
        return *static_cast<const ValImpl*>(v);
    }

    static ValImpl& from(Val* v)
    {
        return *static_cast<ValImpl*>(v);
    }

public:
    union {
        bool        b;
        int64_t     i64;
        double      f64;
        const char* str;
        List*       list;
    }                 m_data = {};      //  8 bytes
    mutable uint32_t  m_type = vtNull;  //  4 bytes, mutable because we set vtUsedBit when we access the value
    int32_t           m_line_no = 0;    //  4 bytes
    const char*       m_name = "";      //  8 bytes
    int32_t           m_idx = -1;       //  4 bytes
};

class ArrImpl : public ValImpl
{
public:

    static const ArrImpl& from(const Arr* arr)
    {
        return *reinterpret_cast<const ArrImpl*>(arr);
    }

    int32_t get_len() const
    {
        return static_cast<int32_t>(m_data.list->values.size());
    }

    const ValImpl& get_element(int32_t idx) const
    {
        return m_data.list->values.at(idx);
    }

    ValImpl& get_element(int32_t idx)
    {
        return m_data.list->values.at(idx);
    }

    ValImpl& add_element()
    {
        ValImpl& v = m_data.list->values.emplace_back();
        v.m_idx = static_cast<int32_t>(get_len()) - 1;
        return v;
    }

};

class ObjImpl : public ArrImpl
{
public:

    static const ObjImpl& from(const Obj* obj)
    {
        return *reinterpret_cast<const ObjImpl*>(obj);
    }

    const Obj& as_obj() const
    {
        return *static_cast<const Obj*>(static_cast<const Val*>(this));
    }

    int32_t find(const char* name) const
    {
        auto& m = map();
        auto iter = m.find(name);
        return (iter != m.end()) ? iter->second : -1;
    }

    bool add_member(const char* name, int32_t idx, ValImpl& v)
    {
        const bool added = map().try_emplace(name, idx).second;
        if (added) {
            v.m_name = name;
        }
        return added;
    }

private:
    const std::unordered_map<std::string_view, int32_t>& map() const
    {
        return static_cast<Dict*>(m_data.list)->map;
    }

    std::unordered_map<std::string_view, int32_t>& map()
    {
        return static_cast<Dict*>(m_data.list)->map;
    }
};

template<class T, uint32_t E>
const T& val_cast(const Val* v)
{
    if ((v->get_type() & E) == 0) {
        throw ErrBadType(*v, T::type());
    }
    return *static_cast<const T*>(v);
}

ValType Val::get_type() const noexcept
{
    return static_cast<ValType>(ValImpl::from(this).m_type & ~vtUsedBit);
}

int32_t Val::get_idx() const noexcept
{
    return ValImpl::from(this).m_idx;
}

const char* Val::get_name() const noexcept
{
    return ValImpl::from(this).m_name;
}

bool Val::is_num() const noexcept
{
    return (get_type() & (vtInt | vtF64)) != 0;
}

const Bool& Val::as_bool() const
{
    return val_cast<Bool, vtBool>(this);
}

const Int& Val::as_int() const
{
    return val_cast<Int, vtInt>(this);
}

const F64& Val::as_f64() const
{
    return val_cast<F64, vtInt | vtF64>(this);
}

const Str& Val::as_str() const
{
    return val_cast<Str, vtStr>(this);
}

const Arr& Val::as_arr() const
{
    return val_cast<Arr, vtArr>(this);
}

const Obj& Val::as_obj() const
{
    return val_cast<Obj, vtObj>(this);
}

int32_t Val::get_line() const
{
    return ValImpl::from(this).m_line_no;
}

static void do_reject_unknow_members(const ValImpl* v)
{
    if (v->get_type() & (vtArr | vtObj)) {
        const ArrImpl& arr = *static_cast<const ArrImpl*>(v);
        for (int32_t i = 0; i < arr.get_len(); i++) {
            v = &arr.get_element(i);
            if (0 == (v->m_type & vtUsedBit) && (arr.get_type() & vtObj)) {
                throw ErrUnknownMember(*v);
            }
            do_reject_unknow_members(v);
        }
    }
}

void Val::reject_unknow_members() const
{
    do_reject_unknow_members(&ValImpl::from(this));
}

static void do_ignore_members(const ValImpl* v)
{
    if (v->get_type() & (vtArr | vtObj)) {
        const ArrImpl& arr = *static_cast<const ArrImpl*>(v);
        for (int32_t i = 0; i < arr.get_len(); i++) {
            v = &arr.get_element(i);
            v->mark_as_used();
            do_ignore_members(v);
        }
    }
}

void Val::ignore_members() const noexcept
{
    do_ignore_members(&ValImpl::from(this));
}

bool Bool::get() const noexcept
{
    return ValImpl::from(this).m_data.b;
}

int64_t Int::get() const noexcept
{
    return ValImpl::from(this).m_data.i64;
}

int64_t Int::get(int64_t lo, int64_t hi) const
{
    const int64_t num = get();
    if (lo <= hi && (num < lo || num > hi)) {
        throw ErrBadIntRange(*this, lo, hi);
    }
    return num;
}

int32_t Int::get_i32() const
{
    return static_cast<int32_t>(get(INT32_MIN, INT32_MAX));
}

int32_t Int::get_i32(int32_t lo, int32_t hi) const
{
    if (lo > hi) {
        lo = INT32_MIN;
        hi = INT32_MAX;
    }
    return static_cast<int32_t>(get(lo, hi));
}

double F64::get() const noexcept
{
    auto& impl = ValImpl::from(this);
    return (vtInt & impl.get_type()) ? impl.m_data.i64 : impl.m_data.f64;
}

double F64::get(double lo, double hi) const
{
    const double num = get();
    if (lo <= hi && (num < lo || num > hi)) {
        throw ErrBadF64Range(*this, lo, hi);
    }
    return num;
}

const char* Str::get() const noexcept
{
    return ValImpl::from(this).m_data.str;
}

int32_t Str::get_enum_idx(const char* const str_set[], size_t len) const
{
    const char* str = get();
    for (size_t i = 0; i < len; i++) {
        if (strcmp(str, str_set[i]) == 0) return static_cast<int32_t>(i);
    }
    throw ErrBadEnum(*this);
}

int32_t Arr::get_len() const noexcept
{
    return ArrImpl::from(this).get_len();
}

const Val& Arr::get_element(int32_t idx) const
{
    const ValImpl& v = ArrImpl::from(this).get_element(idx);
    v.mark_as_used();
    return v;
}

bool Arr::get_bool(int32_t idx) const
{
    return get_element(idx).as_bool().get();
}

int32_t Arr::get_i32(int32_t idx, int32_t lo, int32_t hi) const
{
    return get_element(idx).as_int().get_i32(lo, hi);
}

int64_t Arr::get_i64(int32_t idx, int64_t lo, int64_t hi) const
{
    return get_element(idx).as_int().get(lo, hi);
}

double Arr::get_f64(int32_t idx, double lo, double hi) const
{
    return get_element(idx).as_f64().get(lo, hi);
}

const char* Arr::get_str(int32_t idx) const
{
    return get_element(idx).as_str().get();
}

const Arr& Arr::get_arr(int32_t idx) const
{
    return get_element(idx).as_arr();
}

const Obj& Arr::get_obj(int32_t idx) const
{
    return get_element(idx).as_obj();
}

int32_t Obj::get_member_idx(const char* name, bool required) const
{
    auto& self = ObjImpl::from(this);
    int32_t idx = self.find(name);
    if (required && idx < 0) {
        throw ErrMemberNotFound(*this, name);
    }
    return idx;
}

const char* Obj::get_member_name(int32_t idx) const
{
    return ObjImpl::from(this).get_element(idx).get_name();
}

const Val* Obj::get_member(const char* name, bool required) const
{
    const int32_t idx = get_member_idx(name, required);
    return (idx >= 0) ? &get_element(idx) : nullptr;
}

bool Obj::get_bool(const char* name, const bool* def) const
{
    auto* v = get_member(name, nullptr == def);
    return v ? v->as_bool().get() : *def;
}

int32_t Obj::get_i32(const char* name, int32_t lo, int32_t hi, const int32_t* def) const
{
    auto* v = get_member(name, nullptr == def);
    return v ? v->as_int().get_i32(lo, hi) : *def;
}

int64_t Obj::get_i64(const char* name, int64_t lo, int64_t hi, const int64_t* def) const
{
    auto* v = get_member(name, nullptr == def);
    return v ? v->as_int().get(lo, hi) : *def;
}

double Obj::get_f64(const char* name, double lo, double hi, const double* def) const
{
    auto* v = get_member(name, nullptr == def);
    return v ? v->as_f64().get(lo, hi) : *def;
}

const char* Obj::get_str(const char* name, const char* def) const
{
    auto* v = get_member(name, nullptr == def);
    return v ? v->as_str().get() : def;
}

int32_t Obj::get_str_enum_idx(
    const char* name,
    const char* const str_set[],
    size_t len,
    bool required) const
{
    const ujson::Val* v = get_member(name, required);
    if (nullptr == v) return -1;
    return v->as_str().get_enum_idx(str_set, len);
}

const Arr& Obj::get_arr(const char* name) const
{
    return get_member(name)->as_arr();
}

const Obj& Obj::get_obj(const char* name) const
{
    return get_member(name)->as_obj();
}

class Parser
{
public:
    Parser(char* str) :
        m_next{ str },
        m_line_count{ 1 }
    {
    }

    ValImpl* parse()
    {
        ValImpl* v = parse_val(nullptr);
        skip_white_space();
        if (0 != *m_next) {
            throw ErrSyntax("invalid value syntax", m_line_count);
        }
        return v;
    }

private:
    
    ValImpl* parse_val(ArrImpl* parent)
    {
        skip_white_space();
        if (auto v = parse_val_null(parent); v) { return v; }
        if (auto v = parse_val_bool(parent); v) { return v; }
        if (auto v = parse_val_num (parent); v) { return v; }
        if (auto v = parse_val_str (parent); v) { return v; }
        if (auto v = parse_val_arr (parent); v) { return v; }
        if (auto v = parse_val_obj (parent); v) { return v; }
        throw ErrSyntax("invalid syntax", m_line_count);
        return nullptr;
    }

    void raise_bad_utf()
    {
        throw ErrSyntax("invalid string syntax: bad utf-16 codepoint", m_line_count);
    }

    ValImpl* add_val(ArrImpl* parent)
    {
        ValImpl* v = (parent)?
            &parent->add_element() :
            new ValImpl;
        v->m_line_no = m_line_count;
        return v;
    }

    ObjImpl* parse_val_obj(ArrImpl* parent)
    {
        ObjImpl* obj = nullptr;
        if (!skip_text("{")) return obj;
        obj = &add_val(parent)->init_obj();
        while (true) {
            skip_white_space();
            if (skip_text("}")) break;
            const char* name = parse_str();
            if (nullptr == name) {
                throw ErrSyntax("invalid object syntax: expected member name or '}'", m_line_count);
            }
            skip_white_space();
            if (!skip_text(":")) {
                throw ErrSyntax("invalid object syntax: expected ':' after member name", m_line_count);
            }
            skip_white_space();
            int32_t idx = obj->get_len();
            ValImpl* v = parse_val(obj);
            if (!obj->add_member(name, idx, *v)) {
                throw ErrSyntax("invalid object syntax: duplicate member name", m_line_count);
            }
            skip_white_space();
            if (skip_text("}")) break;
            if (!skip_text(",")) {
                throw ErrSyntax("invalid object syntax: expected ',' or '}'", m_line_count);
            }
        }
        return obj;
    }

    ArrImpl* parse_val_arr(ArrImpl* parent)
    {
        ArrImpl* arr = nullptr;
        if (!skip_text("[")) return arr;
        arr = &add_val(parent)->init_arr();
        while (true) {
            skip_white_space();
            if (skip_text("]")) break;
            std::ignore = parse_val(arr);
            skip_white_space();
            if (skip_text("]")) break;
            if (!skip_text(",")) {
                throw ErrSyntax("invalid array syntax: expected ',' or ']'", m_line_count);
            }
        }
        return arr;
    }

    ValImpl* parse_val_null(ArrImpl* parent)
    {
        ValImpl* v = nullptr;
        if (skip_text("null")) {
            v = add_val(parent);
        }
        return v;
    }

    ValImpl* parse_val_bool(ArrImpl* parent)
    {
        ValImpl* v = nullptr;
        bool b = false;
        if (skip_text("false")) {
            b = false;
        }
        else if (skip_text("true")) {
            b = true;
        }
        else {
            return v;
        }
        v = add_val(parent);
        v->init_bool(b);
        return v;
    }

    ValImpl* parse_val_num(ArrImpl* parent)
    {
        ValImpl* v = nullptr;
        bool negative = false;
        bool is_float = false;
        char* p = m_next;
        if ('-' == *p) {
            negative = true;
            p += 1;
        }
        char* num_start = p;
        while (*p >= '0' && *p <= '9') p += 1;
        if (p == m_next) return v; // not a number as we didn't encounter ('-', '0'...'9')
        if (p == num_start) {
            throw ErrSyntax("invalid number syntax: no digits after '-'", m_line_count);
        }
        if ('0' == *num_start && (p - num_start) > 1) {
            throw ErrSyntax("invalid number syntax: can't start with '0' if followed by another digit", m_line_count);
        }
        if ('.' == *p) {
            is_float = true;
            p += 1;
            while (*p >= '0' && *p <= '9') p += 1;
        }
        if ('E' == *p || 'e' == *p) {
            is_float = true;
            p += 1;
            if ('+' == *p || '-' == *p) p += 1;
            while (*p >= '0' && *p <= '9') p += 1;
        }
        if (!is_float) {
            // If integer exceeds -9223372036854775808 ... 9223372036854775807
            // we should raise an error as it will not fit in 64 bits.
            // Note this could be implemented via strtoll() and checking errno for ERANGE.
            // 
            // We test below that the absolute number will not exceed -a*10 + b
            // For this, we work with a negative value of 'n',
            // otherwise it may not fit the lower limit as a positive value.
            //
            const int64_t a = -922337203685477580;
            const int b = negative ? 8 : 7;
            int64_t n = 0;
            p = num_start;
            while (true) {
                char c = *p;
                if (c < '0' || c > '9') break;
                int d = c - '0';
                if (n < a || n == a && d > b) {
                    throw ErrSyntax("invalid number syntax: integer doesn't fit in 64 bits", m_line_count);
                }
                n = n * 10 - d;
                p += 1;
            }
            if (!negative) n = -n;
            v = add_val(parent);
            v->init_int(n);
        }
        else { // It is a float
            char* end = m_next;
            errno = 0;
            double n = std::strtod(m_next, &end);
            if (ERANGE == errno) {
                throw ErrSyntax("invalid number syntax: float is too huge", m_line_count);
            }
            if (end != p) {
                throw ErrSyntax("invalid number syntax: bad float format", m_line_count);
            }
            v = add_val(parent);
            v->init_f64(n);
        }
        m_next = p;
        return v;
    }

    ValImpl* parse_val_str(ArrImpl* parent)
    {
        ValImpl* v = nullptr;
        const char* str = parse_str();
        if (nullptr != str) {
            v = add_val(parent);
            v->init_str(str);
        }
        return v;
    }

    const char* parse_str()
    {
        const char* str = nullptr;
        if (!skip_text("\"")) return str;
        char* str_end = m_next;
        str = m_next;
        while (true) {
            char c = *m_next++;
            if ('"' == c) break;
            if (c == '\r' || c == '\n' || c == 0) {
                throw ErrSyntax("invalid string syntax: line ending before closing quotes", m_line_count);
            }
            if (c < ' ') {
                throw ErrSyntax("invalid string syntax: control characters not allowed", m_line_count);
            }
            if ('\\' == c) {
                switch (*(m_next++))
                {
                case '\\': c = '\\'; break;
                case '/':  c = '/' ; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u':
                    str_end = parse_encoding(str_end);
                    continue;
                default:
                    m_next -= 1;
                    throw ErrSyntax("invalid string syntax: bad escape character", m_line_count);
                }
            }
            *str_end++ = c;
        }
        *str_end = 0; // replace ending '"' with 0
        return str;
    }

    char* parse_encoding(char* str_end)
    {
        uint32_t code = parse_hex4();
        if (code >= 0xDC00 && code <= 0xDFFF) {
            raise_bad_utf(); // orphan low surrogate
        }
        if (code >= 0xD800 && code <= 0xDBFF) { // high surrogate
            // Expect next \uXXXX escape with low surrogate
            if (!skip_text("\\u")) {
                raise_bad_utf(); // low surrogate not specified
            }
            uint32_t code2 = parse_hex4();
            if (code2 < 0xDC00 || code2 > 0xDFFF) {
                raise_bad_utf(); // invalid low surrogate
            }
            code = (((code - 0xD800) << 10) | (code2 - 0xDC00)) + 0x10000;
        }
        if (code <= 0x0007F) {      // binary (0000 0000 0xxx xxxx) -> (0xxx xxxx)
            *str_end++ = static_cast<char>(code);
        }
        else if (code <= 0x007FF) { // binary (0000 0xxx xxyy yyyy) -> (110x xxxx) (10yy yyyy)
            *str_end++ = static_cast<char>(0xC0 | ((code >> 6) & 0xFF));
            *str_end++ = static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code <= 0x0FFFF) { // binary (xxxx yyyy yyzz zzzz) -> (1110 xxxx) (10yy yyyy) (10zz zzzz)
            *str_end++ = static_cast<char>(0xE0 | ((code >> 12) & 0xFF));
            *str_end++ = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            *str_end++ = static_cast<char>(0x80 | (code & 0x3F));
        }
        else { // code <= 0x10FFFF  // binary (000x xxyy yyyy zzzz zzuu uuuu) -> (1111 0xxx) (10yy yyyy) (10zz zzzz) (10uu uuuu)
            *str_end++ = static_cast<char>(0xF0 | ((code >> 18) & 0xFF));
            *str_end++ = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            *str_end++ = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            *str_end++ = static_cast<char>(0x80 | (code & 0x3F));
        }
        return str_end;
    }

    uint32_t parse_hex4()
    {
        uint32_t code = 0;
        char* p = m_next;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            code <<= 4;
            if (c >= '0' && c <= '9') {
                c -= '0';
            }
            else if (c >= 'A' && c <= 'F') {
                c -= 'A' - 10;
            }
            else if (c >= 'a' && c <= 'f') {
                c -= 'a' - 10;
            }
            else {
                raise_bad_utf(); // bad hex4 format
            }
            code += c;
        }
        m_next = p;
        return code;
    }

    bool skip_text(const char* str)
    {
        size_t len = strlen(str);
        if (0 != strncmp(m_next, str, len)) return false;
        m_next += len;
        return true;
    }

    void skip_white_space()
    {
        while (true) {
            if (' ' == *m_next || '\t' == *m_next) {
                m_next += 1;
                continue;
            }
            if ('\r' == *m_next || '\n' == *m_next) {
                skip_to_eol();
                continue;
            }
            if ('/' == m_next[0] && '/' == m_next[1]) {
                skip_to_eol();
                continue;
            }
            break;
        }
    }

    void skip_to_eol()
    {
        char* p = m_next;
        while (true) {
            const char c = *p;
            if (0 == c) break;
            p += 1;
            if ('\r' == c) {
                m_line_count++;
                if ('\n' == *p) p += 1; // Windows style new-line (CR LF), otherwise old Mac (CR)
                break;
            }
            if ('\n' == c) {            // Unix style new-line (LF)
                m_line_count++;
                break;
            }
        }
        m_next = p;
    }

private:
    char*   m_next;
    int32_t m_line_count;
};

Err::Err(const char* msg, int32_t line_no) noexcept
  : std::runtime_error(msg),
    line(line_no)
{
}

static std::string type_to_str(ValType t)
{
    const char* str = "";
    switch (t) {
    case vtNone: str = "none";  break;
    case vtNull: str = "nul";   break;
    case vtBool: str = "bool";  break;
    case vtInt : str = "int";   break;
    case vtF64 : str = "float"; break;
    case vtStr:  str = "str";   break;
    case vtArr:  str = "arr";   break;
    case vtObj:  str = "obj";   break;
    default:
        return std::to_string(t);
    }
    return str;
}

std::string Err::get_err_str() const
{
    std::string str = "(" + std::to_string(line) + "): " + what() + '\n';
    return str;
}

ErrValue::ErrValue(const char* msg, const Val& v) noexcept
    : Err(msg, v.get_line()),
      val_name(v.get_name()),
      val_idx(v.get_idx()),
      val_type(v.get_type())
{
}

std::string ErrValue::get_err_str() const
{
    std::string str = Err::get_err_str();
    if (val_name.size() > 0) {
        str += "  value name: " + val_name + '\n';
    }
    else if (val_idx >= 0) {
        str += "  value index: " + std::to_string(val_idx) + '\n';
    }
    if (val_type != vtNone) {
        str += "  val_type: " + type_to_str(val_type) + '\n';
    }
    return str;
}

ErrBadType::ErrBadType(const Val& v, ValType expected) noexcept
    : ErrValue("bad type", v),
    expected_type(expected)
{
}

std::string ErrBadType::get_err_str() const
{
    std::string str = ErrValue::get_err_str();
    if (expected_type != vtNone) {
        str += "  expected_type: " + type_to_str(expected_type) + '\n';
    }
    return str;
}

ErrBadIntRange::ErrBadIntRange(const Val& v, int64_t _lo, int64_t _hi) noexcept
    : ErrValue("bad integer range", v),
      lo(_lo),
      hi(_hi)
{
}

std::string ErrBadIntRange::get_err_str() const
{
    std::string str = ErrValue::get_err_str();
    if (lo <= hi) {
        str += "  expected range: " + std::to_string(lo) + " ... " + std::to_string(hi) + '\n';
    }
    return str;
}

ErrBadF64Range::ErrBadF64Range(const Val& v, double _lo, double _hi) noexcept
    : ErrValue("bad float range", v),
      lo(_lo),
      hi(_hi)
{
}

std::string ErrBadF64Range::get_err_str() const
{
    std::string str = ErrValue::get_err_str();
    if (lo <= hi) {
        str += "  expected range: " + std::to_string(lo) + " ... " + std::to_string(hi) + '\n';
    }
    return str;
}

ErrMemberNotFound::ErrMemberNotFound(const Obj& v, const char* name) noexcept
    : ErrValue("member not found", v)
{
    val_name = name;
    val_idx = -1;
    val_type = vtNone;
}

ErrUnknownMember::ErrUnknownMember(const Val& v) noexcept
    : ErrValue("unknown member", v)
{
}

ErrBadEnum::ErrBadEnum(const Val& v) noexcept
    : ErrValue("unsupported value", v)
{
}

void Json::clear() noexcept
{
    free_root();
    free_buf();
}

void Json::free_root() noexcept
{
    if (nullptr != m_root) {
        auto* v = &ValImpl::from(m_root);
        v->clear();
        delete v;
        m_root = nullptr;
    }
}

void Json::free_buf() noexcept
{
    delete[] m_buf;
    m_buf = nullptr;
}

const Val& Json::parse(const char* str, size_t len)
{
    clear();
    if (0 == len) {
        len = strlen(str);
    }
    m_buf = new char[len + 1];
    strncpy_s(m_buf, len + 1, str, len);
    m_buf[len] = 0;
    return parse_in_place(m_buf);
}

const Val& Json::parse_in_place(char* str)
{
    free_root();
    Parser p(str);
    m_root = p.parse();
    return *m_root;
}

}; // namespace ujson
