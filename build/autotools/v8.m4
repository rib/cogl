# SYNOPSIS
#
#   AX_LIB_V8([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the V8 library from Google for embedding Javascript into C++ programs.
#
#   If no path to the installed boost library is given the macro searchs
#   under /usr, /usr/local and /opt.
#
#   This macro calls:
#
#     AC_SUBST(V8_CPPFLAGS) / AC_SUBST(V8_LDFLAGS)
#
#   And sets:
#
#     HAVE_V8
#
# LICENSE
#
#   Copyright (c) 2009 Graham Cox <graham@grahamcox.co.uk>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.

AC_DEFUN([AX_LIB_V8],
[
    AC_ARG_WITH([v8],
        AC_HELP_STRING(
            [--with-v8=@<:@ARG@:>@],
            [use V8 library @<:@default=yes@:>@, optionally specify the prefix for v8 library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_V8="no"
        elif test "$withval" = "yes"; then
            WANT_V8="yes"
            ac_v8_path=""
        else
            WANT_V8="yes"
            ac_v8_path="$withval"
        fi
        ],
        [WANT_V8="yes"]
    )

    V8_CPPFLAGS=""
    V8_LDFLAGS=""

    if test "x$WANT_V8" = "xyes"; then

        ac_v8_header="v8.h"

        v8_version_req=ifelse([$1], [], [2.0.5.2], [$1])
        v8_version_req_shorten=`expr $v8_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        v8_version_req_major=`expr $v8_version_req : '\([[0-9]]*\)'`
        v8_version_req_minor=`expr $v8_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        v8_version_req_micro=`expr $v8_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        v8_version_req_nano=`expr $v8_version_req : '[[0-9]]*\.[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$v8_version_req_nano" = "x" ; then
            v8_version_req_nano="0"
        fi

        v8_version_req_number=`expr $v8_version_req_major \* 1000000000 \
                                   \+ $v8_version_req_minor \* 1000000 \
                                   \+ $v8_version_req_micro \* 1000 \
                                   \+ $v8_version_req_nano`

        AC_MSG_CHECKING([for V8 library >= $v8_version_req])

        if test "$ac_v8_path" != ""; then
            ac_v8_ldflags="-L$ac_v8_path/lib"
            ac_v8_cppflags="-I$ac_v8_path/include"
        else
            for ac_v8_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_v8_path_tmp/include/$ac_v8_header" \
                    && test -r "$ac_v8_path_tmp/include/$ac_v8_header"; then
                    ac_v8_path=$ac_v8_path_tmp
                    ac_v8_cppflags="-I$ac_v8_path_tmp/include"
                    ac_v8_ldflags="-L$ac_v8_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_v8_ldflags="$ac_v8_ldflags -lv8"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_v8_cppflags"

        saved_LDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS $ac_v8_ldflags"

        AC_LANG_PUSH([C++])
        AC_TRY_COMPILE([#include <v8.h>], ,
            success=yes, success=no)
        if test "$success" = "yes"; then
            AC_TRY_LINK([#include <v8.h>], 
                [v8::V8::GetVersion();],
                success=yes, success=no)
        fi
        if test "$success" = "yes"; then
            AC_TRY_RUN([#include <v8.h>
                int main() {
                    int major, minor, micro, nano;
                    sscanf(v8::V8::GetVersion(), "%d.%d.%d.%d", &major, &minor, &micro, &nano);
                    int version = (major * 1000000000)
                                + (minor * 1000000)
                                + (micro * 1000)
                                + nano;
                    if (version >= $v8_version_req_number) {
                        return 0;
                    }
                    else {
                        return 1;
                    }
                }
                ], [success=yes], [success=no])
        fi
        AC_LANG_POP([C++])

        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDFLAGS"

        if test "$success" = "yes"; then
            AC_MSG_RESULT([found in $ac_v8_path])

            V8_CPPFLAGS="$ac_v8_cppflags"
            V8_LDFLAGS="$ac_v8_ldflags"

            AC_SUBST(V8_CPPFLAGS)
            AC_SUBST(V8_LDFLAGS)
            AC_DEFINE([HAVE_V8], [], [Have the V8 library])
            ifelse([$2], , :, [$2])
        else
            AC_MSG_RESULT([not found])
            ifelse([$3], , :, [$3])
        fi
    fi
])
