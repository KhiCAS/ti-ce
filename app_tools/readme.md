# KhiCAS app packaging

The root makefile includes the installed toolchain makefile followed by
`makefile_elf`. Run `make app` (the default target) to build KhiCAS, the split
`AppIns*` AppVars and `INST.8xp` in `bin/`. The pipeline is:

```
KhiCAS.obj (ELF with --emit-relocs)
  -> convbin -j elf -k 8ek -> KhiCAS.8ek
  -> relocate_app.py      -> _KhiCAS.bin
  -> make_8ek.py          -> _KhiCAS.8ek
  -> make_segments.py     -> AppIns00.8xv, AppIns01.8xv, ...
```

`INST.8xp` is copied to `bin/` from the existing installer. The `mkapp*` scripts
publish the pre-relocated app, segments and installer, and create the bundle.
The `.8ek` is unsigned; use `INST` and the split AppVars to install it as
described in the repository's installation instructions.

The application name comes from `NAME` in the root makefile.
`APPLICATION_DESCRIPTION` controls the optional description embedded by
convbin. The transfer-file `COMMENT` defaults to the build time and toolchain
version from `cedev-config --comment`. Distribution builds are published
under the `nightly` release tag.

For `APPLANG=fr`, the relocator preserves the legacy French-app metadata bit
(byte 0x0E of the eZ8 metadata, bit 3, or raw app byte 0x10E). English leaves
the bit clear; French and French L2 set it (`0x00` -> `0x08`). Language and L2
code differences come from `FRANCAIS` and `WITH_QUAD`/`WITH_PERIODIC` compiler
defines in the root makefile.

`obj/.khicas-build-config` tracks `APPLANG` and `VARIANT` for the compiler,
linker and relocator. Switching variants in the same build directory rebuilds
the code and final installer files without requiring `make clean`. A changed
configuration forces the complete build, including LTO and packaging, so this
also works with macOS Make 3.81's one-second timestamp resolution. Repeating
the same configuration preserves the stamp and output timestamps.

convbin resolves the ELF symbol/addend and VMA/LMA details and emits six-byte
app relocation records for Flash references. References to initialized data and
BSS already contain their linked RAM addresses, so the postprocessor keeps
those bytes. There is no hardcoded RAM base in the new relocator.

For an app with no remaining relocation records, its payload address is:

```
payload_start = 0x3B0000 - 3 - final_raw_app_size + 0x100 + 0x2A
```

The three bytes are the length trailer appended by `make_segments.py`; the raw
app size includes the 256-byte app header, 42-byte metadata, payload (including
any description), and 260-byte signature field. This preserves the legacy
relocator's Flash base calculation. It assumes installation in the first app
slot using the existing `INST` layout. All apps must be deleted first.

The relocator validates the unsigned convbin headers, length fields, metadata
offsets and relocation bounds before writing its output. It preserves absent
data offsets (zero) and the default description offset (3). The splitter reports
conversion failures and removes surplus segments when a smaller app replaces a
larger one.

To check against a second GNU ld link at the calculated Flash address, run:

```
python3 -m unittest discover -s app_tools -p 'test_*.py' -v
```

These tests require the binutils-based toolchain on PATH (or set
`CEDEV_TOOLCHAIN` to its installation). They cover Flash pointers, RAM and BSS
pointers, initialized-data LMA references, addends, relative branches, grouped
byte relocations, descriptions, malformed input, appvar checksums and complete
segment reassembly including the trailer. A small compiled fixture uses the
real makefiles to check English -> French -> French L2 -> English switches
without cleaning, with and without LTO, including compiled feature selection,
metadata, final app bytes, installer segments and an unchanged incremental
build. The same-input metadata test checks that setting the French bit preserves
all other app bytes and the relocation base. Physical-calculator behavior
requires an installation test.
