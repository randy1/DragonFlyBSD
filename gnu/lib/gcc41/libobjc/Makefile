# $DragonFly: src/gnu/lib/gcc41/libobjc/Makefile,v 1.1 2008/06/30 12:16:03 hasso Exp $

GCC_NO_PATH=	1
.include "../Makefile.inc"
.include "${STOPDIR}/cc_tools/Makefile.tools"

.PATH:		${GCCDIR}/libobjc ${GCCDIR}/libobjc/objc ${GCCDIR}/gcc

CFLAGS+=	-I${GCCDIR}/libobjc/objc -I. -DIN_TARGET_LIBS

LIB=		objc
SHLIB_MAJOR=	2

SRCS=		NXConstStr.m Object.m Protocol.m archive.c class.c encoding.c
SRCS+=		exception.c gc.c hash.c hash_compat.c init.c linking.m misc.c
SRCS+=		nil_method.c objects.c sarray.c selector.c sendmsg.c thr-objc.c
SRCS+=		thr.c
SRCS+=		insn-flags.h insn-constants.h runtime-info.h unwind.h

INCS=		NXConstStr.h Object.h Protocol.h encoding.h hash.h objc-api.h
INCS+=		objc-list.h objc.h runtime.h sarray.h thr.h typedstream.h
INCSDIR=	${INCLUDEDIR}/objc

unwind.h: unwind-generic.h
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+=	unwind.h

runtime-info.h: 
	echo "" > tmp-runtime.m
	echo "/* This file is automatically generated */" > ${.TARGET}
	${CC} -print-objc-runtime-info -S tmp-runtime.m >> ${.TARGET}
	rm -f tmp-runtime.m tmp-runtime.s

.include <bsd.lib.mk>
