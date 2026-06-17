sbin_PROGRAMS=solidbits
solidbits_SOURCES=main.c solidbits.h xxhash.h helper.c net.c net.h parser.c xxhash.c file.c bitops.c
# config.h is generated at the top build dir; find it plus the top source dir.
AM_CPPFLAGS=-I$(top_builddir) -I$(top_srcdir)
solidbits_CFLAGS=$(LIBUV_CFLAGS)
solidbits_LDADD=$(LIBUV_LIBS)
