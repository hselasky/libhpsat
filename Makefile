#
# Makefile for Hans Petter's SAT solving library
#

LIB=		hpsat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=
PREFIX?=/usr/local
LIBDIR?=${PREFIX}/lib
INCLUDEDIR?=${PREFIX}/include

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

.if defined(HAVE_VERIFY)
CFLAGS+= -DVERIFY
.endif

SRCS= \
	hpsat.cpp \
	hpsat_cnf.cpp \
	hpsat_simplify.cpp \
	hpsat_solve.cpp

INCS= \
	hpsat.h

.include <bsd.lib.mk>
