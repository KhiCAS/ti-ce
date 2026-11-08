# Representative CE CAS benchmarks

34 fixed inputs selected after B. Parisse's benchmark feedback. The case manifest
records categories, independent checks and FlowCE provenance at commit
68cddf63227fac3e17b571a43ff8aa2b22cc1839. No library/algorithm tuning uses the
held-out input. Existing production behavior is unchanged (BENCHMARK defaults NO).

Build the French normal app with the matching CE SDK on PATH:

```sh
gmake -j4 APPLANG=fr VARIANT=normal BENCHMARK=YES
python3 tests/benchmark_representative.py \
  --runner /path/to/cemu-headless --rom /path/to/signature-disabled.rom \
  --app-bin bin/_KhiCAS.bin --out /tmp/khicas-representative
```

The runner boots a fresh disposable ROM for each case, patches the benchmark
selector only, collects one cold/warmup and three measured records, and retains
all failures. The original ROM is never modified, and temporary ROMs are deleted.
Use --ids 0,26,29 to select cases, --jobs 2 for concurrent emulator processes,
and --limit-seconds 120 for the no-progress deadline in emulated seconds.

Exit status is nonzero if a selected case fails validation or does not complete.
The tested Giac revision leaves factor(x^4+4) and sum(1/n^2,n,1,inf) unevaluated;
these remain visible failures and are excluded from valid-workload aggregates.
Do not interpret their elapsed times as successful factorization/summation.

CAS-only timing measures parse, evaluation and result formatting separately.
No console/UI work or answer autosimplification is included. Diagnostics are
disabled before Console_Init; plotting uses :; to discard the result, then an
untimed check verifies sampled curve creation without formatting the curve.
The plot UI is not called. This is distinct from FlowCE batch.py's full
Enter-to-keypad-ready timing, which includes answer.cc transformations and UI.

Exact algebra/matrices use symbolic checks; Taylor remainders are removed before
comparison with fixed coefficients. Antiderivatives are differentiated and checked
at five points with a 0.3% scaled tolerance for CE single-precision arithmetic.
Eigenvectors have residual checks; eigenvalues have trace and square-trace checks.
Tabvar/plot checks cover structure and sampling rather than a complete semantic
proof. Compare actual outputs and keep failed cases visible.

Report production app bytes separately from benchmark-instrumented binaries.
Flash allocation is ceil((raw bytes+3-byte install trailer)/65536) sectors,
not the number of installer AppVars. Report absolute per-case seconds and the
fixed uSTL >=1-second subset as well as full aggregates.

The runner writes `results.json` and per-case samples, validation status,
protocol transcripts and emulator logs under `--out`. Retain those with the
compiler/source revisions and hashes of the measured binaries. These are CEmu
measurements, not physical-device or release/CI validation.
