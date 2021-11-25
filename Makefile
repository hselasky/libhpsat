#
# Makefile for Hans Petter's 3-adic Root-SAT solving library
#

LIB=		hpRsat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

SRCS= \
	hpRsat_add.cpp \
	hpRsat_digraph.cpp \
	hpRsat_fft.cpp \
	hpRsat_mul.cpp \
	hpRsat_parse.cpp \
	hpRsat_solve.cpp \
	hpRsat_trig.cpp \
	hpRsat_var.cpp \

INCS= \
	hpRsat.h

.include <bsd.lib.mk>
