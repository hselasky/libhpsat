#
# Makefile for Hans Petter's 2-SAT solving library
#

LIB=		hp2sat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

SRCS= \
	hp2sat_andmap.cpp \
	hp2sat_cnf.cpp \
	hp2sat_solve.cpp

INCS= \
	hp2sat.h

.include <bsd.lib.mk>
