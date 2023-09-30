#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

//#include <mmintrin.h>  // MMX
#include <xmmintrin.h> // SSE
#include <emmintrin.h> // SSE2
#include <pmmintrin.h> // SSE3
#include <tmmintrin.h> // SSSE3
#include <smmintrin.h> // SSE4.1
//#include <nmmintrin.h> // SSE4.2
//#include <ammintrin.h> // SSE4A
#include <wmmintrin.h> // AES
//#include <immintrin.h> // AVX, AVX2, FMA

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOFLOAT
#include "stb_sprintf.h"

#include "meowhash.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef wchar_t wchar;
#define null nullptr

#define global_variable static

#if defined(_MSC_VER) && !defined(__clang__)
#define noinline_function __declspec(noinline)
#else
#define noinline_function __attribute__((noinline))
#endif

#define U8_MAX   0xff
#define U16_MAX  0xffff
#define U32_MAX  0xffffffffull
#define U64_MAX  0xffffffffffffffffull

#define S8_MAX   127
#define S8_MIN   -128
#define S16_MAX  32767
#define S16_MIN  -32768
#define S32_MAX  2147483647l
#define S32_MIN  -2147483648l
#define S64_MAX  9223372036854775807ll
#define S64_MIN  -9223372036854775808ll

template<typename t>
t _typeof_remove_reference(t);
#define typeof(X) decltype(_typeof_remove_reference(X))

#define array_length(x) (sizeof(x) / sizeof(*(x)))
#define alen(x) array_length(x)
#define array_as_slice(x) Slice<typeof(x[0])>{ x, array_length(x) }

#define max(a, b) ((a) > (b)? (a) : (b))
#define min(a, b) ((a) < (b)? (a) : (b))
#define clamp(x, min, max) ((x) < (min)? (min) : ((x) > (max)? (max) : (x)))
#define abs(a) ((a) < 0? -(a) : (a))
#define sign(a) ((a) == 0? 0 : ((a) > 0? 1 : -1))

#define swap(a, b) { auto _swap_temp = (a); (a) = (b); (b) = _swap_temp; }

#define static_assert(x) static_assert(x, "static_assert failed: "#x)

#define macro_stringize(x) _macro_stringize(x)
#define _macro_stringize(x) #x

#define assert(cond) ((cond)? ((void) 0) : _assert(true, __FILE__ "(" macro_stringize(__LINE__) "): assert(%s) failed\n", #cond))
#define assert_soft(cond) ((cond)? ((void) 0) : _assert(false, __FILE__ "(" macro_stringize(__LINE__) "): assert(%s) failed\n", #cond))
#define unimplemented() _assert(true, __FILE__ "(" macro_stringize(__LINE__) "): Reached unimplemented code\n")
#define fail(message, ...) _assert(true, message, __VA_ARGS__)

#define unused(x) (void)(x)


void _assert(bool hard, char *message, ...);

#if defined(DEBUG)
#define debug_assert(cond) assert(cond)
#else
#define debug_assert(cond) ((void)(cond))
#endif

typedef void (*proc_address)();


#define lit_to_str(x) { (x), (sizeof(x) / sizeof(x[0])) - 1 }


struct Range
{
    s32 min;
    s32 max;
};

struct s32v2
{
    s32 x;
    s32 y;
};

struct Rect
{
    union {
        struct {
            s32v2 min;
            s32v2 max;
        };
        struct {
            s32 x0, y0;
            s32 x1, y1;
        };
    };
};

s32v2 operator+(s32v2 left, s32v2 right)
{
    s32v2 sum = {};
    sum.x = left.x + right.x;
    sum.y = left.y + right.y;
    return(sum);
}

bool rect_contains(Rect rect, s32v2 p)
{
    return(p.x >= rect.x0 && p.y >= rect.y0 && p.x < rect.x1 && p.y < rect.y1);
}

s32 rect_external_distance(Rect rect, s32v2 p)
{
    s32 distance = 0;
    if (p.x < rect.x0) {
        if (p.y < rect.y0) {
            distance = (rect.x0 - p.x)*(rect.y0 - p.y);
        } else if (p.y > rect.y1) {
            distance = (rect.x0 - p.x)*(p.y - rect.y1);
        } else {
            distance = rect.x0 - p.x;
            distance *= distance;
        }
    } else if (p.x > rect.x1) {
        if (p.y < rect.y0) {
            distance = (p.x - rect.x1)*(rect.y0 - p.y);
        } else if (p.y > rect.y1) {
            distance = (p.x - rect.x1)*(p.y - rect.y1);
        } else {
            distance = p.x - rect.x1;
            distance *= distance;
        }
    }
    return(distance);
}


