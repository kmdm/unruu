AC_CONFIG_MACRO_DIR([m4])
AC_INIT([unruu],[0.2.0])
AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_PROG_LIBTOOL
AC_PROG_CC

AC_ARG_WITH([static], AS_HELP_STRING([--with-static],
            [Build unruu statically]))

AS_IF([test "x$with_static" = "xyes"], [
    AC_SUBST([EXTRA_LDFLAGS], [-all-static])
])

PKG_CHECK_MODULES([UNSHIELD], [libunshield >= 0.7], [], [AC_MSG_ERROR(
    [unruu requires at least version 0.7 of libunshield]
)])
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
