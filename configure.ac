AC_INIT([unruu],[0.1])
AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_PROG_CC

PKG_CHECK_MODULES([UNSHIELD], [libunshield >= 0.7], [], [AC_MSG_ERROR(
    [unruu requires at least version 0.7 of libunshield]
)])
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