bool is_power_of_two(u64 value);
u64 next_power_of_two(u64 value);
s32 log2(u64 value);
s32 log10(u64 value);
u64 round_up(u64 value, u64 step);
u64 round_down(u64 value, u64 step);
bool mem_is_zero(void *data, u64 length);

template<typename Type>
struct Slice
{
    Type *data;
    s64 length;

    Type &operator[](s64 index)
    {
        debug_assert(index >= 0 && index < this->length);
        return(this->data[index]);
    }
};

typedef Slice<char> str;
typedef Slice<wchar> wstr;

#define str_fmt(SomeString) ((s32) (SomeString).length), (SomeString).data
str stack_printf(char *message, ...);
void stack_printf_append(str *buffer, char *message, ...);
void stack_printf_append_va(str *result, char *message, va_list arguments);

void debug_printf(char *message, ...);
void app_debug_print_hook(str string);


void *MemBigAlloc(s64 Size);
void MemBigFree(void *Pointer);

#include "allocators.hpp"
#include "unicode.h"

template<typename Type>
struct Array
{
    Type *data;
    s64 length;
    s64 capacity;

    Type &operator[](s64 index)
    {
        debug_assert(index >= 0 && index < this->length);
        return(this->data[index]);
    }

    void ensure_length(s64 min_length)
    {
        if (min_length > this->capacity) {
            s64 new_capacity = max(64, max(this->capacity*2, min_length));
            this->data = (Type *) heap_grow(this->data, new_capacity * sizeof(Type));
            this->capacity = new_capacity;
        }
    }

    void make_space(s64 extra)
    {
        this->ensure_length(this->length + extra);
    }

    Type *push(s64 count = 1)
    {
        this->ensure_length(this->length + count);

        Type *result = &this->data[this->length];
        this->length += count;
        memset(result, 0, count * sizeof(Type));
        return(result);
    }

    void append(Type element)
    {
        Type *slot = this->push(1);
        *slot = element;
    }

    Type *pop(s64 count = 1)
    {
        this->length -= count;
        assert(this->length > 0);
        Type *result = &this->data[this->length];
        return(result);
    }

    Type remove_ordered(s64 index)
    {
        debug_assert(index >= 0 && index < this->length);
        Type result = this->data[index];
        for (s64 i = index + 1; i < this->length; i += 1) {
            this->data[i - 1] = this->data[i];
        }
        this->length -= 1;
        return(result);
    }

    void insert_ordered(s64 index, Type element)
    {
        this->push(1);
        for (s64 i = this->length - 1; i > index; i--) {
            this->data[i] = this->data[i - 1];
        }
        this->data[index] = element;
    }

    Slice<Type> as_slice()
    {
        Slice<Type> result = {};
        result.data = this->data;
        result.length = this->length;
        return(result);
    }

    void clear()
    {
        this->length = 0;
    }

    void free()
    {
        if (this->data) heap_free(this->data);
        *this = {};
    }
};

template<typename Type>
struct GapArray
{
    Type *data;
    s32 a, b, cap;
    s32 length;

    Type &operator[](s32 index)
    {
        if (index >= this->a) {
            index += this->b - this->a;
            debug_assert(index <= this->cap);
        } else {
            debug_assert(index >= 0);
        }
        return(this->data[index]);
    }

    void _move_gap(s32 offset)
    {
        if (this->a < offset) {
            s32 delta = offset - this->a;
            memcpy(this->data + this->a, this->data + this->b, delta * sizeof(Type));
            this->a += delta;
            this->b += delta;
        } else if (this->a > offset) {
            s32 delta = this->a - offset;
            this->a -= delta;
            this->b -= delta;
            memcpy(this->data + this->b, this->data + this->a, delta * sizeof(Type));
        }
    }

    s32 _length()
    {
        return(this->a + this->cap - this->b);
    }

