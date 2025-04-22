# ----------------------------
# Makefile Options
# ----------------------------

NAME = DEMO
ICON = icon.png
COMPRESSED = NO
ARCHIVED = NO

CFLAGS = -Wall -Wextra -Oz -DMP_16BIT -DTICE -Isrc/tommath
CXXFLAGS = -Wall -Wextra -Wno-sign-compare -Oz -DMP_16BIT -DHAVE_CONFIG_H -DTICE -DXLIGHT -DCOMPILE_FOR_STABILITY -Iustl -Isrc/tommath -Wno-unused-parameter -Wno-deprecated-register -Wno-reorder-ctor -Wno-unused-variable -Wno-inline-new-delete -DWITH_LAPLACE -DWITH_PERIODIC -DWITH_UNITS -DWITH_DESOLVE  -DWITH_EQW -DWITH_PLOT -DWITH_SHEET -DWITH_TABVAR #
CPP_EXTENSION = cc
APP_NAME = KhiCAS
APP_VERSION = 5.0.0.0000
DESCRIPTION = KhiCAS

EXTRA_LDFLAGS = -e 9999
ALLOCATOR = CUSTOM
SKIP_LIBRARY_LDFLAGS = YES
PREFER_OS_CRT = YES
PREFER_OS_LIBC = NO
LIBLOAD_OPTIONAL = fileioc

# STACK_HIGH ?= D1F87E # attempt to have more stack does not work

# ----------------------------

#include $(shell cedev-config --makefile)
include app_tools/makefile
