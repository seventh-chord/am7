#pragma once


void stack_enter_frame();
void stack_leave_frame();

void *stack_alloc_aligned(s64 size, s32 alignment);

// only work on the last allocation (last allocation is saved/restored on enter_frame/leave_frame)
void stack_trim_allocation(void *allocation, s64 new_size);
void stack_grow_allocation(void *allocation, s64 new_size);
s64 stack_get_allocation_size(void *allocation);

#define stack_alloc(type, count) ((type *) stack_alloc_aligned(sizeof(type) * (count), alignof(type)))
#define stack_grow_slice(slice, extra) (stack_grow_allocation((slice).data, ((slice).length + extra) * sizeof((slice).data[0])), (slice).length += extra)
#define stack_make_slice(type, count) (Slice<type> { stack_alloc(type, (count)), (count) })

template<typename Type>
Slice<Type> stack_copy(Slice<Type> source);

void *heap_alloc(s64 size);
void heap_free(void *pointer);
void *heap_grow(void *old_pointer, s64 new_size);
str heap_copy(str of);


struct Arena;
void *arena_alloc_aligned(Arena *arena, s32 size, s32 alignment);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);

template<typename Type>
Slice<Type> arena_copy(Arena *arena, Slice<Type> source);

#define arena_alloc(arena, type, count) ((type *) arena_alloc_aligned(arena, sizeof(type) * (count), alignof(type)))
#define arena_make_slice(arena, type, count) (Slice<type> { arena_alloc(arena, type, (count)), (count) })


struct StackAllocator
{
    enum { CAPACITY = 128*1024*1024 };
    u8 *data;
    u64 length;
    void **last_frame;

    void *last_allocation;
    s64 last_allocation_size;
};
global_variable StackAllocator global_stack_allocator = {0};

void *stack_alloc_aligned(s64 size, s32 alignment)
{
    if (!global_stack_allocator.data) global_stack_allocator.data = (u8 *) MemBigAlloc(StackAllocator::CAPACITY);
    global_stack_allocator.length = ((global_stack_allocator.length + alignment - 1) / alignment) * alignment;

    assert(global_stack_allocator.length + size <= StackAllocator::CAPACITY);
    void *result = &global_stack_allocator.data[global_stack_allocator.length];
    global_stack_allocator.length += size;

    global_stack_allocator.last_allocation = result;
    global_stack_allocator.last_allocation_size = size;

    memset(result, 0, size);
    return(result);
}

s64 stack_get_allocation_size(void *allocation)
{
    assert(allocation == global_stack_allocator.last_allocation);
    return(global_stack_allocator.last_allocation_size);
}

void stack_trim_allocation(void *allocation, s64 new_size)
{
    assert(allocation == global_stack_allocator.last_allocation);
    assert(new_size <= global_stack_allocator.last_allocation_size);
    global_stack_allocator.length -= global_stack_allocator.last_allocation_size - new_size;
    global_stack_allocator.last_allocation_size = new_size;
}

void stack_grow_allocation(void *allocation, s64 new_size)
{
    assert(allocation == global_stack_allocator.last_allocation);
    assert(new_size >= global_stack_allocator.last_allocation_size);
    s64 delta = new_size - global_stack_allocator.last_allocation_size;
    assert(global_stack_allocator.length + delta <= StackAllocator::CAPACITY);
    global_stack_allocator.length += delta;
    global_stack_allocator.last_allocation_size = new_size;
}

void stack_enter_frame()
{
    void *previous_last_allocation = global_stack_allocator.last_allocation;
    s64 previous_last_allocation_size = global_stack_allocator.last_allocation_size;

    void **marker = stack_alloc(void *, 3);
    marker[0] = previous_last_allocation;
    marker[1] = (void *) previous_last_allocation_size;
    marker[2] = global_stack_allocator.last_frame;
    global_stack_allocator.last_frame = marker;

    global_stack_allocator.last_allocation = 0;
    global_stack_allocator.last_allocation_size = 0;
}

