/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 *
 * $NetBSD: e_remainderf.S,v 1.5 2003/07/26 19:24:59 salo Exp $
 * $DragonFly: src/lib/libm/arch/i386/e_remainderf.S,v 1.1 2005/07/26 21:15:19 joerg Exp $
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(remainderf)
	XMM_TWO_ARG_FLOAT_PROLOGUE
	flds	ARG_FLOAT_TWO
	flds	ARG_FLOAT_ONE
1:	fprem1
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_FLOAT_EPILOGUE
	ret
