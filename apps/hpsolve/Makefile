PROG_CXX=hpsolve
MAN=
SRCS=hpsolve.cpp
PREFIX?=/usr/local
BINDIR?=${PREFIX}/bin

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0
.endif

CFLAGS+= -I${PREFIX}/include
LDFLAGS+= -L${PREFIX}/lib -lhpsat

.include <bsd.prog.mk>
