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
 * Copyright (c) 2009, PageMail, Inc. All Rights Reserved.
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

/**
 *  @file	Memory.c		Implementation of the GPSEE gffi Memory class.
 *					This class provides a handle and allocator for 
 * 					heap; we also support void pointers by using
 *					0-byte heap segments at specific addresses.
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jul 2009
 *  @version	$Id: Memory.c,v 1.14 2010/12/02 21:59:43 wes Exp $
 */

#include <gpsee.h>
#include "gffi.h"
#include <math.h>

#define CLASS_ID MODULE_ID ".Memory"

/**
 *  Utility function to parse length arguments to memory objects.
 *  -1 means "believe strlen"
 */
static JSBool memory_parseLengthArgument(JSContext *cx, jsval v, char *buffer, size_t *lengthp, const char *throwPrefix)
{
  ssize_t	l;
  size_t	length;

  if (JSVAL_IS_INT(v))
    l = JSVAL_TO_INT(v);
  else
  {
    jsdouble d;
	
    if (JS_ValueToNumber(cx, v, &d) == JS_FALSE)
      return JS_FALSE;

    l = d;

    if (d != l)
      return gpsee_throw(cx, "%s.overflow", throwPrefix);
  }

  if (l == -1)
    length = strlen((char *)buffer);
  else
  {
    length = l;
    if (length != l)
      return gpsee_throw(cx, "%s.overflow", throwPrefix); 
  }

  *lengthp = length;

  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.asString -- a method to take a Memory object,
 *  treat it as a char * and be decode toString() following the regular JSAPI
 *  C Strings rules.
 *
 *  asString can take one argument: a length. If length is -1, use strlen;
 *  otherwise use at most length bytes. If length is not defined, we use either
 *     - buffer->length when !hnd->memoryOwner != this, or
 *     - strlen
 */
/* @jazzdoc gffi.Memory.asString()
 * This is an interface to JS_NewStringCopyN(). It turns a C character buffer into a JS String, obeying the
 * current rules for JS C Strings -- this will be either a naive uchar-uint16 cast or interpretation via UTF-8,
 * depending on the embedding.
 *
 * @form (instance of Memory).asString()
 * This form of the asString() instance method infers the length of memory to be consumed, and the Javascript String returned,
 * on its own. It will use strlen() to accomplish this if it does not already "know" its own length. 
 *
 * @form (instance of Memory).asString(-1)
 * Like the above form, but use of strlen() is forced.
 *
 * @form (instance of Memory).asString(length >= 0)
 * As the first form, but assuming the specified length.
 */
static JSBool memory_asString(JSContext *cx, uintN argc, jsval *vp)
{
  memory_handle_t	*hnd;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  JSString		*str;
  size_t		length;
  jsval			*argv = JS_ARGV(cx, vp);

  if (!obj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  if (!hnd->buffer)
  {
    *vp = JSVAL_NULL;
    return JS_TRUE;
  }

  switch(argc)
  {
    case 0:
      if (hnd->length || (hnd->memoryOwner == obj))
	length = hnd->length;
      else
	length = strlen((char *)hnd->buffer);
      break;
    case 1:
      if (memory_parseLengthArgument(cx, argv[0], hnd->buffer, &length, CLASS_ID ".asString.argument.1") != JS_TRUE)
	return JS_FALSE;
      break;
    default:
      return gpsee_throw(cx, CLASS_ID ".asString.arguments.count");
  }

  str = JS_NewStringCopyN(cx, (char *)hnd->buffer, length);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.asDataString -- a method to take a Memory object,
 *  treat it as a char * and be decoded toString() with a simple uchar->uint16 cast.
 *  This function will not change behaviour regardless of the local environment
 *  or spidermonkey UTF-8 settings.
 *
 *  asDataString can take one argument: a length. If length is -1, use strlen;
 *  otherwise use at most length bytes. If length is not defined, we use either
 *     - buffer->length when !hnd->memoryOwner != this, or
 *     - strlen
 */
/* @jazzdoc gffi.Memory.asDataString()
 * This is an interface turns a C character buffer into a JS String, ignoring the
 * current rules for JS C Strings -- this will be always be a naive uchar-uint16 cast.
 *
 * @form (instance of Memory).asDataString()
 * This form of the asDataString() instance method infers the length of memory to be consumed, and the Javascript String returned,
 * on its own. It will use strlen() to accomplish this if it does not already "know" its own length. 
 *
 * @form (instance of Memory).asDataString(-1)
 * Like the above form, but use of strlen() is forced.
 *
 * @form (instance of Memory).asDataString(length >= 0)
 * As the first form, but assuming the specified length.
 */
static JSBool memory_asDataString(JSContext *cx, uintN argc, jsval *vp)
{
  memory_handle_t	*hnd;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  JSString		*str;
  size_t		length, i;
  jsval			*argv = JS_ARGV(cx, vp);
  jschar		*buf;

  if (!obj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  if (!hnd->buffer)
  {
    *vp = JSVAL_NULL;
    return JS_TRUE;
  }

  switch(argc)
  {
    case 0:
      if (hnd->length || (hnd->memoryOwner == obj))
	length = hnd->length;
      else
	length = strlen((char *)hnd->buffer);
      break;
    case 1:
      if (memory_parseLengthArgument(cx, argv[0], hnd->buffer, &length, CLASS_ID ".asDataString.argument.1") != JS_TRUE)
	return JS_FALSE;
      break;
    default:
      return gpsee_throw(cx, CLASS_ID ".asDataString.arguments.count");
  }

  buf = JS_malloc(cx, length * 2);
  if (!buf)
    return JS_FALSE;

  for (i=0; i < length; i++)
    buf[i] = ((unsigned char *)hnd->buffer)[i];

  str = JS_NewUCString(cx, buf, length);
  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.copyDataString -- a method to take a String and
 *  and copy it into a Memory object's backing store a simple uint16->uchar cast.
 *  This function will not change behaviour regardless of the local environment
 *  or spidermonkey UTF-8 settings.
 *
 */
/* @jazzdoc gffi.Memory.copyDataString()
 * 
 * Turn a JS String into a C character buffer into a JS String, ignoring the
 * current rules for JS C Strings -- this will be always be a ive uint16-uchar cast.
 * Strings using characters > 255 will have those characters truncated to eight bits.
 *
 * @form (instance of Memory).copyDataString(instance of String)
 * This form of the copyDataString() instance method infers the length of Javascript String consumed
 * on its own. 
 *
 * @form (instance of Memory).asDataString(instance of String, length)
 * Like the above form, but the number of characters to consume is forced.
 */
static JSBool memory_copyDataString(JSContext *cx, uintN argc, jsval *vp)
{
  memory_handle_t	*hnd;
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);
  size_t		length, i;
  jsval			*argv = JS_ARGV(cx, vp);
  jschar		*buf;
  JSString		*str;

  if (!obj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  if (!hnd->buffer)
  {
    *vp = JSVAL_NULL;
    return JS_TRUE;
  }

  if (JSVAL_IS_STRING(argv[0]))
    str = JSVAL_TO_STRING(argv[0]);
  else
    return gpsee_throw(cx, CLASS_ID ".fromDataString.arguments.0.type: must be a string");

  switch(argc)
  {
    case 1:
      length = JS_GetStringLength(str);
      break;
    case 2:
      if (JSVAL_IS_INT(argv[1]))
	length = JSVAL_TO_INT(argv[1]);
      else
      {
	jsdouble d;

	if (JS_ValueToNumber(cx, argv[0], &d) != JS_TRUE)
	  return JS_FALSE;

	length = d;
	if (d != length)
	  return gpsee_throw(cx, CLASS_ID ".copyDataString.overflow");
      }

      break;
    default:
      return gpsee_throw(cx, CLASS_ID ".fromDataString.arguments.count");      
  }

  buf = JS_GetStringChars(str);
  
  for (i=0; i < min(length, JS_GetStringLength(str)); i++)
    hnd->buffer[i] = buf[i];
  
  return JS_TRUE;
}

/**
 *  Implements Memory.prototype.toString -- a method to take a Memory object,
 *  and return the address of the backing store encoded in a JavaScript String,
 *  so that JS programmers can examine pointers.
 */
static JSBool memory_toString(JSContext *cx, uintN argc, jsval *vp)
{
  memory_handle_t	*hnd;
  JSObject		*thisObj = JS_THIS_OBJECT(cx, vp);

  if (!thisObj)
    return JS_FALSE;

  hnd = JS_GetInstancePrivate(cx, thisObj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  return pointer_toString(cx, hnd->buffer, vp);
}

/** When ownMemory is true, we free the memory when the object is finalized */
static JSBool memory_ownMemory_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  memory_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  if (*vp != JSVAL_TRUE && *vp != JSVAL_FALSE)
  {
    JSBool b;

    if (JS_ValueToBoolean(cx, *vp, &b) == JSVAL_FALSE)
      return JS_FALSE;
    else
      *vp = (b == JS_TRUE) ? JSVAL_TRUE : JSVAL_FALSE;
  }

  if (*vp == JSVAL_TRUE)
    hnd->memoryOwner = obj;
  else
    hnd->memoryOwner = NULL;

  return JS_TRUE;
}

static JSBool memory_ownMemory_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  memory_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);

  if (!hnd)
    return JS_FALSE;

  if (hnd->memoryOwner == obj)
    *vp = JSVAL_TRUE;
  else
    *vp = JSVAL_FALSE;

  return JS_TRUE;
}

/** Size of memory region in use, not necessary how much is allocated.
 *  Size of zero often means "we don't know", not "it's empty"
 */
static JSBool memory_size_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  memory_handle_t	*hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  jsdouble		d;

  if (!hnd)
    return JS_FALSE;

  if (INT_FITS_IN_JSVAL(hnd->length))
  {
    *vp = INT_TO_JSVAL(hnd->length);
    return JS_TRUE;
  }

  d = hnd->length;
  if (hnd->length != d)
    return gpsee_throw(cx, CLASS_ID ".size.getter.overflow");

  return JS_NewNumberValue(cx, d, vp);
}

/**
 *  Calling realloc has the same hazards as calling it in C: if the buffer
 *  moves, all the objects which were casted to it are now invalid, and
 *  property accesses on them are liable to crash the embedding.
 *
 *  This function returns true when the underlying memory was moved, and
 *  false when it was not. It will not throw an exception if the memory
 *  was moved or invalidated, except possibly for OOM.
 */
static JSBool memory_realloc(JSContext *cx, uintN argc, jsval *vp)
{
  size_t		newLength;
  void			*newBuffer;
  memory_handle_t	*hnd;
  jsval			*argv = JS_ARGV(cx, vp);
  JSObject		*obj = JS_THIS_OBJECT(cx, vp);

  if (!obj)
    return JS_FALSE;
 
  hnd = JS_GetInstancePrivate(cx, obj, memory_clasp, NULL);
  if (!hnd)
    return JS_FALSE;

  if (hnd->memoryOwner != obj)
    return gpsee_throw(cx, CLASS_ID ".realloc.notOwnMemory: cannot realloc memory we do not own");

  if (JSVAL_IS_INT(argv[0]))
    newLength = JSVAL_TO_INT(argv[0]);
  else
  {
    jsdouble d;

    if (JS_ValueToNumber(cx, argv[0], &d) != JS_TRUE)
      return JS_FALSE;

    newLength = d;
    if (d != newLength)
      return gpsee_throw(cx, CLASS_ID ".realloc.overflow");
  }

  newBuffer = JS_realloc(cx, hnd->buffer, newLength);
  if (!newBuffer && newLength)
    return JS_FALSE;

  if (hnd->buffer == newBuffer)
    *vp = JSVAL_FALSE;
  else
  {
    hnd->buffer = newBuffer;
    *vp = JSVAL_TRUE;
  }

  hnd->length = newLength;

  return JS_TRUE;
}

/**
 *  Implements the memory::duplicate method. 
 *  This method copies, but does not interpret, the contents of the memory buffer.
 *  If the memory buffer is a C struct, the struct will be copied but not "deep copied".
 *  This is basically malloc + memcpy (i.e. memdup or strdup if memory is a C string).
 *
 *  An optional argument specifies the maximum length of the buffer to duplicate. 
 *  -1 means "use strlen".
 */
static JSBool memory_duplicate(JSContext *cx, uintN argc, jsval *vp)
{
  JSObject 		*robj;
  JSObject		*thisObj = JS_THIS_OBJECT(cx, vp);
  JSObject              *memory_proto;
  struct_handle_t	*thisHnd = thisObj ? gpsee_getInstancePrivate(cx, thisObj, mutableStruct_clasp, immutableStruct_clasp) : NULL;
  memory_handle_t	*newHnd;
  jsval 		mcArgv[] = { JSVAL_TO_INT(0), JSVAL_TRUE };
  size_t		length;

  if (!thisHnd)
    return JS_FALSE;

  switch(argc)
  {
    case 0:
      length = thisHnd->length;
      break;
    case 1:
      if (memory_parseLengthArgument(cx, JS_ARGV(cx, vp)[0], thisHnd->buffer, &length, CLASS_ID ".duplicate.argument.0") == JS_FALSE)
	return JS_FALSE;
      break;
    default:
      return gpsee_throw(cx, CLASS_ID ".arguments.count");
  }

  if (gpsee_getModuleData(cx, memory_clasp, (void **)&memory_proto, CLASS_ID ".duplicate") == JS_FALSE)
    return JS_FALSE;

  robj = JS_NewObject(cx, memory_clasp, memory_proto, thisObj);
  if (!robj)
    return JS_FALSE;

  if (Memory_Constructor(cx, robj, sizeof(mcArgv) / sizeof(mcArgv[0]), mcArgv, vp) == JS_FALSE)
    return JS_FALSE;

  newHnd = JS_GetPrivate(cx, robj);

  if (!length)
    return gpsee_throw(cx, CLASS_ID ".duplicate.length.unknown");

  newHnd->buffer = JS_malloc(cx, length);
  if (!newHnd->buffer)
    return JS_FALSE;

  newHnd->length = length;
  memcpy(newHnd->buffer, thisHnd->buffer, min(length, newHnd->length ?: length));
  newHnd->memoryOwner = thisObj;

  return JS_TRUE;
}

/**
 *  Implement a non-strict equality callback for instances of Memory. 
 *  This effectively overloads the == and != operators when the left-hand 
 *  side of the expression is a Memory() object.
 *
 *  @param	cx		JavaScript context
 *  @param	thisObj		The object on the left-hand side of the expression
 *  @param	v		The value of the right-hand side of the expression
 *  @param	bp		[out]	Pointer to JS_TRUE when equal, JS_FALSE otherwise
 *  @returns	JS_TRUE unless we've thrown an exception.
*/
JSBool Memory_Equal(JSContext *cx, JSObject *thisObj, jsval v, JSBool *bp)
{
  memory_handle_t *thisHnd = JS_GetPrivate(cx, thisObj);
  memory_handle_t *thatHnd;
  JSObject	  *thatObj;

  /* The first operand (this) is guaranteed to be a Memory object.
   * Check now to see if the second operand (that) is one of either
   * an object or null */
  if (!JSVAL_IS_OBJECT(v))
  {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  /* Check for NULL */
  if (v == JSVAL_NULL)
  {
    *bp = (thisHnd->buffer == NULL) ? JS_TRUE : JS_FALSE;
    return JS_TRUE;
  }

  thatObj = JSVAL_TO_OBJECT(v);
  if (JS_GET_CLASS(cx, thatObj) != memory_clasp)
  {
    *bp = JS_FALSE;
    return JS_TRUE;
  }

  thatHnd = JS_GetPrivate(cx, thatObj);
  *bp = (thisHnd && thatHnd && (thisHnd->buffer == thatHnd->buffer)) ? JS_TRUE : JS_FALSE;

  return JS_TRUE;
}
/**
 *  Implements Memory cast function.
 *  
 *  Casting a number X to Memory causes us to generate a Memory object for data
 *  at address X.
 *
 *  Note that this facility should only be used for "special" numbers, like -1 and 0,
 *  for comparison purposes and not some kind of crazy de-reference scheme or to 
 *  subvert the mutable/immutable casting rules.
 */
JSBool Memory_Cast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  memory_handle_t	*hnd;
  JSObject              *memory_proto;

  if (argc != 1)
    return gpsee_throw(cx, CLASS_ID ".cast.arguments.count");

  if (gpsee_getModuleData(cx, memory_clasp, (void **)&memory_proto, CLASS_ID ".cast") == JS_FALSE)
    return JS_FALSE;

  obj = JS_NewObject(cx, memory_clasp, memory_proto, NULL);
  if (!obj)
    return JS_FALSE;

  *rval = OBJECT_TO_JSVAL(obj);
 
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, obj, hnd);

  if (JSVAL_IS_OBJECT(argv[0]) && gpsee_isByteThing(cx, JSVAL_TO_OBJECT(argv[0])))
  {
    byteThing_handle_t *other_hnd = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));

    if (!other_hnd)
      return gpsee_throw(cx, MODULE_ID ".cast.arguments.0.invalid: invalid bytething (missing private handle)");

    hnd->buffer         = (void *)other_hnd->buffer;
    hnd->length         = other_hnd->length;
    hnd->memoryOwner    = JSVAL_TO_OBJECT(argv[0]);

    return JS_TRUE;
  }

  if (!JSVAL_IS_INT(argv[0]))
  {
    jsdouble 	d;

    if (JS_ValueToNumber(cx, argv[0], &d) != JS_TRUE)
      return JS_FALSE;

    if (isnan(d))
      return pointer_fromString(cx, argv[0], (void **)&hnd->buffer, CLASS_ID ".cast");

    GPSEE_STATIC_ASSERT(sizeof(size_t) == sizeof(void *));
    hnd->buffer = (void *)((size_t)d);
    if ((void *)((size_t)d) != hnd->buffer)
      return gpsee_throw(cx, CLASS_ID ".cast.overflow");
  }
  else
    hnd->buffer = (void *)(size_t)JSVAL_TO_INT(argv[0]);

  return JS_TRUE;
}

