// config.h - Generated from config.h.in by configure.
#pragma once

// Define to the one symbol short name of this package.
#define USTL_NAME	"ustl"
// Define to the full name and version of this package.
#define USTL_STRING	"ustl v1.6"
// Define to the version of this package.
#define USTL_VERSION	0x160
// Define to the address where bug reports for this package should be sent.
#define USTL_BUGREPORT	"Mike Sharov <msharov@users.sourceforge.net>"

/// Define to 1 if you want stream operations to throw exceptions on
/// insufficient data or insufficient space. All these errors should
/// be preventable in output code; the input code should verify the
/// data in a separate step. It slows down stream operations a lot,
/// but it is your decision. By default only debug builds throw.
///
#define WANT_STREAM_BOUNDS_CHECKING 0

#if !WANT_STREAM_BOUNDS_CHECKING && !defined(NDEBUG)
    #define WANT_STREAM_BOUNDS_CHECKING 0
#endif

/// Define to 1 if you want backtrace symbols demangled.
/// This adds some 15k to the library size, and requires that you link it and
/// any executables you make with the -rdynamic flag (increasing library size
/// even more). By default only the debug build does this.
#undef WANT_NAME_DEMANGLING

#if !WANT_NAME_DEMANGLING && !defined(NDEBUG)
    #define WANT_NAME_DEMANGLING 1
#endif

/// Define to 1 if you want to build without libstdc++
#define WITHOUT_LIBSTDCPP 1

/// Define GNU extensions if unavailable.
#ifndef __GNUC__
    /// GCC (and some other compilers) define '__attribute__'; ustl is using this
    /// macro to alert the compiler to flag inconsistencies in printf/scanf-like
    /// function calls.  Just in case '__attribute__' is undefined, make a dummy.
    /// 
    #ifndef __attribute__
	#define __attribute__(p)
    #endif
#endif
#if __GNUC__ >= 4
    #define DLL_EXPORT		__attribute__((visibility("default")))
    #define DLL_LOCAL		__attribute__((visibility("hidden")))
    #define INLINE		__attribute__((always_inline))
#else
    #define DLL_EXPORT
    #define DLL_LOCAL
    #define INLINE
#endif
#if __cplusplus >= 201103L && (!__GNUC__ || (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 2)) || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #define HAVE_CPP11 1
#endif
#if !HAVE_CPP11
    #define noexcept		throw()
    #define constexpr
#endif
#if __GNUC__ >= 3 && (__i386__ || __x86_64__)
    /// GCC 3+ supports the prefetch directive, which some CPUs use to improve caching
    #define prefetch(p,rw,loc)	__builtin_prefetch(p,rw,loc)
#else
    #define prefetch(p,rw,loc)
#endif
#if __GNUC__ < 3
    /// __alignof__ returns the recommended alignment for the type
    #define __alignof__(v)	min(sizeof(v), sizeof(void*))
    /// This macro returns 1 if the value of x is known at compile time.
    #ifndef __builtin_constant_p
	#define __builtin_constant_p(x)	0
    #endif
#endif

// Define to empty if 'const' does not conform to ANSI C.
/* #define const */
// Define as '__inline' if that is what the C compiler calls it
/* #define inline __inline */
// Define to 'long' if <sys/types.h> does not define.
/* typedef long off_t; */
// Define to 'unsigned' if <sys/types.h> does not define.
/* typedef long size_t; */

/// gcc has lately decided that inline is just a suggestion
/// Define to 1 if when you say 'inline' you mean it!
#undef WANT_ALWAYS_INLINE
#if WANT_ALWAYS_INLINE
    #define inline INLINE inline
#endif

/// Define to 1 if you have the <assert.h> header file.
#define HAVE_ASSERT_H 1

/// Define to 1 if you have the <ctype.h> header file.
#define HAVE_CTYPE_H 1

/// Define to 1 if you have the <errno.h> header file.
#define HAVE_ERRNO_H 1

/// Define to 1 if you have the <fcntl.h> header file.
#define HAVE_FCNTL_H 1

/// Define to 1 if you have the <float.h> header file.
#define HAVE_FLOAT_H 1

/// Define to 1 if you have the <inttypes.h> header file.
#define HAVE_INTTYPES_H 1

/// Define to 1 if you have the <limits.h> header file.
#define HAVE_LIMITS_H 1

/// Define to 1 if you have the <locale.h> header file.
#define HAVE_LOCALE_H 1

// Define to 1 if you have the <alloca.h> header file.
#define HAVE_ALLOCA_H 1

// Define to 1 if you have the <signal.h> header file.
#define HAVE_SIGNAL_H 1

// Define to 1 if you have the __va_copy function
#define HAVE_VA_COPY 0

// Define to 1 if you have the <stdarg.h> header file.
#define HAVE_STDARG_H 1

// Define to 1 if you have the <stddef.h> header file.
#define HAVE_STDDEF_H 1

// Define to 1 if you have the <stdint.h> header file.
#define HAVE_STDINT_H 1

// Define to 1 if you have the <stdio.h> header file.
#define HAVE_STDIO_H 1

// Define to 1 if you have the <stdlib.h> header file.
#define HAVE_STDLIB_H 1

// Define to 1 if you have the <string.h> header file.
#define HAVE_STRING_H 1

// Define to 1 if you have the 'strrchr' function.
#define HAVE_STRRCHR 1

