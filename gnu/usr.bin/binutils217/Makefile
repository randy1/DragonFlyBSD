# $DragonFly: src/gnu/usr.bin/binutils217/Makefile,v 1.1 2007/04/13 12:24:32 corecode Exp $

SUBDIR=		libiberty libbfd libopcodes libbinutils \
		addr2line ar as ld nm objcopy objdump ranlib readelf \
		size strings strip doc

TARGET_ARCH?=	${MACHINE_ARCH}

# interdependant header files
.ORDER: ${SUBDIR}

.include <bsd.subdir.mk>