    void insert(s32 offset, Type value)
    {
        if (this->b - this->a <= 0) {
            s32 new_cap = max(this->cap*2, 128);
            s32 new_b = new_cap - this->cap + this->b;
            Type *new_data = (Type *) heap_alloc(new_cap * sizeof(Type));
            if (this->data) {
                memcpy(new_data, this->data, this->a*sizeof(Type));
                memcpy(new_data + new_b, this->data + this->b, (this->cap - this->b)*sizeof(Type));
                heap_free(this->data);
            }
            this->b = new_b;
            this->cap = new_cap;
            this->data = new_data;
        }

        this->_move_gap(offset);

        this->data[this->a] = value;
        ++this->a;
        this->length = this->_length();
    }

    void remove_range(s32 min, s32 max)
    {
        assert(0 <= min && min <= max && max <= this->length);
        this->_move_gap(max);
        this->a = min;
        this->length = this->_length();
    }

    void get_parts(Slice<Type> parts[2])
    {
        parts[0].data = this->data;
        parts[0].length = this->a;
        parts[1].data = this->data + this->b;
        parts[1].length = this->cap - this->b;
    }

    void clear()
    {
        this->a = 0;
        this->b = cap;
        this->length = 0;
    }

    void free()
    {
        if (this->data) heap_free(this->data);
        *this = {};
    }
};


#define for_each(it, slice) for (auto it = &(slice).data[0], _end_of_##it = &(slice).data[(slice).length]; it != _end_of_##it; ++it)
#define for_each_remove(it, slice) (memmove(it, it + 1, sizeof(*it) * ((&(slice).data[(slice).length]) - (it + 1))), (slice).length--, it--, _end_of_##it--)
#define for_each_index(it, slice) ((it) - (slice).data)


u64 hash_good_64(str of)
{
    meow_u128 raw = MeowHash(MeowDefaultSeed, of.length, of.data);
    u64 result = MeowU64From(raw, 0);
    return(result);
}

u32
hash_fnv1a(void *datav, int size)
{
    uint32_t hash = 2166136261;
    char *data = (char *) datav;
    while (size--) hash = (hash ^ *data++) * 16777619;
    return(hash);
}

u32
hash_fnv1a(char *data)
{
    uint32_t hash = 2166136261;
    while (*data) hash = (hash ^ *data++) * 16777619;
    return(hash);
}

u32 hash_rehash_32(u32 hash)
{
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;
    return(hash);
}

struct HashMap32
{
    struct Slot
    {
        u32 key;
        u32 value;
    };

    enum {
        EMPTY = 0,
        TOMB = 1,
    };

    Slot *slots;
    s32 capacity;
    u32 capacity_mask;
    s32 internal_count;

    u32 extra_slots[2];
    u32 extra_slots_occupied;

    void _grow()
    {
        if ((this->internal_count + 1)*3/2 > this->capacity) {
            Slot *old_slots = this->slots;
            s32 old_capacity = this->capacity;

            s32 new_capacity = max(this->capacity*2, 128);
            assert(new_capacity > this->capacity);
            Slot *new_slots = (Slot *) heap_alloc(new_capacity * sizeof(Slot));
            memset(new_slots, 0, new_capacity * sizeof(Slot));
            this->slots = new_slots;
            this->capacity = new_capacity;
            this->capacity_mask = new_capacity - 1;
            this->internal_count = 0;

            if (old_slots) {
                for (s32 i = 0; i < old_capacity; ++i) {
                    if (old_slots[i].key >= 2) {
                        this->insert(old_slots[i].key, old_slots[i].value);
                    }
                }
                heap_free(old_slots);
            }
        }
        ++this->internal_count;
    }

    void insert(u32 key, u32 value)
    {
        if (key < 2) {
            this->extra_slots[key] = value;
            this->extra_slots_occupied |= 1 << key;
        } else {
            this->_grow();
            s32 index = hash_rehash_32(key) & this->capacity_mask;
            while (this->slots[index].key >= 2) index = (index + 1) & this->capacity_mask;
            this->slots[index] = { key, value };
        }
    }

