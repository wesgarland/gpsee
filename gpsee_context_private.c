/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is PageMail, Inc.
 *
 * Portions created by the Initial Developer are 
 * Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
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
 * ***** END LICENSE BLOCK ***** 
 */

/** 
 *  @file	gpsee_context_private.c 	GPSEE JS_ContextPrivate() storage multiplexer. Allows one
 *						private storage element (of arbitrary size) per (cx,id) pair.
 *						Consumes runtime-global context callback, but tries to replicate
 *						similar functionality.
 *  @author	Wes Garland
 *  @date	Jan 2008
 *  @version	$Id: gpsee_context_private.c,v 1.4 2010/05/12 01:12:50 wes Exp $
 *
 *  @note	If anything (class, module, etc) in the embedding makes use of this mechanism for context-private
 *		storage, *everything* must be make use it for content-private storage.
 *
 *  @warning	There is still some cruft around the routines which are supposed to be provide a substitute
 *		for JS_SetContextCallback -- they probably don't work as expected.
 */

static __attribute__((unused)) const char gpsee_rcsid[]="$Id: gpsee_context_private.c,v 1.4 2010/05/12 01:12:50 wes Exp $";

#define _GPSEE_INTERNALS
#include "gpsee.h"

typedef struct
{
  size_t	listSize;
  struct
  {
    JSContext 		*cx;
    const void		*id;
    JSContextCallback	cb;
    void 		*storage;
  } *list;
} gpsee_context_private_t;

/** Destroy (free) all memory allocated by gpsee_getContextPrivate(). Intended to be
 *  called by the context callback when the context is being destroyed.
 *
 *  @warning	Does not do thread-exclusion locking. HOWEVER, it should only be executed by a 
 *		context associated with the current thread. Provided this contract is not broken,
 *		it should be safe to run unlocked, since the data structures are unique across
 *		contexts. This caveat a subtle implication embeddings using this facility:
 *		the thread destroying a context must also be thread that owns it, via JS_SetContextThread().
 *
 */
static JSBool gpsee_destroyContextPrivate(JSContext *cx)
{
  size_t			i;
  gpsee_context_private_t 	*hnd;

  JS_BeginRequest(cx);			/* enforces correct cx thread so we can do without locking */

  hnd = JS_GetContextPrivate(cx);
  if (hnd)
  {
    for (i=0; i < hnd->listSize; i++)
      JS_free(cx, hnd->list[i].storage);

    free(hnd->list);
    free(hnd);

    JS_SetContextPrivate(cx, NULL);
  }

  JS_EndRequest(cx);
  return JS_TRUE;
}

/** called by spidermonkey when contexts are created or destroyed */
JSBool gpsee_contextCallback(JSContext *cx, uintN contextOp)
{
  size_t			i;
  gpsee_context_private_t 	*hnd;

  hnd = JS_GetContextPrivate(cx);

  switch (contextOp)
  {
    case JSCONTEXT_NEW:
    break;

    case JSCONTEXT_DESTROY:
      if (hnd)
      {
	for (i=0; i < hnd->listSize; i++)
	{
	  if (hnd->list[i].cb)
	    hnd->list[i].cb(cx, JSCONTEXT_DESTROY);
	}
      }

      gpsee_destroyContextPrivate(cx);
    break;

    default:
      return JS_TRUE;
  }

  return JS_TRUE;
}

/** Each class may have its own context private storage.  This function returns
 *  a segment of memory that is unique across (cx,id). The first time a particular
 *  segment of memory is returned, it is initialized to all-zeroes.
 *
 *  If a JSContextCallback is passed to this function, it is called immediately with JSCONTEXT_NEW, and its return value
 *  is currently completely ignored. Your code may expect that returning JS_FALSE from a context callback will cancel
 *  the instantiation of the context. Be warned!

 *  @param	cx 		A JavaScript context. Must be in a request.
 *  @param	id		A unique non-zero number identifying the storage owner.
 *				Convention says	that id for module-context is a static variable set to MODULE_ID, 
 *				and that id for class-context is the static class pointer (clasp).
 *
 *  @param	size		The size of the memory to allocate for the storage. Note that storage
 *				will never be re-allocated (so get it right the first time) and that
 *				size of zero will cause us to not allocate any storage. It is permissible
 *				to pass size 0 when trying to retrieve, but not create, a context private
 *				area, however, this technique should probably not be widely use outside
 *				of a context callback function,
 *				
 *  @param	cb		Callback function, used basically the same way as 
 *				rt->cxCallback, except it's called for each (cx,id)
 *				pair the first time private storage is allocated, and
 *				when the context is destroyed.  Pass NULL for no callback.
 *
 *  @returns	The storage, or NULL when we are unable to JS_malloc().
 *
 *  @note	The allocated structure will have the same alignment as a void pointer.
 *
 *  @warning	Does not do thread-exclusion locking. HOWEVER, it should only be executed by a 
 *		context associated with the current thread. Provided this contract is not broken,
 *		it should be safe to run unlocked, since the data structures are unique across
 *		contexts. This caveat adds a subtle implication to embeddings using this facility:
 *		the thread destroying a context must also be thread that owns it, via JS_SetContextThread().
 *
 *  @warning	Passing NULL as id is a very bad idea: we reserve 0 as a special id which approximately
 *		simulates functionality formerly available as JS_SetContextCallback.
 *
 *  @see	JS_SetContextThread()
 *  @see	gpsee_destroyContextPrivate() 
 */
void *gpsee_getContextPrivate(JSContext *cx, const void *id, size_t size, JSContextCallback cb)
{
  size_t			i;
  void				*retval;
  gpsee_context_private_t 	*hnd;
  JSContextCallback             oldCallback;

  hnd = JS_GetContextPrivate(cx);
  if (!hnd)
  {
    if (!(hnd = JS_malloc(cx, sizeof(*hnd))))
    {
      JS_ReportOutOfMemory(cx);
      return NULL;
    }

    memset(hnd, 0, sizeof(*hnd));
    JS_SetContextPrivate(cx, hnd);
    oldCallback = JS_SetContextCallback(JS_GetRuntime(cx), gpsee_contextCallback);
  }

  for (i=0; i < hnd->listSize; i++)
  {
    if ((hnd->list[i].cx == cx) && (hnd->list[i].id == id))
    {
      retval = hnd->list[i].storage;	/* already exists */
      goto out;
    }
  }

  /* make a new one */
  i = hnd->listSize++;
  hnd->list = JS_realloc(cx, hnd->list, hnd->listSize * sizeof(hnd->list[0]));
  if (!hnd->list)
    panic("Out of memory in " __FILE__);
  
  if (id)
    hnd->list[i].cx	= cx;
  else
    hnd->list[i].cx	= (JSContext *)JS_GetRuntime(cx);	/* simulating JS_SetContextCallback */
  hnd->list[i].id	= id;
  hnd->list[i].cb	= cb;
  hnd->list[i].storage 	= size ? JS_malloc(cx, size) : NULL;

  if (hnd->list[i].storage)
    memset(hnd->list[i].storage, 0, size);

  retval = hnd->list[i].storage;
  if (cb)
    cb(cx, JSCONTEXT_NEW);

  if (id == NULL)	/* special case, we are simulating JS_SetContextCallback() */
    retval = cb;

  out:
  return retval;
}	

JSContextCallback gpsee_setContextCallback(JSContext *cx, JSContextCallback cb)
{
  return (JSContextCallback) gpsee_getContextPrivate(cx, NULL, 0, cb);
}
