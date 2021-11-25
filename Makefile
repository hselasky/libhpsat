#
# Makefile for Hans Petter's N-SAT solving library
#

LIB=		hpNsat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

SRCS= \
	hpNsat_andmap.cpp \
	hpNsat_cnf.cpp \
	hpNsat_solve.cpp

INCS= \
	hpNsat.h

.include <bsd.lib.mk>
