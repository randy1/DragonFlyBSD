# $FreeBSD: src/sys/modules/sppp/Makefile,v 1.5.2.1 2002/06/17 06:14:40 jhay Exp $
# $DragonFly: src/sys/net/sppp/Makefile,v 1.4 2005/02/18 11:41:42 corecode Exp $

.PATH: ${.CURDIR}/../ppp_layer

KMOD=	sppp
SRCS=	if_spppsubr.c slcompress.c
SRCS+=	opt_inet.h opt_inet6.h opt_ipx.h

EXPORT_SYMS=	sppp_attach	\
		sppp_detach	\
		sppp_input	\
		sppp_ioctl	\
		sppp_dequeue	\
		sppp_pick	\
		sppp_isempty	\
		sppp_flush

.if !defined(BUILDING_WITH_KERNEL)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h
.endif

.include <bsd.kmod.mk>