    bool get(u32 key, u32 *value)
    {
        if (key < 2) {
            bool available = this->extra_slots_occupied & (1 << key);
            if (available) *value = this->extra_slots[key];
            return(available);
        } else if (this->capacity) {
            s32 index = hash_rehash_32(key) & this->capacity_mask;
            while (true) {
                s32 current_key = this->slots[index].key;
                if (current_key == key) {
                    *value = this->slots[index].value;
                    return(true);
                }
                if (current_key == EMPTY) {
                    return(false);
                }
                index = (index + 1) & this->capacity_mask;
            }
        }
        return(false);
    }

    void clear()
    {
        for (s32 i = 0; i < this->capacity; ++i) {
            this->slots[i].key = EMPTY;
        }
    }
};


str cstring_to_str(char *cstring);
wstr cstring_to_wstr(wchar *cstring);
s64 cstring_length(char *cstring, s64 max = S64_MAX);
s64 cstring_length(wchar *cstring, s64 max = S64_MAX);

template<typename Left, typename Right>
bool contains(Slice<Left> haystack, Right needle);
template<typename Type>
Slice<Type> slice(Slice<Type> from, s64 start = 0, s64 one_past_end = S64_MIN);

template<typename Type>
void stable_sort(Slice<Type> slice, bool (*in_proper_order)(Type *left, Type *right));
bool in_lexicographic_order(str *Left, str *Right);
bool in_length_order(str *left, str *right);

struct CmdArg
{
    str narrow;
    wstr wide;
};
Slice<CmdArg> get_command_line_arguments();

str cstring_to_str(char *cstring)
{
    str result = {};
    result.data = cstring;
    if (cstring) {
        while (*cstring) {
            ++cstring;
            ++result.length;
        }
    }
    return(result);
}

wstr cstring_to_wstr(wchar *cstring)
{
    wstr result = {};
    result.data = cstring;
    if (cstring) {
        while (*cstring) {
            ++cstring;
            ++result.length;
        }
    }
    return(result);
}

s64 cstring_length(char *cstring, s64 max)
{
    s64 length = 0;
    if (cstring) while (length < max && *cstring++) ++length;
    return(length);
}

s64 cstring_length(wchar *cstring, s64 max)
{
    s64 length = 0;
    if (cstring) while (length < max && *cstring++) ++length;
    return(length);
}

bool operator==(str a, str b)
{
    return(a.length == b.length && (memcmp(a.data, b.data, a.length) == 0));
}

bool operator!=(str a, str b)
{
    return(!(a == b));
}

bool operator==(str a, char *b)
{
    return(a == cstring_to_str(b));
}
bool operator==(char *a, str b)
{
    return(cstring_to_str(a) == b);
}
bool operator!=(str a, char *b)
{
    return(!(a == cstring_to_str(b)));
}
bool operator!=(char *a, str b)
{
    return(!(cstring_to_str(a) == b));
}

bool operator==(wstr a, wstr b)
{

    if (a.length == b.length) {
        return(memcmp(a.data, b.data, a.length * sizeof(wchar_t)) == 0);
    } else {
        return(false);
    }
}

bool operator!=(wstr a, wstr b)
{
    return(!(a == b));
}

bool operator==(wstr a, wchar *b)
{
    return(a == cstring_to_wstr(b));
}
bool operator==(wchar *a, wstr b)
{
    return(cstring_to_wstr(a) == b);
}
bool operator!=(wstr a, wchar *b)
{
    return(!(a == cstring_to_wstr(b)));
}
bool operator!=(wchar *a, wstr b)
{
    return(!(cstring_to_wstr(a) == b));
}

template<typename Type>
Slice<Type> slice(Slice<Type> from, s64 start, s64 one_past_end)
{
    if (start < 0) {
        start = from.length + start;
    }
    if (one_past_end == S64_MIN) {
        one_past_end = from.length;
    } else if (one_past_end < 0) {
        one_past_end = from.length + one_past_end;
    }

    assert(start >= 0 && start <= from.length);
    assert(one_past_end >= 0 && one_past_end <= from.length);
    assert(one_past_end >= start);

    Slice<Type> result = {};
    result.data = from.data + start;
    result.length = one_past_end - start;
    return(result);
}

