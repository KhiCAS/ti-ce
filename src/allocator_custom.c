#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lcd.h>
#include <debug.h>

// This allocator uses the 2nd part of the VRAM as another area for heap allocations, providing 76KB more memory,
// but requires you to only use the first part of the VRAM for the LCD configured in 8bpp.

// When passed to malloc, returns how much free memory there's left
#define MAGIC_SIZE_QUERY_FREEMEM 0xFFFFFF

// minimal size (avoid blocks that are too small)
#define MALLOC_MINSIZE 6

typedef struct char2_ {
    char c1, c2, c3, c4;
} char2_t;

typedef struct char3_ {
    char c1, c2, c3, c4, c5, c6, c7, c8, c9, c10;
} char3_t;

typedef struct char6_ {
    char c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16;
} char6_t;

typedef struct __attribute__((packed)) block
{
    struct block* ptr;
    size_t size;
} __attribute__((packed)) block_t;

extern uint8_t __heap_low[];
extern uint8_t __heap_high[];

// assumes that heap2_ptrend<=lcd_Ram
static uintptr_t heap2_ptr = (uintptr_t)__heap_low;
static uintptr_t heap2_ptrend = (uintptr_t)__heap_high;

// 256 slots per pool: byte bitmaps avoid expensive 24-bit divisions on eZ80.
#define ALLOC2 256
#define ALLOC3 256
#define ALLOC6 256

struct small_pool {
    unsigned char used[32];
    unsigned char hint; // first bitmap byte that may have a free slot
};

static struct small_pool pool2, pool3, pool6;
static const unsigned char pool_bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};

static int pool_take(struct small_pool* p)
{
    for (unsigned char i = p->hint; i < sizeof(p->used); ++i)
    {
        const unsigned char u = p->used[i];
        if (u != 0xff)
        {
            unsigned char f = ~u, k = 0;
            while (!(f & 1)) { f >>= 1; ++k; }
            p->used[i] = u | pool_bit[k];
            p->hint = i;
            return (unsigned char)(i << 3) | k;
        }
    }
    p->hint = sizeof(p->used);
    return -1;
}

static void pool_give(struct small_pool* p, unsigned char pos)
{
    const unsigned char i = pos >> 3;
    p->used[i] &= ~pool_bit[pos & 7];
    if (i < p->hint) p->hint = i;
}

#define LCD_SIZE_8BPP (LCD_WIDTH * LCD_HEIGHT)
#define ALLOC_START ((unsigned char*)lcd_Ram + LCD_SIZE_8BPP)
#define ALLOC_END ((unsigned char*)lcd_Ram + LCD_SIZE)

static char2_t * const tab2 = (char2_t*)(ALLOC_START);
static char3_t * const tab3 = (char3_t*)(ALLOC_START + ALLOC2 * sizeof(char2_t));
static char6_t * const tab6 = (char6_t*)(ALLOC_START + ALLOC2 * sizeof(char2_t) + ALLOC3 * sizeof(char3_t));

static uintptr_t heap_ptr = (uintptr_t)(ALLOC_START + ALLOC2 * sizeof(char2_t) + ALLOC3 * sizeof(char3_t) + ALLOC6 * sizeof(char6_t));
static uintptr_t heap_ptrend = (uintptr_t)ALLOC_END;

static block_t _alloc_base, _alloc2_base;
// these are 0 initialized, pointing to a chained list of freed pointers

// Optional emergency reserve and callback for a caller-managed OOM recovery.
// Without a reserve, retain the existing abort behavior unless soft mode is set.
#define OOM_RESERVE 8192
void* oom_reserve = NULL;
volatile char oom_hit = 0, oom_soft = 0;
typedef void oom_interrupt_t(void);
oom_interrupt_t* oom_interrupt = NULL;
void free(void* ptr);

