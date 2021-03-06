# $DragonFly: src/gnu/usr.bin/binutils217/libbfd/Makefile,v 1.1 2007/04/13 12:24:32 corecode Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd ${SRCDIR}/opcodes

CONTRIBDIR=	${SRCDIR}/bfd

LIB=	bfd
SRCS+=	archive.c archive64.c archures.c bfd.c bfdio.c binary.c cache.c \
	coffgen.c corefile.c elf.c elf-eh-frame.c elf-strtab.c format.c \
	hash.c ihex.c init.c libbfd.c linker.c merge.c opncls.c reloc.c \
	section.c simple.c srec.c stab-syms.c stabs.c syms.c \
	targets.c tekhex.c \
	targmatch.h dwarf1.c dwarf2.c config.h bfd.h bfdver.h
.if (${TARGET_ARCH} == "alpha" || ${TARGET_ARCH} == "sparc64")
WARNS=	2
.endif
CFLAGS+= -I${SRCDIR}/bfd
INTERNALLIB=	true

CLEANFILES+=	bfd.h bfdver.h targmatch.h

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH: ${BINUTILSDISTDIR}/bfd ${BINUTILSDISTDIR}/opcodes
CFLAGS+= -I${BINUTILSDISTDIR}/bfd -I${BINUTILSDISTDIR}/include -I.
.endif

# XXX broken
SELARCH=
.if ${BINUTIL_ARCH} == "x86_64"
SELARCH= &bfd_i386_arch
.else
.for _a in ${ARCHS}
.if ${SELARCH} == ""
SELARCH+= &bfd_${_a}_arch
.else
SELARCH+= ,&bfd_${_a}_arch
.endif
.endfor
.endif
CFLAGS+= -DSELECT_ARCHITECTURES="${SELARCH}"

SELVEC=
.for _v in ${VECS}
CFLAGS+= -DHAVE_${_v}
.if ${SELVEC} == ""
SELVEC+= &${_v}
.else
SELVEC+= ,&${_v}
.endif
.endfor
.if defined(DEFAULT_VECTOR)
CFLAGS+= -DDEFAULT_VECTOR=${DEFAULT_VECTOR}
.endif
CFLAGS+= -DSELECT_VECS="${SELVEC}"

# Used to do keyword replacement in bfd-X.h
#
.if ${MACHINE_ARCH} == "i386"
BFD_HOST_64BIT_LONG?=0
BFD_HOST_64_BIT?=long long
BFD_HOST_U_64_BIT?=unsigned long long
.else
BFD_HOST_64BIT_LONG?=1
BFD_HOST_64_BIT?=long long
BFD_HOST_U_64_BIT?=unsigned long long
.endif
BFD_HOST_64_BIT_DEFINED?=1
BFD_HOST_LONG_LONG?=1

bfd.h: 	${SRCDIR}/bfd/bfd-in2.h
	sed -e "s/@wordsize@/${BFD_ARCH_SIZE}/g" \
	    -e "s/@bfd_default_target_size@/${BFD_DEFAULT_TARGET_SIZE}/g" \
	    -e "s/@BFD_HOST_64BIT_LONG@/${BFD_HOST_64BIT_LONG}/g" \
	    -e "s/@BFD_HOST_64_BIT@/${BFD_HOST_64_BIT}/g" \
	    -e "s/@BFD_HOST_U_64_BIT@/${BFD_HOST_U_64_BIT}/g" \
	    -e "s/@BFD_HOST_64_BIT_DEFINED@/${BFD_HOST_64_BIT_DEFINED}/g" \
	    -e "s/@BFD_HOST_LONG_LONG@/${BFD_HOST_LONG_LONG}/g" \
	    -e "s/@bfd_file_ptr@/${BFD_HOST_64_BIT}/g" \
	    < ${SRCDIR}/bfd/bfd-in2.h > bfd.h.new
	mv -f bfd.h.new bfd.h

bfdver.h: ${SRCDIR}/bfd/version.h
	sed -e "s/@bfd_version_string@/\\\"`echo -n ${VERSION}`\\\"/g" \
	    -e "s/@bfd_version@/${VERSION_BFD}/g" \
	    < ${SRCDIR}/bfd/version.h > bfdver.h.new
	mv -f bfdver.h.new bfdver.h

# match targets defined in the assembler's targ-env.h with an output vector.
# XXX i386-*-dragonfly and x86_64-*-dragonfly are obsolete, but not quite
# removed from the binutils build yet.
targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}.new
	echo ${TARGMATCH:Q} >> ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}
	
.include <bsd.lib.mk>
