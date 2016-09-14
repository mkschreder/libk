includedir=$(prefix)/include/libk/
lib_LTLIBRARIES=libk.la 
include_HEADERS=
AM_CFLAGS=$(CODE_COVERAGE_CFLAGS) -std=gnu99 -Wall -Werror -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
-Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
-Wshadow -Wcast-align -Wchar-subscripts -Winline \
-Wnested-externs -Wredundant-decls -Wmissing-field-initializers -Wextra \
-Wformat=2 -Wno-format-nonliteral -Wpointer-arith -Wno-missing-braces \
-Wno-unused-parameter -Wno-unused-variable -Wno-inline

libk_la_SOURCES=serial.c threads.c
libk_la_CFLAGS=$(AM_CFLAGS)

