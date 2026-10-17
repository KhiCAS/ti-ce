# KhiCAS CE

**An XCAS port for the TI-84 Plus CE / TI-83 Premium CE calculators.**

![KhiCAS Screenshots](screenshot.jpg)

## Recent changes

- Native 2D expression and matrix editing, with fractions, powers, roots,
integrals, sums, limits and series templates. The editor uses the FlowCE
layout and structural editing code adapted by Bernard Parisse.
- F5 evaluates the selected subexpression, or the whole expression in caret
mode. Series templates default to polynomial output without a remainder term.
- Improved interruption and low-memory handling, plus fixes for catalog
completion, console menus and cursor placement, reciprocal printing, and
graph redraw after cancelling a menu.

## Using the 2D editor

Open the editor from the console with the **fraction template key** on a
TI-83 Premium CE, or **2nd, X,T,theta,n** on a TI-84 Plus CE. The current
expression starts selected. On the TI-83 Premium CE, **2nd, fraction** starts
with the caret at the end instead.

| Key | Action in the editor |
| --- | --- |
| MODE | Switch between caret and subexpression selection modes |
| Arrows | Move the caret; in selection mode, left/right select siblings and up/down select the parent/child |
| F5 (rightmost key under the screen) | Evaluate the selection, or the whole expression in caret mode |
| ENTER | Accept the edited expression into the console |
| CLEAR / annul | Undo the last edit; cancel when there is nothing left to undo |
| 2nd, MODE (QUIT) | Cancel and discard edits immediately |
| 2nd, CLEAR | Empty the editor |

F1/F2 menus and MATH provide command templates. For example,
`series(sin(x),x,0,5,polynom)` displays a series template; empty point and
order fields default to `0` and `5` when accepted. In a matrix cell, comma
adds a column and 2nd, ENTER adds a row. The text line below the 2D display
shows the underlying Giac expression.

An 8 KiB emergency reserve and a preallocated error value help interrupted
calculations unwind under memory pressure. Recovery is best effort; some
allocation failures still terminate the application.

## Compilation:

### Prerequisites

- Get the [nightly CE toolchain](https://github.com/CE-Programming/toolchain/releases/tag/nightly)
used by CI, which includes the latest compiler fixes.
(See [nightly docs](https://ce-programming.github.io/toolchain/nightly/).)

- `python3` is also required for the post-build steps.

Clone with `git clone --recurse-submodules https://github.com/KhiCAS/ti-ce.git`.
In an existing checkout, run `git submodule update --init --recursive` after
pulling so that the Giac sources match the referenced revision.

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
