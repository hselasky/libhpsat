PROG_CXX=hp2solve
LDADD=-lhp2sat
MAN=
SRCS=hp2solve.cpp
BINDIR?=/usr/local/bin

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0
.endif

.include <bsd.prog.mk>
