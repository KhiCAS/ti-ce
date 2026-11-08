// Optional CE-side tests using KhiCAS's allocator and actual uSTL headers.
#ifdef KHICAS_BENCHMARK
#include <giac/giacPCH.h>
#include <stdint.h>
#include <stdio.h>
#undef clock
#include <time.h>
#undef std

volatile unsigned char khicas_benchmark_mode = 1;
extern "C" const char khicas_suite_selector[] __attribute__((used)) =
    "KHICAS_SUITE_SELECT=000";

static uint32_t reference_next_pow2(uint32_t value) {
    if (value > (uint32_t(1) << 31)) return 1;
    uint32_t result = 1;
    while (result < value) result <<= 1;
    return result;
}

static bool bit_widths() {
    if (ustl::FirstBit(uint32_t(0), 17) != 17 ||
        ustl::FirstBit(uint64_t(0), 23) != 23) return false;
    for (unsigned bit = 0; bit < 32; ++bit) {
        volatile uint32_t value = uint32_t(1) << bit;
        if (ustl::FirstBit(value, 0) != bit) return false;
    }
    for (unsigned bit = 0; bit < 64; ++bit) {
        volatile uint64_t value = uint64_t(1) << bit;
        if (ustl::FirstBit(value, 0) != bit) return false;
    }
    for (uint32_t value = 0; value <= 4096; ++value) {
        volatile uint32_t input = value;
        if (ustl::NextPow2(input) != reference_next_pow2(value)) return false;
    }
    const uint32_t edges[] = {0x7FFFFFUL, 0x800000UL, 0x800001UL,
        0xFFFFFFUL, 0x1000000UL, 0x7FFFFFFFUL, 0x80000000UL,
        0x80000001UL, 0xFFFFFFFFUL};
    for (unsigned i = 0; i < sizeof(edges)/sizeof(edges[0]); ++i) {
        volatile uint32_t input = edges[i];
        if (ustl::NextPow2(input) != reference_next_pow2(edges[i])) return false;
    }
    return true;
}

static bool geometric_growth() {
    ustl::memblock block;
    block.reserve(17);
    if (block.capacity() != 17) return false;
    block.reserve(18);
    if (block.capacity() != 32) return false;
    block.reserve(33, true);
    if (block.capacity() != 33) return false;
    ustl::vector<unsigned char> bytes;
    unsigned relocations = 0;
    for (unsigned i = 0; i < 4096; ++i) {
        const void *before = bytes.begin();
        bytes.push_back(static_cast<unsigned char>(i % 251));
        relocations += before != bytes.begin();
        if (bytes.size() != i+1 || bytes.capacity() < bytes.size()) return false;
    }
    for (unsigned i = 0; i < bytes.size(); ++i)
        if (bytes[i] != i % 251) return false;
    ustl::vector<int> exact;
    exact.reserve(129, true);
    return relocations <= 14 && exact.capacity() == 129;
}

struct tracked {
    static unsigned constructed, destroyed;
    unsigned marker;
    tracked() : marker(123) { ++constructed; }
    ~tracked() { marker = 0; ++destroyed; }
};
unsigned tracked::constructed = 0;
unsigned tracked::destroyed = 0;

static bool vector_lifetimes() {
    tracked::constructed = tracked::destroyed = 0;
    {
        ustl::vector<tracked> values;
        values.resize(4);
        if (tracked::constructed != 4 || tracked::destroyed) return false;
        values.resize(2);
        if (tracked::constructed != 4 || tracked::destroyed != 2) return false;
        values.resize(5);
        if (tracked::constructed != 7 || tracked::destroyed != 2) return false;
        values.clear();
        if (tracked::destroyed != 7) return false;
        values.resize(3);
        values.deallocate();
        if (tracked::constructed != 10 || tracked::destroyed != 10) return false;
    }
    if (tracked::destroyed != 10) return false;
    {
        tracked foreign[4];
        const unsigned before = tracked::destroyed;
        { ustl::vector<tracked> borrowed; borrowed.link(foreign, 4); borrowed.clear(); }
        { ustl::vector<tracked> borrowed; borrowed.link(foreign, 4); borrowed.deallocate(); }
        { ustl::vector<tracked> borrowed; borrowed.link(foreign, 4); }
        { ustl::vector<tracked> borrowed; borrowed.link(foreign, 4); borrowed.resize(2); }
        if (tracked::destroyed != before) return false;
        for (unsigned i = 0; i < 4; ++i)
            if (foreign[i].marker != 123) return false;
    }
    ustl::vector<unsigned char> bytes(3);
    bytes.data()[1] = 42;
    const ustl::vector<unsigned char>& view = bytes;
    return view.data() == view.cdata() && view.data()[1] == 42;
}

