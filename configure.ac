#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.50)
AC_INIT([Grid Tools], [0.16.3], [David.Coss@stjude.org])
AC_CONFIG_SRCDIR([src])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

#templates
AH_TEMPLATE([USE_BOINC],[Define to use the BOINC library])

# Checks for programs.
AC_PROG_CXX
AC_PROG_FC
AC_PROG_RANLIB

AM_PATH_PYTHON
AC_ARG_VAR([PYTHON_INCLUDE], [Include flags for python, bypassing python-config])
AC_ARG_VAR([PYTHON_CFLAGS], [C compiler flags for python, including includes, bypassing python-config])
AC_ARG_VAR([PYTHON_LDFLAGS], [C linker flags for python, including libraries, bypassing python-config])
AC_ARG_VAR([PYTHON_CONFIG], [Path to python-config])
AS_IF([test -z "$PYTHON_INCLUDE"], [
  AS_IF([test -z "$PYTHON_CONFIG"], [
    AC_PATH_PROGS([PYTHON_CONFIG],
                  [python$PYTHON_VERSION-config python-config],
                  [no],
                  [`dirname $PYTHON`])
    AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON.])])
  ])
  AC_MSG_CHECKING([python include flags])
  PYTHON_INCLUDE=`$PYTHON_CONFIG --includes`
  AC_MSG_RESULT([$PYTHON_INCLUDE])
])

AS_IF([test -z "$PYTHON_CFLAGS"], [
  AS_IF([test -z "$PYTHON_CONFIG"], [
    AC_PATH_PROGS([PYTHON_CONFIG],
                  [python$PYTHON_VERSION-config python-config],
                  [no],
                  [`dirname $PYTHON`])
    AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON.])])
  ])
  AC_MSG_CHECKING([python cflags])
  PYTHON_CFLAGS=`$PYTHON_CONFIG --cflags`
  AC_MSG_RESULT([$PYTHON_CFLAGS])
])

AS_IF([test -z "$PYTHON_LDFLAGS"], [
  AS_IF([test -z "$PYTHON_CONFIG"], [
    AC_PATH_PROGS([PYTHON_CONFIG],
                  [python$PYTHON_VERSION-config python-config],
                  [no],
                  [`dirname $PYTHON`])
    AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON.])])
  ])
  AC_MSG_CHECKING([python ldflags])
  PYTHON_LDFLAGS=`$PYTHON_CONFIG --ldflags`
  AC_MSG_RESULT([$PYTHON_LDFLAGS])
])






# Checks for libraries.

AC_ARG_WITH(mysql_config,
        AC_HELP_STRING(
                [--with-mysql_config=PROGRAM],
                [location of the mysql_config program]),
        [mysql_config_prog=$withval])
AC_PATH_PROG(MYSQL_CONFIG, mysql_config, $mysql_config_prog)

if test "x$MYSQL_CONFIG" = "x"; then
	AC_MSG_ERROR([Couldn't find mysql_config. Please verify that it is installed.])
fi
AC_SUBST(MYSQL_LIBS,$($MYSQL_CONFIG --libs))
AC_SUBST(MYSQL_CFLAGS,$($MYSQL_CONFIG --cflags))



# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_ARG_WITH([pthread],AS_HELP_STRING([--with-pthread@<:@=yes@:>@],[Compile with pthread.]),
	with_pthread=$withval)
if test x$with_pthread != xno; then
	with_pthread="-lpthread";
else
	with_pthread="";
fi

AC_MSG_CHECKING([Link statically])
AC_ARG_ENABLE([static],
	AS_HELP_STRING([--enable-static],[Enable building of grid tools statically. Default: enabled. To link against shared libraries, use --disable-static]),
	[enable_static="$enableval"],
	[enable_static=no])
AC_MSG_RESULT([$enable_static])
if test x"$enable_static" = x"yes"; then
   LDFLAGS="$LDFLAGS -static "
fi

AC_ARG_WITH([boinc],AS_HELP_STRING([--with-boinc@<:@=dir@:>@],[Include boinc under specified path @<:@Default=no@:>@]),with_boinc=${withval},AC_MSG_RESULT(Not using BOINC))
AM_CONDITIONAL([MAKE_BOINC_PROGRAMS],[test x$with_boinc != x])
if test x$with_boinc != x; then
	AM_CONDITIONAL([MAKE_BOINC_PROGRAMS],[test x$with_boinc != xno])
	if test x$with_boinc != xno;then 
   		AC_SUBST(BOINC_CPPFLAGS,"-I$with_boinc/include/  -I$with_boinc/boinc/ -I$with_boinc/boinc/api/ -I$with_boinc/boinc/lib/ -I$with_boinc/boinc/sched/ -I$with_boinc/boinc/db/ ")
   		AC_SUBST(BOINC_LIBS,"$with_boinc/lib/libsched.a $with_boinc/lib/libboinc_crypt.a $with_boinc/lib/libboinc_api.a $with_boinc/lib/libboinc.a $with_pthread")
   		AC_SUBST(BOINC_LDFLAGS,"-L$with_boinc/lib")
   		AC_DEFINE([USE_BOINC],[1])
   	fi   
fi


AC_CONFIG_FILES([Makefile
                 src/Makefile
		 src/mmpbsa-submit-src/Makefile])
AC_OUTPUT