/** 
 *  Implements the Memory constructor.
 *
 *  Constructor takes exactly one or two arguments: 
 *   - number of bytes to allocate on heap.
 *   - whether the memory pointed to by this object is to
 *     be freed when the JS Object is finalzed.
 *
 *  This object may be used by C methods to keep handles on
 *  unknown amounts of memory. For this, simply construct the
 *  object with a 0 byte argument and set the pointer value
 *  by manipulating the data in the private slot.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated Memory object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
JSBool Memory_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  memory_handle_t	*hnd;

  if ((argc != 1) && (argc != 2))
    return gpsee_throw(cx, CLASS_ID ".arguments.count");

  *rval = OBJECT_TO_JSVAL(obj);
 
  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
    return JS_FALSE;

  /* cleanup now solely the job of the finalizer */
  memset(hnd, 0, sizeof(*hnd));
  JS_SetPrivate(cx, obj, hnd);

  if (JSVAL_IS_INT(argv[0]))
  {
    hnd->length = JSVAL_TO_INT(argv[0]);
  }
  else
  {
    jsdouble d;

    if (JS_ValueToNumber(cx, argv[0], &d) == JS_FALSE)
      return JS_FALSE;

    hnd->length = d;
    if (d != hnd->length)
      return gpsee_throw(cx, CLASS_ID ".constructor.size: %1.2g is not a valid memory size", d);
  }
  
  if (hnd->length)
  {
    hnd->buffer = JS_malloc(cx, hnd->length);
    if (!hnd->buffer)
      return JS_FALSE;

    hnd->memoryOwner = obj;
    memset(hnd->buffer, 0, hnd->length);
  }

  if (argc == 2) /* Allow JS programmer to override sanity */
  {
    if (argv[1] != JSVAL_TRUE && argv[1] != JSVAL_FALSE)
    {
      JSBool b;

      if (JS_ValueToBoolean(cx, argv[1], &b) == JSVAL_FALSE)
        return JS_FALSE;
      else
        argv[1] = (b == JS_TRUE) ? JSVAL_TRUE : JSVAL_FALSE;
    }

    if (argv[1] == JSVAL_TRUE)
      hnd->memoryOwner = obj;
    else
      hnd->memoryOwner = NULL;
  }

  return JS_TRUE;
}

