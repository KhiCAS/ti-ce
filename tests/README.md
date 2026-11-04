# Tests

With CEdev on `PATH`, run the build regression tests:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

`test_lto_tuning.py` exercises real C/C++ compilation, bitcode merging,
LTO code generation, global configuration invalidation, incremental builds and
compiler capability fallback. The shared fixture builds in temporary directories
and does not replace normal app outputs.

`test_inline_thresholds.py` checks per-source override precedence, persistence
through the bitcode merge, non-LTO builds and incremental
rebuilds when settings are changed or cleared.
