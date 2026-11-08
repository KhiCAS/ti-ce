# KhiCAS's CE uSTL fork

This is a selectively patched uSTL snapshot, not a full import of a newer
release. The `v1.6` label in `config.h` is historical: several headers already
match early 2.x sources. Keep the CE allocator, exception settings and existing
APIs when importing fixes.

On eZ80, `unsigned int` is 24 bits, `unsigned long` is 32 bits and
`unsigned long long` is 48 bits. The bit helpers must therefore use the actual
builtin operand width; a 64-bit value needs both 32-bit halves. Nonexact buffer
reserves keep the initial allocation exact and round later growth to a power of
two when it fits `size_t`. `reserve(n, true)` retains exact allocation requests.

## Selected upstream correctness fixes

The following changes are adapted from the surviving
[SlyVTT/ustl mirror](https://github.com/SlyVTT/ustl), with CE-specific follow-ups:

- [`63d01c1`](https://github.com/SlyVTT/ustl/commit/63d01c1f55357aa16b3d17b81db029b105cd3c88):
  copy a linked buffer in full before erasing from it, and implement `copy_link`
  through `reserve`. Raw buffers copy their logical size; strings also copy their
  terminator through `minimumFreeCapacity()`.
- [`780534f`](https://github.com/SlyVTT/ustl/commit/780534fa2508759ec63e4d8e255a807187f0aa39):
  clamp string erase lengths, including `npos`, to the remaining contents.
- [`f66275d`](https://github.com/SlyVTT/ustl/commit/f66275dc72cb71f741a35a51f9e9a51c0eb73842):
  update formatted string size after formatting. Restore bounded `vsnprintf`
  writes in place of the local `vsprintf` override, handle negative results,
  and pair `va_copy`/`va_end` on every string and stream formatting attempt.
- [`e23e783`](https://github.com/SlyVTT/ustl/commit/e23e783bcafc6b64b06f06dd8e8fa53f75d30113):
  skip element destructors for borrowed vectors on clear, deallocate and
  destruction; separate resize growth from shrinkage and simplify append-hole
  arithmetic. Also avoid destroying borrowed elements on shrink. Retain the CE
  `resize(n, bool bExact)` API and its exact-growth default.
- [`768b0ba`](https://github.com/SlyVTT/ustl/commit/768b0badce54224f44245f6a298f9287911c424f):
  return typed pointers from vector data accessors.
- [`c346071`](https://github.com/SlyVTT/ustl/commit/c34607174bf7854657fc575370c76e91900745c3):
  build every heap subtree and repair only the root subtree on pop. Use parent
  indices to avoid forming an iterator before the beginning, and handle empty
  ranges before advancing or decrementing an iterator.

These changes retain uSTL's existing bitwise-relocation model for vector
elements. Run the [CE regression suite](../tests/README.md#ustl-regressions)
against the actual KhiCAS allocator.
