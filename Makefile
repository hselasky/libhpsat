#
# Makefile for Hans Petter's 3-SAT solving library
#

LIB=		hp3sat
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wno-invalid-offsetof
MAN=

.if defined(HAVE_DEBUG)
CFLAGS+= -g -O0 -DDEBUG
.endif

SRCS= \
	hp3sat_adder.cpp \
	hp3sat_andmap.cpp \
	hp3sat_bitmap.cpp \
	hp3sat_cnf.cpp \
	hp3sat_compress.cpp \
	hp3sat_demux.cpp \
	hp3sat_explore_path.cpp \
	hp3sat_logic_builder.cpp \
	hp3sat_solve.cpp \
	hp3sat_simplify.cpp \
	hp3sat_xormap.cpp

INCS= \
	hp3sat.h

.include <bsd.lib.mk>
