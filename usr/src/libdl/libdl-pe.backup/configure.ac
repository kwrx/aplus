
AC_PREREQ([2.69])
AC_INIT(libdl, 1.0.0)
AC_CONFIG_SRCDIR([src])


AC_PROG_CC
AC_PROG_CXX
AC_CHECK_TOOL([AR], "ar")


AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/time.h unistd.h])

AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

AC_CHECK_FUNCS([malloc], , AC_MSG_FAILURE("malloc not found"))
AC_CHECK_FUNCS([free], , AC_MSG_FAILURE("free not found"))
AC_CHECK_FUNCS([memset], , AC_MSG_FAILURE("memset not found"))

AC_SUBST([package], "libdl")
AC_SUBST([version], "1.0.0")
AC_SUBST([srcdir], "src")
AC_SUBST([tstdir], "test")

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
