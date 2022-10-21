bin_PROGRAMS = gsar
# use not gsar_CPPFLAGS but AM_CPPFLAGS, because definition of
# simple '.c.o' '.cpp.o' is expand into waste many 'XXX.o'.
AM_CPPFLAGS = -x c -std=c99 -Wno-deprecated-declarations @GTK_CFLAGS@
gsar_LDADD = @GTK_LIBS@
gsar_SOURCES = \
	main.c bmp_grid.c gtk2_helper.c bmp_helper.c lock.c portab.c
