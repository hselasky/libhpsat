#
# Makefile for Hans Petter's M-SAT solving library
#

LIB=		hpMsat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

SRCS= \
	hpMsat_eq.cpp \
	hpMsat_logic.cpp \
	hpMsat_solve.cpp

INCS= \
	hpMsat.h

.include <bsd.lib.mk>
