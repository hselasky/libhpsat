PROG_CXX=hpMsat
LDADD=-lhpMsat -pthread
MAN=
SRCS=hpMsat.cpp
PREFIX?=/usr/local
BINDIR?=${PREFIX}/bin

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0
.endif

.include <bsd.prog.mk>
