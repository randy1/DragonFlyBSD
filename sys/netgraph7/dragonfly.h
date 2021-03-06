/*
 * Copyright (c) 2008 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/netgraph7/dragonfly.h,v 1.1 2008/06/26 23:05:35 dillon Exp $
 */

#include <sys/lock.h>
#include <sys/objcache.h>

struct mtx {
	struct lock	lock;
};

#define mtx_contested(mtx)	0

#define mtx_lock(mtx)	lockmgr(&(mtx)->lock, LK_EXCLUSIVE|LK_RETRY)
#define mtx_unlock(mtx)	lockmgr(&(mtx)->lock, LK_RELEASE)
#define mtx_assert(mtx, unused) 		\
			(lockstatus(&(mtx)->lock, curthread) == LK_EXCLUSIVE)
#define mtx_init(mtx, name, something, type)	\
			lockinit(&(mtx)->lock, name, 0, 0)
#define mtx_destroy(mtx)			\
			lockuninit(&(mtx)->lock)
#define mtx_trylock(mtx)			\
			(lockmgr(&(mtx)->lock, LK_EXCLUSIVE|LK_NOWAIT) == 0)

#define printf		kprintf
#define snprintf	ksnprintf

typedef struct objcache	*objcache_t;
#define uma_zone_t	objcache_t

#define CTR1(ktr_line, ...)
#define CTR2(ktr_line, ...)
#define CTR3(ktr_line, ...)
#define CTR4(ktr_line, ...)
#define CTR5(ktr_line, ...)
#define CTR6(ktr_line, ...)
#define cpu_spinwait()	cpu_pause()

#define splnet()	0
#define splx(v)

