PROG_CXX=hpRsolve
LDADD=-lhpRsat -lgmp -lgmpxx -lm
PREFIX?=/usr/local
MAN=
SRCS=hpRsolve.cpp
BINDIR?=${PREFIX}/bin
CFLAGS+=-I${PREFIX}/include
LDFLAGS+=-L${PREFIX}/lib

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0
.endif

.include <bsd.prog.mk>