static bool linked_buffers() {
    const char source[] = "0123456789abcdef";
    ustl::memblock block;
    block.link(source, 16);
    block.copy_link();
    if (block.is_linked() || block.size() != 16 || memcmp(block.begin(), source, 16)) return false;
    block.deallocate();
    block.link(source, 16);
    block.erase(block.begin()+4, 8);
    if (block.size() != 8 || memcmp(block.begin(), "0123cdef", 8)) return false;
    ustl::string text;
    text.link(source, 16);
    text.erase(ustl::string::size_type(4), ustl::string::size_type(8));
    if (text != "0123cdef" || strcmp(source, "0123456789abcdef")) return false;
    return true;
}

static bool erase_to_end() {
    ustl::string text("abcdef");
    text.erase(ustl::string::size_type(2));
    if (text != "ab" || text.size() != 2) return false;
    text.erase(ustl::string::size_type(2), ustl::string::npos);
    if (text != "ab") return false;
    text.erase(ustl::string::size_type(0), ustl::string::size_type(100));
    return text.empty() && !*text.c_str();
}

static bool string_formatting() {
    ustl::string text;
    // This needs a sizing pass, then a second pass with the same argument list.
    if (text.format("%s:%d:%s", "abcdefgh", 123, "tail") != 17 ||
        text != "abcdefgh:123:tail") return false;
    if (text.format("%s-%d", "x", 7) != 3 || text != "x-7" || text.size() != 3) return false;
    if (text.format("%s", "") != 0 || !text.empty()) return false;
    const char source[] = "borrowed";
    text.deallocate();
    text.link(source, 8);
    if (text.format("%s/%d", "replacement", 42) != 14 ||
        text != "replacement/42" || strcmp(source, "borrowed")) return false;
    ustl::ostringstream stream;
    if (stream.format("%s-%d-%s", "abcdefgh", 123, "tail") != 17) return false;
    return stream.str() == "abcdefgh-123-tail";
}

static bool heap_algorithms() {
    for (unsigned count = 0; count <= 31; ++count) {
        int values[32];
        int expected[32];
        for (unsigned i = 0; i < count; ++i)
            values[i] = expected[i] = int((i*17+count*3)%13)-6;
        // A separate insertion-sort oracle checks multiplicity as well as order.
        for (unsigned i = 1; i < count; ++i) {
            const int value = expected[i];
            unsigned j = i;
            while (j && expected[j-1] > value) { expected[j] = expected[j-1]; --j; }
            expected[j] = value;
        }
        ustl::make_heap(values, values+count);
        if (!ustl::is_heap(values, values+count)) return false;
        for (unsigned n = count; n; --n) {
            if (values[0] != expected[n-1]) return false;
            ustl::pop_heap(values, values+n);
            if (!ustl::is_heap(values, values+n-1)) return false;
        }
        ustl::make_heap(values, values+count, ustl::greater<int>());
        for (unsigned n = count, i = 0; n; --n, ++i) {
            if (values[0] != expected[i]) return false;
            ustl::pop_heap(values, values+n, ustl::greater<int>());
        }
        ustl::pop_heap(values, values);
        for (unsigned i = 0; i < count; ++i) {
            values[i] = expected[i];
            ustl::push_heap(values, values+i+1);
            if (!ustl::is_heap(values, values+i+1)) return false;
        }
        ustl::sort_heap(values, values+count);
        for (unsigned i = 0; i < count; ++i)
            if (values[i] != expected[i]) return false;
    }
    return true;
}

int khicas_run_benchmarks(giac::context *) {
    const volatile char *selector = khicas_suite_selector;
    const unsigned offset = sizeof("KHICAS_SUITE_SELECT=")-1;
    const unsigned id = 100*(selector[offset]-'0') +
        10*(selector[offset+1]-'0') + selector[offset+2]-'0';
    sprintf((char *)0xFB0000, "KHICAS_V2_BEGIN|%u|%lu\n", id,
            (unsigned long)CLOCKS_PER_SEC);
    for (unsigned repeat = 0; repeat < 4; ++repeat) {
        sprintf((char *)0xFB0000, "KHICAS_V2_START|%u|%u\n", id, repeat);
        const clock_t start = clock();
        const bool ok = id == 0 ? bit_widths() : id == 1 ? geometric_growth() :
            id == 2 ? vector_lifetimes() : id == 3 ? linked_buffers() :
            id == 4 ? erase_to_end() : id == 5 ? string_formatting() :
            id == 6 ? heap_algorithms() : false;
        const clock_t elapsed = clock()-start;
        sprintf((char *)0xFB0000, "KHICAS_V2_SAMPLE|%u|%u|0|%lu|0|%s\n",
                id, repeat, (unsigned long)elapsed, ok ? "ok" : "failed");
        if (repeat == 3)
            sprintf((char *)0xFB0000, "KHICAS_V2_CHECK|%u|%u\n", id, unsigned(ok));
    }
    sprintf((char *)0xFB0000, "KHICAS_V2_DONE\n");
    return 0;
}
#endif
