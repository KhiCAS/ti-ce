# Tests and benchmarks

The Python tests exercise the real CEdev compile/merge/codegen pipeline,
configuration invalidation, per-source inlining and compiler capability fallback,
safe preparation of disposable ROM copies, and SDK/uSTL math-header compatibility.
With CEdev on `PATH`, run:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The math-header tests use that SDK's compiler and headers. They check C math,
C++14/17/23 include orders, preservation of the `std` macro, scalar/complex
overloads, and separation of SDK and uSTL numeric limits. Run the same command
with a different SDK's `bin` directory first on `PATH` to check another version.

Build/packaging helpers remain in `app_tools/`. The optional calculator-side
benchmark implementation lives in `tests/benchmark.cc`; `BENCHMARK=YES` adds it
to the application sources. It is not compiled in production builds.

## CAS benchmarks

Build a console-enabled headless CEmu runner in an isolated directory. This does
not modify the CEmu checkout. Use a CEmu version with the native
`DEBUG_SUPPORT=1` Makefile option and initialize its `core/debug/zdis` submodule
first. The helper copies the sources and invokes that option without patching
them:

```sh
python3 tests/build_benchmark_runner.py --source ../CEmu --out /tmp/khicas-cemu-bench
```

Build and measure each policy using the same ROM, language and frozen case set:

```sh
make -j8 APPLANG=fr BENCHMARK=YES LTO_TUNING=NO
python3 tests/benchmark_representative.py --runner /tmp/khicas-cemu-bench/tests/autotester/cemu-headless --rom /path/to/app-enabled.rom --app-bin bin/_KhiCAS.bin --out tests/results/reference
make -j8 APPLANG=fr BENCHMARK=YES LTO_TUNING=YES
python3 tests/benchmark_representative.py --runner /tmp/khicas-cemu-bench/tests/autotester/cemu-headless --rom /path/to/app-enabled.rom --app-bin bin/_KhiCAS.bin --out tests/results/balanced
make -j8 APPLANG=fr BENCHMARK=NO
```

The [representative suite](REPRESENTATIVE_BENCHMARKS.md) contains 34 fixed inputs,
including polynomial GCD/factorization, harder integrals, limits, Taylor series,
dense matrices, calculation-only plots and variation tables. The runner boots a
fresh disposable ROM for every case, records a warmup and three measurements,
checks the result and retains failures. Parsing, evaluation and result formatting
are timed separately on the CE SDK's real 32,768-Hz clock; diagnostic console and
graph UI work are excluded. `--ids 0,26,29` selects a smaller set.

The tested Giac revision leaves `factor(x^4+4)` and `sum(1/n^2,n,1,inf)`
unevaluated. The full command therefore returns nonzero while preserving all
34 results; these failures must remain visible and outside successful-workload
aggregates. Compare matching case IDs, expressions and valid result values
between configurations before comparing their timings. The older
`benchmark_cas.py` parser supports the historical 12-input harness and remains
available for its artifacts; it does not parse the current harness protocol.

The benchmark-only volatile mode flag retains the ordinary UI paths during LTO.
Production builds contain neither the selector nor the benchmark implementation.
The runner removes existing app chains, including language packs, from its
ROM copy to preserve first-app placement; the original ROM is not modified.

`test_cas_usage.py` also checks entry through the current 2D editor, graph-menu
cancellation for three Clear-hold durations, held-Clear consumption and arrow
repeat. It prepares the same empty-app ROM copy:

```sh
python3 tests/test_cas_usage.py --runner /tmp/khicas-cemu-bench/tests/autotester/cemu-headless --rom /path/to/app-enabled.rom --app bin/_KhiCAS.8ek --out tests/results/ui
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

Generated JSON, logs, screenshots, emulator states and disposable ROM copies are
local artifacts under ignored `tests/results/` (or a directory outside the
checkout). They are not versioned fixtures. The README summary records the
measured size/speed tradeoff; rerun these commands for a different compiler or
flag set. Compare clean production app sizes separately from benchmark app sizes,
which also include the harness.

## uSTL regressions

Run the CE-side bit-width, buffer-growth and upstream correctness checks with the actual KhiCAS
allocator. Clean when switching between the regular CAS harness and this entry
point, since the selected extra source list is not a build-configuration key:

```sh
make clean
make -j8 APPLANG=fr BENCHMARK=YES EXTRA_CXX_SOURCES=tests/ustl_regressions.cc
python3 tests/benchmark_representative.py --runner /path/to/cemu-headless --rom /path/to/app-enabled.rom --app-bin bin/_KhiCAS.bin --cases tests/ustl_cases.json --out tests/results/ustl
make clean
make -j8 APPLANG=fr BENCHMARK=NO
```

The checks cover runtime inputs at 24/32/64-bit boundaries, the documented
`NextPow2` overflow result, exact initial allocations followed by geometric growth,
contents across reallocations,
explicit exact-capacity requests, vector lifetimes and borrowed storage, linked
buffer erasure, `erase(..., npos)`, bounded variadic formatting, and heap
algorithms with shuffled duplicates and both comparators. These optional tests are not production
sources and do not change the frozen 34-case CAS input list.
