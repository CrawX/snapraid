dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.65])
AC_INIT([snapraid], [9.0], [], [], [http://www.snapraid.it])
AM_INIT_AUTOMAKE([foreign no-dependencies subdir-objects])
AC_CONFIG_SRCDIR([cmdline/snapraid.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_CHECK_PROG([VALGRIND],[valgrind],[valgrind],[])
AC_CHECK_PROG([WINE],[wine],[wine],[])
AC_CHECK_PROG([SDE],[sde],[sde],[])
AC_CHECK_PROG([ADVD2],[advd2],[advd2],[])
AM_CONDITIONAL(HAVE_ADVD2, [test x"$ADVD2" != x])

dnl Checks for system.
AC_SYS_LARGEFILE

dnl Checks for header files.
AC_HEADER_ASSERT
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stddef.h stdint.h stdlib.h string.h limits.h])
AC_CHECK_HEADERS([unistd.h getopt.h fnmatch.h io.h inttypes.h byteswap.h])
AC_CHECK_HEADERS([pthread.h math.h])
AC_CHECK_HEADERS([sys/file.h sys/ioctl.h sys/vfs.h sys/statfs.h sys/param.h sys/mount.h])
AC_CHECK_HEADERS([linux/fiemap.h linux/fs.h mach/mach_time.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_C_VOLATILE
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_INT8_T
AC_STRUCT_DIRENT_D_INO
AC_STRUCT_DIRENT_D_TYPE
AC_CHECK_MEMBERS([struct stat.st_nlink, struct stat.st_mtim.tv_nsec, struct stat.st_mtimensec, struct stat.st_mtimespec.tv_nsec], [], [], [[
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
]])
AC_CHECK_MEMBERS([struct statfs.f_type], [], [], [[
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
]])

dnl Checks for library functions.
AC_CHECK_FUNCS([memset strchr strerror strrchr mkdir gettimeofday strtoul])
AC_CHECK_FUNCS([getopt getopt_long snprintf vsnprintf sigaction])
AC_CHECK_FUNCS([ftruncate fallocate fsync access posix_fallocate posix_fadvise])
AC_CHECK_FUNCS([pread pwrite getc_unlocked ferror_unlocked fnmatch])
AC_CHECK_FUNCS([futimes futimens futimesat localtime_r])
AC_CHECK_FUNCS([fstatat flock statfs])
AC_CHECK_FUNCS([mach_absolute_time])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_CC_OPT([-pthread], CFLAGS="$CFLAGS -pthread", [])
AC_CHECK_FUNCS([pthread_create])
AC_SEARCH_LIBS([exp], [m])

dnl Checks for architecture
AC_C_BIGENDIAN

dnl Checks for compiler
AC_CHECK_CC_OPT([-Wall], CFLAGS="$CFLAGS -Wall", [])
AC_CHECK_CC_OPT([-Wextra], CFLAGS="$CFLAGS -Wextra", [])
AC_CHECK_CC_OPT([-Wuninitialized], CFLAGS="$CFLAGS -Wuninitialized", [])
AC_CHECK_CC_OPT([-Wshadow], CFLAGS="$CFLAGS -Wshadow", [])

dnl Checks for AS supporting the SSE2 instructions.
AC_MSG_CHECKING([for sse2])
asmsse2=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if defined(__i386__) || defined(__x86_64__)
	void f(void)
	{
		asm volatile("pxor %xmm0,%xmm1");
	}
#else
#error not x86
#endif
]])],
[AC_DEFINE([HAVE_SSE2], [1], [Define to 1 if sse2 is supported by the assembler.]) asmsse2=yes])
AC_MSG_RESULT([$asmsse2])

dnl Checks for AS supporting the SSSE3 instructions.
AC_MSG_CHECKING([for ssse3])
asmssse3=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if defined(__i386__) || defined(__x86_64__)
	void f(void)
	{
		asm volatile("pshufb %xmm0,%xmm1");
	}
#else
#error not x86
#endif
]])],
[AC_DEFINE([HAVE_SSSE3], [1], [Define to 1 if ssse3 is supported by the assembler.]) asmssse3=yes])
AC_MSG_RESULT([$asmssse3])

dnl Checks for AS supporting the SSE4.2 instructions.
AC_MSG_CHECKING([for sse42])
asmsse42=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if defined(__i386__) || defined(__x86_64__) 
	unsigned f(unsigned crc, unsigned char b)
	{
		asm volatile("crc32b %1, %0" : "+r" (crc) : "rm" (b));
		return crc;
	}
#else
#error not x86
#endif
]])],
[AC_DEFINE([HAVE_SSE42], [1], [Define to 1 if sse4.2 is supported by the assembler.]) asmsse42=yes])
AC_MSG_RESULT([$asmsse42])

dnl Checks for AS supporting the AVX2 instructions.
AC_MSG_CHECKING([for avx2])
asmavx2=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if defined(__i386__) || defined(__x86_64__)
	void f(void* ptr)
	{
		asm volatile("vbroadcasti128 %0, %%ymm0" : : "m" (ptr));
	}