/* @jazzdoc gffi.Memory
 * The GFFI module's Memory abstraction represents a pointer, analogous to the void* C type.
 *
 * @form new gffi.Memory(size:Number)
 * This is an interface to malloc(). The returned Memory instance is the "owner" of the memory allocated this way. See
 * gffi.Memory.prototype.ownMemory for more information on memory ownership in GFFI.
 *
 * @form gffi.Memory((instance of Memory-like object))
 * This is analogous to a cast operation and can be used on objects like gffi.MutableStruct, binary.ByteArray,
 * and binary.ByteString. The returned Memory object holds the same address as the one held by the argument given.
 */
JSBool Memory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (JS_IsConstructing(cx) != JS_TRUE)
    return Memory_Cast(cx, obj, argc, argv, rval);
  else
    return Memory_Constructor(cx, obj, argc, argv, rval);
}

/** 
 *  Memory Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 */
static void Memory_Finalize(JSContext *cx, JSObject *obj)
{
  memory_handle_t	*hnd = JS_GetPrivate(cx, obj);

  if (!hnd)
    return;

  if (hnd->buffer && (hnd->memoryOwner == obj))
    JS_free(cx, hnd->buffer);

  JS_free(cx, hnd);

  return;
}

JSClass *memory_clasp = NULL;