// NB This is the most basic string compare function I could think of, and it really is not what you want if you are doing any serious sorting.
bool in_lexicographic_order(str *left, str *right)
{
    s32 min_length = min(left->length, right->length);
    for (s32 i = 0; i < min_length; i += 1) {
        char l = (*left)[i];
        if (l >= 'a' && l <= 'z') l -= 'a' + 'A';
        char r = (*right)[i];
        if (r >= 'a' && r <= 'z') r -= 'a' + 'A';

        if (l < r) return true;
        if (l > r) return false;
    }
    return(left->length <= right->length);
}

bool in_length_order(str *left, str *right)
{
    return(left->length <= right->length);
}

template<typename Type>
void stable_sort(Slice<Type> slice, bool (*in_proper_order)(Type *left, Type *right))
{
    // Insertion sort
    for (s64 i = 1; i < slice.length; i += 1) {
        if (!(*in_proper_order)(&slice[i - 1], &slice[i])) {
            Type temp = slice[i];
            s64 j = i;
            do {
                slice[j] = slice[j - 1];
                --j;
            } while (j > 0 && !(*in_proper_order)(&slice[j - 1], &temp));
            slice[j] = temp;
        }
    }
}

template<typename Left, typename Right>
bool contains(Slice<Left> haystack, Right needle)
{
    bool found = false;
    for (s64 i = 0; i < haystack.length && !found; ++i) found = haystack[i] == needle;
    return(found);
}


bool is_power_of_two(u64 Value)
{
    return((Value & (Value - 1)) == 0);
}

u64 next_power_of_two(u64 Value)
{
    Value -= 1;
    Value |= Value >> 1;
    Value |= Value >> 2;
    Value |= Value >> 4;
    Value |= Value >> 8;
    Value |= Value >> 16;
    Value |= Value >> 32;
    Value += 1;
    return(Value);
}

s32 log2(u64 Value)
{
    s32 Result = 0;
    for (s32 i = 0; i < 64; i += 1) {
        if (Value & 1) {
            Result = i + 1;
        }
        Value >>= 1;
    }
    return(Result);
}

s32 log10(u64 Value)
{
    s32 Result = 1;
    u64 Target = 10;
    while (Target <= Value) {
        Target *= 10;
        Result += 1;
    }
    return(Result);
}

u64 round_up(u64 value, u64 step)
{
    return(((value + step - 1) / step) * step);
}

u64 round_down(u64 value, u64 step)
{
    return((value / step) * step);
}

bool mem_is_zero(void *data_raw, u64 length)
{
    u8 *data = (u8 *) data_raw;
    u8 *data_end = data + length;
    while (data < data_end) if (*data++) return(false);
    return(true);
}


#include "win32.hpp"

void _assert(bool hard, char *message, ...)
{
    // Note (Morten, 2020-08-11) To avoid issues with assertions in different threads, we don't use functions depending on the stack allocator here (including stack_printf and utf8_to_utf16)
    static bool _inside_assertion = false;
    if (!_inside_assertion) {
        _inside_assertion = true;

        char buffer[1024];
        va_list arguments;
        va_start(arguments, message);
        stbsp_vsnprintf(buffer, alen(buffer), message, arguments);
        va_end(arguments);

        str narrow = cstring_to_str(buffer);

        wchar_t wide_buffer[1024];
        win32::MultiByteToWideChar(win32::CP_UTF8, 0, buffer, -1, wide_buffer, alen(wide_buffer));
        wstr wide = cstring_to_wstr(wide_buffer);

        if (win32::IsDebuggerPresent()) {
            win32::OutputDebugStringW(wide.data);
            win32::OutputDebugStringW(L"\n");
            win32::DebugBreak();
        }

        #if defined(subsystem_windows)
        win32::MessageBoxW(win32::GetActiveWindow(), wide.data, hard? (wchar_t *) L"Critical error" : (wchar_t *) L"Error", win32::MB_OK | win32::MB_ICONERROR);
        #elif defined(subsystem_console)
        void *my_stderr = win32::GetStdHandle((u32) -12);
        u32 temp = 0;
        if (win32::GetConsoleMode(my_stderr, &temp)) {
            win32::WriteConsoleW(my_stderr, wide.data, wide.length, &temp, 0);
        } else {
            win32::WriteFile(my_stderr, narrow.data, narrow.length, &temp, 0);
        }
        #endif

        _inside_assertion = false;
    }

    if (hard) win32::ExitProcess(1);
}

