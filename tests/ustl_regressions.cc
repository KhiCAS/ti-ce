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
        const bool ok = id == 0 ? bit_widths() : id == 1 ? geometric_growth() : false;
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
