AC_INIT([stuff], [0.1], [the_jk@yahoo.com])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([dist-bzip2 foreign])
AM_SILENT_RULES([yes])
LT_INIT([disable-shared])
AC_PROG_CXX
AM_PROG_CC_C_O

AC_LANG([C++])

DEFINES=
AX_APPEND_COMPILE_FLAGS([-fno-rtti -fno-exceptions],DEFINES)

# Test c++11
OLDCXXFLAGS="$CXXFLAGS"
# Check if it just works with -std=c++11
# The code below was choosen because if you mix a compiler that is C++11
# compatible with a libc++ that isn't fully (like clang 3.3 with gcc 4.6
# libstdcxx) you get errors because of a missing copy constructor for
# std::shared_ptr. Add more tests as we find them.
CXXFLAGS="-std=c++11 $CXXFLAGS"
AC_MSG_CHECKING([for C++11 using (-std=c++11)])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <memory>
]],[[
std::shared_ptr<int> i(new int(5));
std::shared_ptr<int> j(i);
]])],
               [AC_MSG_RESULT([yes])
                DEFINES="$DEFINES -std=c++11"],
               [AC_MSG_RESULT([no])
                CXXFLAGS="-std=c++11 -stdlib=libc++ $CXXFLAGS"
                AC_MSG_CHECKING([for C++11 using (-std=c++11 -stdlib=libc++)])
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <memory>
]],[[
std::shared_ptr<int> i(new int(5));
std::shared_ptr<int> j(i);
]])],
                               [AC_MSG_RESULT([yes])
                                DEFINES="$DEFINES -std=c++11 -stdlib=libc++"],
                               [AC_MSG_RESULT([no])
                                AC_MSG_ERROR([No working C++11 support])])])
CXXFLAGS="$OLDCXXFLAGS"

AX_CXXFLAGS_WARN_ALL([DEFINES])
AX_APPEND_COMPILE_FLAGS([-Wextra -Wno-unused-parameter],DEFINES)
AC_ARG_ENABLE([debug], AC_HELP_STRING([compile with debug options]),
              if test "x$enableval" = "xyes"; then
                DEFINES="$DEFINES -g -DDEBUG"
              else
                DEFINES="$DEFINES -DNDEBUG"
              fi)
AC_SUBST([DEFINES])

# SQLite3

# 3.6.5 so that sqlite3_changes() return correct values for DELETE
PKG_CHECK_MODULES([SQLITE3],[sqlite3 >= 3.6.5])

# Finish up

AC_CONFIG_HEADERS([src/config.h])
AC_OUTPUT([Makefile src/Makefile])