void stack_leave_frame()
{
    assert(global_stack_allocator.last_frame);
    s64 old_length = global_stack_allocator.length;
    global_stack_allocator.length = ((u8 *) global_stack_allocator.last_frame) - global_stack_allocator.data;
    global_stack_allocator.last_allocation = global_stack_allocator.last_frame[0];
    global_stack_allocator.last_allocation_size = (s64) global_stack_allocator.last_frame[1];
    global_stack_allocator.last_frame = (void **) global_stack_allocator.last_frame[2];
    memset(global_stack_allocator.data + global_stack_allocator.length, 0, old_length - global_stack_allocator.length);
}

template<typename Type>
Slice<Type> stack_copy(Slice<Type> source)
{
    Slice<Type> copy = {};
    copy.data = stack_alloc(Type, source.length + 1);
    copy.length = source.length;
    memcpy(copy.data, source.data, source.length * sizeof(Type));
    memset(&copy.data[source.length], 0, sizeof(Type));
    return(copy);
}


enum { ARENA_DEFAULT_CAPACITY = 16*1024 };

struct ArenaPage
{
    s64 length;
    s64 capacity;
    ArenaPage *next;
    s64 _padding;
    u8 data[];
};

struct Arena
{
    ArenaPage *active;
    ArenaPage *free;
};

void *arena_alloc_aligned(Arena *arena, s32 size, s32 alignment)
{
    s32 max_size = size + alignment;
    if (!arena->active || arena->active->length + max_size > arena->active->capacity) {
        ArenaPage *new_page = null;
        for (ArenaPage *free = arena->free, *prev = null; free && !new_page; prev = free, free = free->next) {
            if (free->capacity >= max_size) {
                new_page = free;
                (prev? prev->next : arena->free) = free->next;
            }
        }
        if (!new_page) {
            s64 capacity = max(ARENA_DEFAULT_CAPACITY, round_up(max_size, 1024));
            new_page = (ArenaPage *) heap_alloc(sizeof(ArenaPage) + capacity);
            memset(new_page, 0, sizeof(ArenaPage));
            new_page->capacity = capacity;
        }

        new_page->next = arena->active;
        arena->active = new_page;
    }
    assert(arena->active && arena->active->length + max_size <= arena->active->capacity);

    s64 offset = arena->active->length;
    offset = round_up(offset, alignment);
    u8 *result = &arena->active->data[offset];
    arena->active->length = offset + size;
    return((void *) result);
}

void arena_reset(Arena *arena)
{
    while (arena->active) {
        ArenaPage *page = arena->active;
        arena->active = page->next;

        page->length = 0;
        page->next = arena->free;
        arena->free = page;
    }
}

void arena_free(Arena *arena)
{
    arena_reset(arena);
    while (arena->free) {
        void *pointer = arena->free;
        arena->free = arena->free->next;
        heap_free(pointer);
    }
}

template<typename Type>
Slice<Type> arena_copy(Arena *arena, Slice<Type> source)
{
    Slice<Type> copy = {};
    copy.data = arena_alloc(arena, Type, source.length + 1);
    memcpy(copy.data, source.data, source.length * sizeof(Type));
    memset(&copy.data[source.length], 0, sizeof(Type));
    copy.length = source.length;
    return(copy);
}



#include "win32.hpp"

void *heap_alloc(s64 size)
{
    void *result = null;
    if (size) {
        void *heap = win32::GetProcessHeap();
        result = win32::HeapAlloc(heap, 0, size);
        assert(result);
    }
    return(result);
}
void heap_free(void *pointer)
{
    if (pointer) {
        void *heap = win32::GetProcessHeap();
        s32 result = win32::HeapFree(heap, 0, pointer);
        assert(result);
    }
}
void *heap_grow(void *old, s64 new_size)
{
    void *result = old;
    if (new_size) {
        void *heap = win32::GetProcessHeap();
        if (!result) {
            result = win32::HeapAlloc(heap, 0, new_size);
        } else if (new_size > 0) {
            result = win32::HeapReAlloc(heap, 0, old, new_size);
        }
        assert(result);
    }
    return(result);
}

str heap_copy(str of)
{
    str result = {};
    result.length = of.length;
    result.data = (char *) heap_alloc(of.length + 1);
    memcpy(result.data, of.data, of.length);
    return(result);
}