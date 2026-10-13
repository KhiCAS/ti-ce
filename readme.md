# KhiCAS CE

**An XCAS port for the TI-84 Plus CE / TI-83 Premium CE calculators.**

![KhiCAS Screenshots](screenshot.jpg)

## Compilation:

### Prerequisites

- Get the [nightly CE toolchain](https://github.com/CE-Programming/toolchain/releases/tag/nightly)
used by CI, which includes the latest compiler fixes.
(See [nightly docs](https://ce-programming.github.io/toolchain/nightly/).)

- `python3` is also required for the post-build steps.

### KhiCAS build

Run the `./mkappen` (English build) or `./mkappfr` (French build) script.

It will create, in the bin/ folder, many appvars (.8xv files) and an installer (.8xp).  
A bundle file (.b83/.b84), only usable on TI Connect CE for Windows, will also be created with those files.

`make -j8` builds the English app and installer segments without publishing to
`shared/ti`. The linked ELF is `bin/KhiCAS.obj`; convbin converts it to
`bin/KhiCAS.8ek`, then `app_tools/relocate_app.py` produces `bin/_KhiCAS.bin`
with its Flash relocations applied and removed. `bin/_KhiCAS.8ek` and the
`AppIns*.8xv` files contain this smaller app. The distribution scripts copy the
pre-relocated 8ek to `shared/ti/KhiCAS.8ek`.

The LTO step enables function merging and uses an inline threshold of `-10`
to reduce app size. Set `LTOFLAGS` on the make command line to override these
options; for example, `LTOFLAGS=-Oz` uses the compiler's default size tuning.
CI prints the compiler version, pre-relocated app size and installer AppVar
count. CI follows the nightly toolchain release; CEdev v15.0 ships Clang 19.

This pre-relocation assumes KhiCAS is the first installed app: delete all apps
before installing it with `INST`, as described in the installation instructions.
See [app_tools/readme.md](app_tools/readme.md) for the address calculation and tests.

## Installation on calculator:
See [install_en.txt](install_en.txt) or [install_fr.txt](install_fr.txt).

## Build customization:
Some compile flags control the inclusion of parts of the code.

The total app size ranges from 2.2MB to 2.8MB, approximately.
* math:
  * `-DWITH_LAPLACE`  Laplace/inverse Laplace transform ≈ 20K
  * `-DWITH_DESOLVE`  desolve command ≈ 60K (including Laplace)
* science:
  * `-DWITH_PERIODIC` periodic table ≈ 10K
  * `-DWITH_UNITS` physical units ≈ 40K

Note that 2 apps do never share a flash page, as a consequence you can
add space up to a multiple of 65.536, e.g. 2260 KB and 2280 KB will
leave free the same amount of flash.
