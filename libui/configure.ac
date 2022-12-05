AC_PREREQ(2.61)
AC_INIT([libui-frameworks], 1.0.0)
AM_INIT_AUTOMAKE([-Wall gnu foreign])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check for programs
AM_PROG_AR
LT_INIT
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

AS_CASE([$host],
        [arm*], [ARM=yes],
        [ARM=no]
)

ARM=yes

AC_ARG_WITH([glib],
      AS_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        AC_DEFINE(ENABLE_USEGLIB, 1, [Define if HLOS systems uses glib])
        PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GThread >= 2.16 is required))
        PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GLib >= 2.16 is required))
        GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
        GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"
        AC_SUBST(GLIB_CFLAGS)
        AC_SUBST(GLIB_LIBS)
fi

AC_ARG_WITH([libhardware],
      AS_HELP_STRING([--with-libhardware],
         [enable libhardware, building HLOS systems which use libhardware]))
if (test "x${with_libhardware}" = "xyes"); then
        AC_DEFINE(ENABLE_USELIBHARDWARE, 1, [Define if HLOS systems uses libhardware])
        PKG_CHECK_MODULES(LIBHARDWARE, hardware, dummy=yes,
                                AC_MSG_ERROR(libhardware is required))
        AC_SUBST(LIBHARDWARE_CFLAGS)
        AC_SUBST(LIBHARDWARE_LIBS)
fi

AC_ARG_WITH([opengl-headers],
      AS_HELP_STRING([--with-opengl-headers],
         [enable opengl headers, building HLOS systems which use opengl headers from libui]))
if (test "x${with_opengl_headers}" = "xyes"); then
        AC_DEFINE(ENABLE_OPENGL_HEADERS, 1, [Define if HLOS systems use opengl headers from libui])
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")
AM_CONDITIONAL(USE_LIBHARDWARE, test "x${with_libhardware}" = "xyes")
AM_CONDITIONAL(USE_OPENGL_HEADERS, test "x${with_opengl_headers}" = "xyes")

AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([LIBS])

AC_CONFIG_FILES([ \
        Makefile  \
        libs/ui/Makefile \
        libs/ui/libui.pc \
        include/Makefile \
        opengl/Makefile
        ])
AC_OUTPUT
