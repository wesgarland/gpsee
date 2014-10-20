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
 * Copyright (c) 2010, PageMail, Inc. All Rights Reserved.
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
 *  @file       gpsee_bytethings.c
 *  @brief      Support code for generic GPSEE ByteThings. Generic ByteThings
 *              give us GPSEE-cast native type interoperability at the memory
 *              block level. Generic ByteThings may not be instanciated from Script.
 *
 *  @note       GPSEE ByteThings should not be confused with the bytethings in the
 *              Binary/B module, which are a formally unrelated, confusingly named
 *              conceptual ancestor that still happen to be used in the implementation
 *              for that module.
 *
 *  @ingroup    bytethings
 *  @author     Wes Garland, <wes@page.ca>
 *  @date       June 2010
 */

#include "gpsee.h"

#ifdef GPSEE_DEBUG_BUILD
# define dprintf(a...) do { if (gpsee_verbosity(0) > 2) gpsee_printf(cx, "> "), gpsee_printf(cx, a); } while(0)
#else
# define dprintf(a...) do { ; } while(0)
#endif

/** 
 *  All byteThings must have this JSTraceOp. It is used for two things:
 *  1. It identifies byteThings to other byteThings
 *  2. It ensures that hnd->memoryOwner is not finalized while other byteThings 
 *     directly using the same backing memory store are reacheable.
 *
 *  @param trc	Tracer handle supplied by JSAPI.
 *  @param obj	The object behind traced.
 */
void gpsee_byteThingTracer(JSTracer *trc, JSObject *obj)
{
  byteThing_handle_t	*hnd = JS_GetPrivate(trc->context, obj);

  if (hnd && hnd->memoryOwner && (hnd->memoryOwner != obj))
  {
    JSContext *cx __attribute__((unused)) = trc->context;
    dprintf("Marking bytething at %p owned by %p for %p\n", hnd->buffer, hnd->memoryOwner, obj);
    JS_CallTracer(trc, hnd->memoryOwner, JSTRACE_OBJECT);
  }
}

/** 
 *  ByteThing Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 */
static void ByteThing_Finalize(JSContext *cx, JSObject *obj)
{
  byteThing_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

/**
 *  Instanciate a generic GPSEE ByteThing.  Generic GPSEE ByteThings have a single reserved
 *  slot, which may be used by the instanciator as a GC root.
 *
 *  @param      cx      The current JavaScript context.
 *  @param      buffer  The bytes represented by the ByteThing
 *  @param      length  The number of bytes in the buffer. 
 *  @param      copy    Whether or not to copy the buffer. If set to JS_TRUE, the buffer is copied.
 *                      If set to JS_FALSE, it is the caller's responsibility to insure that the 
 *                      buffer's lifetime is at least as great as that of the returned JSObject.
 *  @returns    The object, or NULL if an exception was thrown.
 *
 *  @note       It is an error to request a copy of a zero-byte buffer.
 *  @note       Requesting a copy of a NULL buffer will cause the same behaviour as a regular copy,
 *              except the new memory will be left uninitialized.
 */
#warning Rooting Error in gpsee_newByteThing API
JSObject *gpsee_newByteThing(JSContext *cx, void *buffer, size_t length, JSBool copy)
{
  static JSClass byteThing_class =
  {
    GPSEE_GLOBAL_NAMESPACE_NAME ".ByteThing",	/**< its name is ByteThing */
    JSCLASS_HAS_PRIVATE|	        /**< private slot in use */
    JSCLASS_HAS_RESERVED_SLOTS(1),      /**< Has a reserved slot */
    JS_PropertyStub, 			/**< addProperty stub */
    JS_PropertyStub, 			/**< deleteProperty stub */
    JS_PropertyStub,			/**< custom getProperty */
    JS_PropertyStub,			/**< setProperty stub */
    JS_EnumerateStub,			/**< enumerateProperty stub */
    JS_ResolveStub,  			/**< resolveProperty stub */
    JS_ConvertStub,  			/**< convertProperty stub */
    ByteThing_Finalize,		        /**< it has a custom finalizer */
    
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  JSObject              *robj;
  byteThing_handle_t    *hnd;

  GPSEE_DECLARE_BYTETHING_CLASS(byteThing);

  if (copy && (length == 0))
  {
    (void)gpsee_throw(cx, GPSEE_GLOBAL_NAMESPACE_NAME ".byteThing.new: Cannot create a copy of a 0-byte GPSEE ByteThing");
    return NULL;
  }

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    goto err_out;
  memset(hnd, 0, sizeof(*hnd));

  robj = JS_NewObject(cx, &byteThing_class, NULL, NULL);
  if (!robj)
    goto err_out;

  hnd->length = length;
  if (copy)
  {
    hnd->memoryOwner = robj;
    hnd->buffer = JS_malloc(cx, length);
    if (buffer)
      memcpy(hnd->buffer, buffer, length);
  }
  else
  {
    hnd->buffer = buffer;
    hnd->memoryOwner = NULL;
    hnd->btFlags = bt_immutable;
  }

  JS_SetPrivate(cx, robj, hnd);
  return robj;

  err_out:
  if (hnd && hnd->buffer && copy)
    JS_free(cx, hnd->buffer);

  if (hnd)
    JS_free(cx, hnd);

  return NULL;
}