void* malloc(size_t alloc_size)
{
    // dbg_printf("[malloc] %zu bytes\n", alloc_size);

    if (alloc_size == 0)
        return NULL;

    // This allows callers to query how much free memory there is left
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM)
        return (void*)((heap_ptrend - heap_ptr) + (heap2_ptrend - heap2_ptr));
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM - 1)
    {
        size_t total = 0;
        for (block_t* b = _alloc_base.ptr; b; b = b->ptr) total += b->size;
        for (block_t* b = _alloc2_base.ptr; b; b = b->ptr) total += b->size;
        return (void*)total;
    }
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM - 2)
    {
        size_t big = 0;
        for (block_t* b = _alloc_base.ptr; b; b = b->ptr)
            if (b->size > big) big = b->size;
        for (block_t* b = _alloc2_base.ptr; b; b = b->ptr)
            if (b->size > big) big = b->size;
        return (void*)big;
    }

    if (alloc_size <= sizeof(char6_t))
    {
        int pos;
        if (alloc_size <= sizeof(char2_t) && (pos = pool_take(&pool2)) >= 0)
            return (void*)(tab2 + pos);
        if (alloc_size <= sizeof(char3_t) && (pos = pool_take(&pool3)) >= 0)
            return (void*)(tab3 + pos);
        if ((pos = pool_take(&pool6)) >= 0)
            return (void*)(tab6 + pos);
    }

    block_t* q;
    block_t* r;

    /* add size of block header to real size */
    size_t size = alloc_size + sizeof(block_t);
    if (size < alloc_size)
        return NULL;

    // dbg_printf("alloc heap %p ptr=%p size=%zu\n",&_alloc_base,_alloc_base.ptr,_alloc_base.size);

    for (block_t* p = &_alloc_base; (q = p->ptr); p = q)
    {
        if (q->size >= size)
        {
            if (q->size <= size + sizeof(block_t) + MALLOC_MINSIZE)
            {
                // dbg_printf("heap recycle full blocsize=%zu size=%zu\n", q->size, size);
                p->ptr = q->ptr;
            }
            else
            {
                // dbg_printf("heap recycle partial blocsize=%zu size=%zu\n", q->size, size);
                q->size -= size;
                q = (block_t*)(((uint8_t*)q) + q->size);
                q->size = size;
            }

            return q + 1;
        }
    }

    /* compute next heap pointer */
    if (heap_ptr + size < heap_ptr || heap_ptr + size >= (uintptr_t)heap_ptrend)
    {
        // dbg_printf("alloc heap2 %p ptr=%p size=%zu\n",&_alloc2_base,_alloc2_base.ptr,_alloc2_base.size);
        for (block_t* p = &_alloc2_base; (q = p->ptr); p = q)
        {
            if (q->size >= size)
            {
                if (q->size <= size + sizeof(block_t))
                {
                    // dbg_printf("heap2 recycle full blocsize=%zu size=%zu\n", q->size, size);
                    p->ptr = q->ptr;
                }
                else
                {
                    // dbg_printf("heap2 recycle partial blocsize=%zu size=%zu\n", q->size, size);
                    q->size -= size;
                    q = (block_t*)(((uint8_t*)q) + q->size);
                    q->size = size;
                }

                return q + 1;
            }
        }
        if (
            (heap2_ptr + size < heap2_ptr) ||
            (heap2_ptr + size >= (uintptr_t)heap2_ptrend)
        ) {
            if (oom_reserve && alloc_size <= OOM_RESERVE - 16)
            {
                void* res = oom_reserve;
                oom_reserve = NULL;
                oom_hit = 1;
                free(res);
                if (oom_interrupt) oom_interrupt();
                return malloc(alloc_size);
            }
            if (oom_soft)
                return NULL;
            lcd_Control = 0b100100101101; // TI-OS default
            abort();
            return NULL;
        }
        r = (block_t*)heap2_ptr;
        if (size < MALLOC_MINSIZE)
            size = MALLOC_MINSIZE;
        r->size = size;
        heap2_ptr = heap2_ptr + size;
        return r + 1;
    }

    if (size < MALLOC_MINSIZE)
        size = MALLOC_MINSIZE;

    r = (block_t*)heap_ptr;
    r->size = size;
    heap_ptr = heap_ptr + size;
    return r + 1;
}

void free(void* ptr)
{
    // dbg_printf("[free] %p\n", ptr);

    if (ptr == NULL)
    {
        return;
    }

    size_t o = (size_t)ptr - (size_t)tab2;
    if (o < ALLOC2 * sizeof(char2_t))
    {
        pool_give(&pool2, (unsigned char)(o >> 2));
        return;
    }
    o = (size_t)ptr - (size_t)tab3;
    if (o < ALLOC3 * sizeof(char3_t))
    {
        // 10 = 2*5; 0xCD is the inverse of 5 modulo 256.
        pool_give(&pool3, (unsigned char)((unsigned char)(o >> 1) * 0xCD));
        return;
    }
    o = (size_t)ptr - (size_t)tab6;
    if (o < ALLOC6 * sizeof(char6_t))
    {
        pool_give(&pool6, (unsigned char)(o >> 4));
        return;
    }

    block_t* q = (block_t*)ptr - 1;

    block_t* p = ((uintptr_t)ptr <= heap2_ptrend) ? &_alloc2_base : &_alloc_base;
    // dbg_printf("free ptr=%p heap_end=%p p=%p pptr=%p psize=%zu\n",ptr,heap_ptrend,p,p->ptr,p->size);

    for (; p->ptr && p->ptr < q; p = p->ptr) {}
    // p next pointer in the free-ed chaine list, p->ptr,
    // is 0 or is the first pointer >= q
    // (this means that p is before q)
    if ((uint8_t*)p->ptr == ((uint8_t*)q) + q->size)
    {
        // concatenate q and p next pointer
        q->size += p->ptr->size;
        q->ptr = p->ptr->ptr;
        // dbg_printf("free concatenate blocsize=%zu\n", q->size);
    }
    else
    {
        // insert in chained list: get q next cell from p next cell
        q->ptr = p->ptr;
        // dbg_printf("free add block blocsize=%zu\n", q->size);
    }
    // check if we can concatenate p and q
    if (((uint8_t*)p) + p->size == (uint8_t*)q)
    {
        // yes
        p->size += q->size;
        p->ptr = q->ptr;
    }
    else
    {
        // no, update next pointer for p
        p->ptr = q;
    }
}

void* realloc(void* ptr, const size_t size)
{
    // dbg_printf("[realloc] %p for %zu bytes\n", ptr, size);

    // The three largest sizes are malloc query commands, not allocations.
    if (size >= MAGIC_SIZE_QUERY_FREEMEM - 2)
        return NULL;

    if (ptr == NULL)
    {
        return malloc(size);
    }

    size_t old_size;
    if ((size_t)ptr - (size_t)tab2 < ALLOC2 * sizeof(char2_t))
        old_size = sizeof(char2_t);
    else if ((size_t)ptr - (size_t)tab3 < ALLOC3 * sizeof(char3_t))
        old_size = sizeof(char3_t);
    else if ((size_t)ptr - (size_t)tab6 < ALLOC6 * sizeof(char6_t))
        old_size = sizeof(char6_t);
    else
    {
        const block_t* h = (block_t*)((uint8_t*)ptr - sizeof(block_t));
        old_size = h->size - sizeof(block_t);
    }
    if (old_size >= size)
        return ptr;

    void* p = malloc(size);
    if (p)
    {
        memcpy(p, ptr, old_size);
        free(ptr);
    }

    return p;
}