/**
 *  Initialize the Memory class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The module's exports object
 *
 *  @returns	Memory.prototype
 */
JSObject *Memory_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto)
{
  JSObject *memory_proto;

  /** Description of this class: */
  static JSExtendedClass memory_eclass =
  {
    {
      GPSEE_CLASS_NAME(Memory),		/**< its name is Memory */
      JSCLASS_HAS_PRIVATE | JSCLASS_IS_EXTENDED,	/**< private slot in use, this is really a JSExtendedClass */
      JS_PropertyStub, 			/**< addProperty stub */
      JS_PropertyStub, 			/**< deleteProperty stub */
      JS_PropertyStub,			/**< custom getProperty */
      JS_PropertyStub,			/**< setProperty stub */
      JS_EnumerateStub,			/**< enumerateProperty stub */
      JS_ResolveStub,  			/**< resolveProperty stub */
      JS_ConvertStub,  			/**< convertProperty stub */
      Memory_Finalize,			/**< it has a custom finalizer */
    
      JSCLASS_NO_OPTIONAL_MEMBERS
    },					/**< JSClass		base */
    Memory_Equal,			/**< JSEqualityOp 	equality */
    NULL,				/**< JSObjectOp		outerObject */
    NULL,				/**< JSObjectOp		innerObject */
    NULL,				/**< JSIteratorOp	iteratorObject */
    NULL,				/**< JSObjectOp		wrappedObject */
  };

  static JSPropertySpec memory_props[] =
  {
    { "size",		0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, memory_size_getter, JS_PropertyStub },
    { "ownMemory", 	0, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, memory_ownMemory_getter, memory_ownMemory_setter },
    { NULL, 0, 0, NULL, NULL }
  };

  static JSFunctionSpec memory_methods[] = 
  {
    JS_FN("toString",		memory_toString, 	0, JSPROP_ENUMERATE),
    JS_FN("asString",		memory_asString, 	0, JSPROP_ENUMERATE),
    JS_FN("asDataString",	memory_asDataString, 	0, JSPROP_ENUMERATE),
    JS_FN("copyDataString",	memory_copyDataString, 	0, JSPROP_ENUMERATE),
    JS_FN("realloc",		memory_realloc, 	0, JSPROP_ENUMERATE),
    JS_FN("duplicate",		memory_duplicate,	0, JSPROP_ENUMERATE),
    JS_FS_END
  };

  GPSEE_DECLARE_BYTETHING_EXTCLASS(memory);

  memory_clasp = &memory_eclass.base;

  memory_proto =
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   parentProto,		/* parent_proto - Prototype object for the class */
 		   memory_clasp, 	/* clasp - Class struct to init. Defs class for use by other API funs */
		   Memory,		/* constructor function - Scope matches obj */
		   1,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   memory_props,	/* ps - props struct for parent_proto */
		   memory_methods,	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  if (gpsee_setModuleData(cx, memory_clasp, memory_proto) == JS_FALSE)
    return NULL;

  GPSEE_ASSERT(memory_proto);

  return memory_proto;
}

