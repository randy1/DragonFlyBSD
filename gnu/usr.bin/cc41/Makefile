# $DragonFly: src/gnu/usr.bin/cc41/Makefile,v 1.3 2008/06/30 12:16:03 hasso Exp $

SUBDIR=		cc_prep
SUBDIR+=	libiberty
SUBDIR+=	cc_tools libbackend libcpp cc1 cc doc
SUBDIR+=	cpp
SUBDIR+=	gcov
.if !defined(NO_CXX)
SUBDIR+=	cc1plus c++ c++filt
.endif
.if !defined(NO_OBJC)
SUBDIR+=	cc1obj
.endif

.ORDER: ${SUBDIR}

.include <bsd.subdir.mk>