void debug_printf(char *message, ...)
{
    stack_enter_frame();

    str formated = {};

    va_list arguments;
    va_start(arguments, message);
    stack_printf_append_va(&formated, message, arguments);
    va_end(arguments);

    wstr wide_message = utf8_to_utf16(formated);
    unused(wide_message); // maybe

    #if defined(DEBUG)
    win32::OutputDebugStringW(wide_message.data);
    #endif

    #if defined(subsystem_console)
    static void *my_stdout = null;
    static bool my_stdout_is_console;
    if (!my_stdout) {
        my_stdout = win32::GetStdHandle((u32) -11);
        u32 temp;
        my_stdout_is_console = (bool) win32::GetConsoleMode(my_stdout, &temp);
    }

    u32 temp;
    if (my_stdout_is_console) {
        assert(win32::WriteConsoleW(my_stdout, wide_message.data, wide_message.length, &temp, null));
    } else {
        assert(win32::WriteFile(my_stdout, formated.data, formated.length, &temp, null));
    }
    #endif

    static bool in_app_debug_print_hook = false;
    if (!in_app_debug_print_hook) {
        in_app_debug_print_hook = true;
        app_debug_print_hook(formated);
        in_app_debug_print_hook = false;
    }

    stack_leave_frame();
}

str stack_printf(char *message, ...)
{
    str result = {};

    va_list arguments;
    va_start(arguments, message);
    stack_printf_append_va(&result, message, arguments);
    va_end(arguments);

    return(result);
}

void stack_printf_append(str *buffer, char *message, ...)
{
    va_list arguments;
    va_start(arguments, message);
    stack_printf_append_va(buffer, message, arguments);
    va_end(arguments);
}

void stack_printf_append_va(str *buffer, char *message, va_list arguments)
{
    va_list arguments2;
    va_copy(arguments2, arguments);
    int required_length = stbsp_vsnprintf(null, 0, message, arguments2);
    va_end(arguments2);

    char *write_to;
    if (buffer->data) {
        stack_grow_allocation(buffer->data, buffer->length + required_length + 1);
        write_to = buffer->data + buffer->length;
        buffer->length += required_length;
    } else {
        buffer->data = stack_alloc(char, required_length + 1);
        buffer->length = required_length;
        write_to = buffer->data;

    }
    
    int actual_length = stbsp_vsnprintf(write_to, required_length + 1, message, arguments);
    assert(actual_length == required_length);
}

void stack_append(str *buffer, str extra)
{
    if (!buffer->data) {
        buffer->data = stack_alloc(char, extra.length);
    } else {
        stack_grow_allocation(buffer->data, buffer->length + extra.length);
    }
    memcpy(buffer->data + buffer->length, extra.data, extra.length);
    buffer->length += extra.length;
}


void *MemBigAlloc(s64 size)
{
    void *result = win32::VirtualAlloc(null, size, win32::MEM_RESERVE | win32::MEM_COMMIT, win32::PAGE_READWRITE);
    if (!result) {
        u32 error_code = win32::GetLastError();
        fail("Couldn't allocate virtual memory: %u\n", error_code);
    }
    return(result);
}

void MemBigFree(void *data)
{
    win32::VirtualFree(data, 0, win32::MEM_RELEASE);
}


enum
{
    SECONDS = 1,
    MILLISECONDS = 1000,
    MICROSECONDS = 1000000,
    NANOSECONDS  = 1000000000,
};

typedef s64 Time;

Time time_read()
{
    s64 now;
    win32::QueryPerformanceCounter(&now);
    return(now);
}

s64 time_convert(Time start, Time end, s64 unit)
{
    s64 frequency;
    win32::QueryPerformanceFrequency(&frequency);
    return((end - start) * unit / frequency);
}

Slice<CmdArg> get_command_line_arguments()
{
    wchar *argument_string = win32::GetCommandLineW();
    s32 count = 0;
    wchar **arguments = win32::CommandLineToArgvW(argument_string, &count);

    Slice<CmdArg> result = stack_make_slice(CmdArg, count);

    for (s32 i = 0; i < count; ++i) {
        CmdArg *arg = &result[i];
        arg->wide = stack_copy(cstring_to_wstr(arguments[i]));
        arg->narrow = utf16_to_utf8(arg->wide);
    }

    win32::LocalFree(arguments);
    return(result);
}