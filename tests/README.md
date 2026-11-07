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

Build and measure the original policy, then the balanced policy, using the same
ROM and application language:

```sh
make -j8 APPLANG=fr BENCHMARK=YES LTO_TUNING=NO
python3 tests/benchmark_cas.py --runner /tmp/khicas-cemu-bench/tests/autotester/cemu-headless --rom /path/to/app-enabled.rom --app bin/_KhiCAS.8ek --out tests/results/reference
make -j8 APPLANG=fr BENCHMARK=YES LTO_TUNING=YES
python3 tests/benchmark_cas.py --runner /tmp/khicas-cemu-bench/tests/autotester/cemu-headless --rom /path/to/app-enabled.rom --app bin/_KhiCAS.8ek --out tests/results/balanced --reference tests/results/reference/results.json
make -j8 APPLANG=fr BENCHMARK=NO
```

The benchmark app runs automatically on launch and returns to the OS. Its
volatile runtime selector retains the ordinary UI paths during LTO. Production
builds contain neither this selector nor the benchmark implementation. The
harness uses the CE SDK's real `clock()` (Giac disables its own internal clock
macro), with 32,768 ticks per second. Parsing, evaluation and printing are timed
separately; result logging is outside those intervals. Three measurements follow
one warmup, with a fixed random seed. Every repetition must return the same
result, all results must match the reference, and 32 independent exact-zero
identities must pass. Unevaluated inputs, resets and incomplete samples fail.

ROM preparation removes app chains from a disposable file copy. OS Reset Apps
preserves the active language pack, which otherwise changes the load address of
a pre-relocated app. The tested image still contains a 50,965-byte
French language app; the original ROM is never edited. The app placement was
checked against the installed Flash bytes before collecting measurements.

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
