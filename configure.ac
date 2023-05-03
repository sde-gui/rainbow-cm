# -------------------------------------------------------------------------------
# Autoconf/automake.
# -------------------------------------------------------------------------------
AC_PREREQ(2.53)
AC_INIT([rainbow-cm], 1.0.0-alpha, https://github.com/sde-gui/rainbow-cm)
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

#AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([src/main.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# -------------------------------------------------------------------------------
# Gettext.
# -------------------------------------------------------------------------------

IT_PROG_INTLTOOL([0.50.1])

AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

AC_SUBST([GETTEXT_PACKAGE], [rainbow-cm])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], "$GETTEXT_PACKAGE", [Define to the gettext package name.])

# -------------------------------------------------------------------------------
# Main settings.
# -------------------------------------------------------------------------------
# prefix=/usr

# -------------------------------------------------------------------------------
#
# -------------------------------------------------------------------------------

AC_ARG_ENABLE(more-warnings,
    [AC_HELP_STRING([--enable-more-warnings],
        [compile with -Wall -Wextra])],
    [enable_more_warnings="${enableval}"],
    [enable_more_warnings=no])

if test "$enable_more_warnings" = "yes"; then
    CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
fi

# -------------------------------------------------------------------------------
# Checks for programs.
# -------------------------------------------------------------------------------
AC_PROG_CC()
AC_PROG_INSTALL()
AC_PROG_MAKE_SET()
AC_CHECK_FUNCS([memset])
PKG_PROG_PKG_CONFIG(0.20)

# -------------------------------------------------------------------------------
# Checks for libraries.
# -------------------------------------------------------------------------------

pkg_modules="gtk+-2.0 >= 2.24.0"
PKG_CHECK_MODULES([GTK], [$pkg_modules])

AC_SUBST(X11_LIBS, -lX11)

# -------------------------------------------------------------------------------
# Checks for header files.
# -------------------------------------------------------------------------------
AC_CHECK_HEADERS([stdlib.h string.h libintl.h])

# -------------------------------------------------------------------------------
# Checks for typedefs, structures, and compiler characteristics.
# -------------------------------------------------------------------------------
AC_C_CONST()
AC_C_INLINE()
AC_TYPE_SIZE_T()
AC_PATH_X()

# -------------------------------------------------------------------------------
# Checks for library functions.
# -------------------------------------------------------------------------------
AC_TYPE_SIGNAL()

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 data/Makefile])

AC_OUTPUT([po/Makefile.in])
