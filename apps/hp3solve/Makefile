PROG_CXX=hp3solve
LDADD=-lhp3sat
MAN=
SRCS=hp3solve.cpp
BINDIR?=/usr/local/bin

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0
.endif

.include <bsd.prog.mk>