#else
#error not x86
#endif
]])],
[AC_DEFINE([HAVE_AVX2], [1], [Define to 1 if avx2 is supported by the assembler.]) asmavx2=yes])
AC_MSG_RESULT([$asmavx2])

dnl Checks for test environment
AS_CASE([$host],
	[*-*-mingw*],
	[
	TESTENV="$WINE"
	FAILENV="$TESTENV"
	],
	[SYMLINK=1]
)
AM_CONDITIONAL(HAVE_SYMLINK, [test x"$SYMLINK" != x])

AC_ARG_ENABLE([profiler],
	[AS_HELP_STRING([--enable-profiler],[enable the use of gprof for code coverage])],
	[
	CFLAGS="-O2 -pg -g -pthread"
	],
	[])

AC_ARG_ENABLE([coverage],
	[AS_HELP_STRING([--enable-coverage],[enable the use of gcov for code coverage])],
	[
	CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage -pthread"
	],
	[])

AC_ARG_ENABLE([valgrind],
	[AS_HELP_STRING([--enable-valgrind],[enable the use of valgrind in testing])],
	[
	TESTENV="$VALGRIND --leak-check=full --error-exitcode=1"
	FAILENV="$VALGRIND --error-exitcode=1"
	CFLAGS="$CFLAGS -DCHECKER"
	MEMORY_CHECKER=1
	],
	[])

AC_ARG_ENABLE([sgcheck],
	[AS_HELP_STRING([--enable-sgcheck],[enable the use of sgcheck in testing])],
	[
	TESTENV="$VALGRIND --tool=exp-sgcheck --suppressions=valgrind.supp --error-exitcode=1"
	FAILENV="$TESTENV"
	CFLAGS="$CFLAGS -DCHECKER"
	MEMORY_CHECKER=1
	],
	[])

AM_CONDITIONAL(HAVE_MEMORY_CHECKER, [test x"$MEMORY_CHECKER" != x])

AC_ARG_ENABLE([helgrind],
	[AS_HELP_STRING([--enable-helgrind],[enable the use of helgrind in testing])],
	[
	TESTENV="$VALGRIND --tool=helgrind --suppressions=valgrind.supp --error-exitcode=1"
	FAILENV="$TESTENV"
	CFLAGS="$CFLAGS -DCHECKER"
	THREAD_CHECKER=1
	],
	[])

AC_ARG_ENABLE([drd],
	[AS_HELP_STRING([--enable-drd],[enable the use of drd in testing])],
	[
	TESTENV="$VALGRIND --tool=drd --suppressions=valgrind.supp --error-exitcode=1"
	FAILENV="$TESTENV"
	CFLAGS="$CFLAGS -DCHECKER"
	THREAD_CHECKER=1
	],
	[])

AM_CONDITIONAL(HAVE_THREAD_CHECKER, [test x"$THREAD_CHECKER" != x])

AC_ARG_ENABLE([sde],
	[AS_HELP_STRING([--enable-sde],[enable the use of SDE emulator in testing])],
dnl p4 -> Pentium4 with SSE2
dnl mrm -> Merom with SSSE3
dnl nhm -> Nehalem with SSE4.2
dnl hsw -> Haswell with AVX2
dnl knl -> Knights Landing with AVX512
	[
	TESTENV_SSE2="$SDE -p4 --"
	TESTENV_SSSE3="$SDE -mrm --"
	TESTENV_SSE42="$SDE -nhm --"
	TESTENV_AVX2="$SDE -hsw --"
	EMULATOR=1
	],
	[])
AM_CONDITIONAL(HAVE_EMULATOR, [test x"$EMULATOR" != x])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug],[enable debugging])],
	[
	CFLAGS="-O0 -g -pthread -Wall -Wextra"
	],
	[])

AC_ARG_ENABLE([warning],
	[AS_HELP_STRING([--enable-warning],[enable extra warning])],
	[
	AC_CHECK_CC_OPT([-Wpointer-arith], CFLAGS="$CFLAGS -Wpointer-arith", [])
	AC_CHECK_CC_OPT([-Wcast-qual], CFLAGS="$CFLAGS -Wcast-qual", [])
	AC_CHECK_CC_OPT([-Wunused], CFLAGS="$CFLAGS -Wunused", [])
	AC_CHECK_CC_OPT([-Wunreachable-code], CFLAGS="$CFLAGS -Wunreachable-code", [])
	AC_CHECK_CC_OPT([-Wpadded], CFLAGS="$CFLAGS -Wpadded", [])
	AC_CHECK_CC_OPT([-Wshadow], CFLAGS="$CFLAGS -Wshadow", [])
	AC_CHECK_CC_OPT([-Weverything], CFLAGS="$CFLAGS -Weverything", [])
	],
	[])

AC_ARG_VAR([TESTENV], [Test environment])
AC_ARG_VAR([FAILENV], [Test environment for failing tests])
AC_ARG_VAR([TESTENV_SSE2], [Test environment for SSE2])
AC_ARG_VAR([TESTENV_SSSE3], [Test environment for SSSE3])
AC_ARG_VAR([TESTENV_SSE42], [Test environment for SSE42])
AC_ARG_VAR([TESTENV_AVX2], [Test environment for AVX2])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

