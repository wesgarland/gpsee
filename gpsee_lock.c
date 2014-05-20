/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/** @file	gpsee_lock.c	Cross-platform Compare-And-Swap operations
 *				This is just SpiderMonkey's js/src/jslock.c, stripped
 *				down and bent into the API we want.
 *
 *  @version	$Id: gpsee_lock.c,v 1.5 2011/12/05 19:13:36 wes Exp $
 *  @author	Wes Garland
 *  @date	Jan 2008
 *
 *  @note	File is a .c file, but treated ~ like a header. I want the compiler to
 *		have every possible opportunity to inline this code.
 */ 

static __attribute__((unused)) const char gpsee_lock_rcsid[]="$Id: gpsee_lock.c,v 1.5 2011/12/05 19:13:36 wes Exp $";

/*
 * JS locking stubs.
 */
#include <jsapi.h>
#include <stdlib.h>

#include "gpsee_config.h"

#if defined(__APPLE__)
#include <libkern/OSAtomic.h>

static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
  /* XXX Barrier needed on ppc? */
#ifdef __LP64__
  return OSAtomicCompareAndSwap64Barrier((int64_t)ov, (int64_t)nv, (volatile int64_t *)w);
#else
  return OSAtomicCompareAndSwap32Barrier((int32_t)ov, (int32_t)nv, (volatile int32_t *)w);
#endif
}

#elif defined(HAVE_ATOMICH_CAS)
#include <atomic.h>

static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
  return atomic_cas_ptr(w, (void *)ov, (void *)nv) == (void *)ov;
}


#elif __GNUC__ > 4 \
  || ( __GNUC__ == 4 && __GNUC_MINOR__ >= 5 )
  
static inline JSBool
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    return __sync_bool_compare_and_swap( w, ov, nv ) ? JS_TRUE : JS_FALSE;
}


#else

#include <memory.h>

/* Exclude Alpha NT. */
#if defined(_WIN32) && defined(_M_IX86)
#pragma warning( disable : 4035 )

static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    __asm {
        mov eax, ov
        mov ecx, nv
        mov ebx, w
        lock cmpxchg [ebx], ecx
        sete al
        and eax, 1h
    }
}

#elif defined(__GNUC__) && defined(__i386__)

/* Note: This fails on 386 cpus, cmpxchgl is a >= 486 instruction */
static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                          "lock\n"
                          "cmpxchgl %2, (%1)\n"
                          "sete %%al\n"
                          "andl $1, %%eax\n"
                          : "=a" (res)
                          : "r" (w), "r" (nv), "a" (ov)
                          : "cc", "memory");
    return (int)res;
}

#elif (defined(__USLC__) || defined(_SCO_DS)) && defined(i386)

/* Note: This fails on 386 cpus, cmpxchgl is a >= 486 instruction */

asm int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
%ureg w, nv;
	movl	ov,%eax
	lock
	cmpxchgl nv,(w)
	sete	%al
	andl	$1,%eax
%ureg w;  mem ov, nv;
	movl	ov,%eax
	movl	nv,%ecx
	lock
	cmpxchgl %ecx,(w)
	sete	%al
	andl	$1,%eax
%ureg nv;
	movl	ov,%eax
	movl	w,%edx
	lock
	cmpxchgl nv,(%edx)
	sete	%al
	andl	$1,%eax
%mem w, ov, nv;
	movl	ov,%eax
	movl	nv,%ecx
	movl	w,%edx
	lock
	cmpxchgl %ecx,(%edx)
	sete	%al
	andl	$1,%eax
}
#pragma asm full_optimization js_CompareAndSwap

#elif defined(__GNUC__) && defined(__x86_64__)
static JS_ALWAYS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                          "lock\n"
                          "cmpxchgq %2, (%1)\n"
                          "sete %%al\n"
                          "movzbl %%al, %%eax\n"
                          : "=a" (res)
                          : "r" (w), "r" (nv), "a" (ov)
                          : "cc", "memory");
    return (int)res;
}
#elif defined(SOLARIS) && defined(sparc) && defined(ULTRA_SPARC)

static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
#if defined(__GNUC__)
    unsigned int res;
    JS_ASSERT(ov != nv);
    asm volatile ("\
stbar\n\
cas [%1],%2,%3\n\
cmp %2,%3\n\
be,a 1f\n\
mov 1,%0\n\
mov 0,%0\n\
1:"
                  : "=r" (res)
                  : "r" (w), "r" (ov), "r" (nv));
    return (int)res;
#else /* !__GNUC__ */
    extern int compare_and_swap(jsword*, jsword, jsword);
    JS_ASSERT(ov != nv);
    return compare_and_swap(w, ov, nv);
#endif
}

#elif defined(AIX)

#include <sys/atomic_op.h>

static JS_INLINE int __attribute__((unused))
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    return !_check_lock((atomic_p)w, ov, nv);
}

#else

#if !defined(MAKEDEPEND)
# error "Compare-and-swap appears not to be implemented on your platform."
#endif /* !MAKEDEPEND */

#endif /* HAVE_ATOMICH_CAS */
#endif /* arch-tests */
