/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 *
 * $NetBSD: s_scalbnf.S,v 1.7 2006/03/21 11:35:21 drochner Exp $
 * $DragonFly: src/lib/libm/arch/i386/s_scalbnf.S,v 1.2 2007/06/16 23:53:00 pavalos Exp $
 */

#include <machine/asm.h>

ENTRY(scalbnf)
#ifdef __x86_64__
	movl %edi,-8(%rsp)
	fildl -8(%rsp)
	movss %xmm0,-4(%rsp)
	flds -4(%rsp)
	fscale
	fstps -4(%rsp)
	movss -4(%rsp),%xmm0
	fstp %st(0)
#else
	fildl 8(%esp)
	flds 4(%esp)
	fscale
	fstp %st(1)		/* clean up stack */
#endif
	ret