// Define to 1 if you have the 'strsignal' function.
#define HAVE_STRSIGNAL 1

// Define to 1 if you have the <sys/stat.h> header file.
#undef HAVE_SYS_STAT_H

// Define to 1 if you have the <sys/types.h> header file.
#undef HAVE_SYS_TYPES_H

// Define to 1 if you have the <sys/mman.h> header file.
#undef HAVE_SYS_MMAN_H

// Define to 1 if you have the <time.h> header file.
#define HAVE_TIME_H 1

// Define to 1 if you have the <unistd.h> header file.
#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H 1
#endif

// Define to 1 if you have the <math.h> header file.
#define HAVE_MATH_H 1

// Define to 1 if you have the <execinfo.h> header file.
#define HAVE_EXECINFO_H 1

// Define to 1 if you have the <cxxabi.h> header file.
#if __GNUC__ >= 3
    #define HAVE_CXXABI_H 1
#endif

// Define to 1 if you have the rintf function. Will use rint otherwise.
#define HAVE_RINTF 1

// STDC_HEADERS is defined to 1 on sane systems.
#if HAVE_ASSERT_H && HAVE_CTYPE_H  && HAVE_ERRNO_H && HAVE_FLOAT_H &&\
    HAVE_LIMITS_H && HAVE_LOCALE_H && HAVE_MATH_H  && HAVE_SIGNAL_H &&\
    HAVE_STDARG_H && HAVE_STDDEF_H && HAVE_STDIO_H && HAVE_STDLIB_H &&\
    HAVE_STRING_H && HAVE_TIME_H
    #define STDC_HEADERS 1
#endif

// STDC_HEADERS is defined to 1 on unix systems.
#if 1 //HAVE_FCNTL_H && HAVE_SYS_STAT_H && HAVE_UNISTD_H
//#define MEMMGR
#define WITHOUT_EXCEPTIONS
#define USTL_THROW(x) 
typedef unsigned off_t;
typedef unsigned size_t;
typedef size_t ssize_t;
typedef unsigned mode_t ;
//#define rint(x) int(x+.5)
//#define calloc(nmemb, size) malloc(size*nmemb)
//#define strtoll strtol
//#define strerror(n) "error"
#ifndef EINTR
#define EINTR            4      /* Interrupted system call */
#endif /* EINTR */
#define	F_GETFL		3		/* get file status flags */
#define	F_SETFL		4		/* set file status flags */
#define O_ACCMODE           0003
#define O_RDONLY 00
#define O_WRONLY 2 // 01
#define O_RDWR 3 // 02
#define O_CREAT           0100        /* Not fcntl.  */
#define O_EXCL                   0200        /* Not fcntl.  */
#define O_NOCTTY           0400        /* Not fcntl.  */
#define O_TRUNC          01000        /* Not fcntl.  */
#define O_APPEND          02000
#define O_NONBLOCK          04000
#define O_NDELAY        O_NONBLOCK
#define O_SYNC               04010000
#define O_ASYNC         020000
#define __O_LARGEFILE        0100000
#define STDUNIX_HEADERS 1
#endif

// Define to 1 if your compiler treats char as a separate type along with
// signed char and unsigned char. This will create overloads for char.
#define HAVE_THREE_CHAR_TYPES 1

// Define to 1 if you have 64 bit types available
#define HAVE_INT64_T 1

// Define to 1 if you have the long long type
#define HAVE_LONG_LONG 1

// Define to 1 if you want unrolled specializations for fill and copy
#define WANT_UNROLLED_COPY 1

// Define to 1 if you want to use MMX/SSE/3dNow! processor instructions
#undef WANT_MMX

// Define to byte sizes of types
#define SIZE_OF_CHAR 1
#define SIZE_OF_SHORT 2
#define SIZE_OF_INT 3
#define SIZE_OF_LONG 4
#define SIZE_OF_LONG_LONG 8
#define SIZE_OF_POINTER 3
#define SIZE_OF_SIZE_T 3
#define SIZE_OF_BOOL SIZE_OF_CHAR
#define SIZE_T_IS_LONG 0

// Byte order macros, converted in utypes.h
#define USTL_LITTLE_ENDIAN	4321
#define USTL_BIG_ENDIAN		1234
#define USTL_BYTE_ORDER		USTL_LITTLE_ENDIAN

// Extended CPU capabilities
#undef CPU_HAS_FPU
#undef CPU_HAS_EXT_DEBUG
#undef CPU_HAS_TIMESTAMPC
#undef CPU_HAS_MSR
#undef CPU_HAS_CMPXCHG8
#undef CPU_HAS_APIC
#undef CPU_HAS_SYSCALL
#undef CPU_HAS_MTRR
#undef CPU_HAS_CMOV
#undef CPU_HAS_FCMOV
#if WANT_MMX
#undef CPU_HAS_MMX
#undef CPU_HAS_FXSAVE
#undef CPU_HAS_SSE 
#undef CPU_HAS_SSE2
#undef CPU_HAS_SSE3
#undef CPU_HAS_EXT_3DNOW
#undef CPU_HAS_3DNOW
#endif

// GCC vector extensions
#if (CPU_HAS_MMX || CPU_HAS_SSE) && __GNUC__ >= 3
    #define HAVE_VECTOR_EXTENSIONS 1
#endif

#if CPU_HAS_SSE && __GNUC__
    #define __sse_align	__attribute__((aligned(16)))
#else
    #define __sse_align	
#endif
