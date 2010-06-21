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
 *  @file	structs.c	Support code for GPSEE's gffi module which
 *				exposes C structures to the runtime so they 
 *				can be used by JavaScript. This is not strictly
 *				'FFI' as the structs must be pre-selected, 
 *				however it is the only reasonable and portable 
 *				way to reflect structs into an FFI without making
 *				the JS programmer responsible for knowing the
 *				struct layout on the platform in questino.
 *
 * @todo	handle expression macros by making them candiates for eval or something
 * @todo	handle function macros as though they were inline functions somehow
 *
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jun 2009
 *  @version	$Id: structs.c,v 1.8 2010/06/14 22:12:00 wes Exp $
 */

#include <gpsee.h>
#include "gffi.h"

/** Generic number getter. Uses memberIdx to figure out which member. */
JSBool struct_getInteger(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  jsdouble	 	num;
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);
  
  switch(hnd->descriptor->members[memberIdx].size)
  {
    case sizeof(int8):
    {
      if (hnd->descriptor->members[memberIdx].isSigned)
	*vp = INT_TO_JSVAL(hnd->buffer[hnd->descriptor->members[memberIdx].offset]);
      else
	*vp = INT_TO_JSVAL((uint32)hnd->buffer[hnd->descriptor->members[memberIdx].offset]);

      return JS_TRUE;
    }
    case sizeof(int16):
    {
      int16	i16;

      memcpy(&i16, hnd->buffer + hnd->descriptor->members[memberIdx].offset, sizeof(int16));

      if (hnd->descriptor->members[memberIdx].isSigned)
	*vp = INT_TO_JSVAL(i16);
      else
	*vp = INT_TO_JSVAL((uint32)i16);

      return JS_TRUE;
    }
    case sizeof(int32):
    {
      int32	i32;

      memcpy(&i32, hnd->buffer + hnd->descriptor->members[memberIdx].offset, sizeof(int32));
      if (INT_FITS_IN_JSVAL(i32))
      {
	if (hnd->descriptor->members[memberIdx].isSigned)
	  *vp = INT_TO_JSVAL(i32);
	else
	  *vp = INT_TO_JSVAL((uint32)i32);

	return JS_TRUE;
      }

      num = i32;
      break;
    }
    case sizeof(int64):
    {
      int64 i64;

      memcpy(&i64, hnd->buffer + hnd->descriptor->members[memberIdx].offset, sizeof(int64));

      if (i64 == (int32)i64 && INT_FITS_IN_JSVAL((int32)i64))
      {
	if (hnd->descriptor->members[memberIdx].isSigned)
	  *vp = INT_TO_JSVAL((int32)i64);
	else
	  *vp = INT_TO_JSVAL((uint32)i64);

	return JS_TRUE;
      }

      num = i64;
      if (num != i64)
	return gpsee_throw(cx, "%s.getInteger.range.%s: %s is a 64-bit value which does not fit in a JavaScript number!",
			   throwLabel, hnd->descriptor->members[memberIdx].name, hnd->descriptor->members[memberIdx].name);
      break;
    }
    default:
      return gpsee_throw(cx, "%s.getInteger.size.%s: Invalid number size for a struct member!",
			 throwLabel, hnd->descriptor->members[memberIdx].name);
  }

  return JS_NewNumberValue(cx, num, vp);
}

/** Generic number setter. Uses memberIdx to figure out which member. */
JSBool struct_setInteger(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  int64		 	num;
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);

  if (JSVAL_IS_INT(*vp))
  {
    if (hnd->descriptor->members[memberIdx].size == sizeof(int32))
    {
      int32 i32 = JSVAL_TO_INT(*vp);
      memcpy(hnd->buffer + hnd->descriptor->members[memberIdx].offset, &i32, sizeof(int32));
      return JS_TRUE;
    }

    num = JSVAL_TO_INT(*vp);
  }
  else
  {
    jsdouble d;
    
    if (JS_ValueToNumber(cx, *vp, &d) == JS_FALSE)
      return JS_FALSE;

    num = d;
    if (num != d)
      return gpsee_throw(cx, "%s.setInteger.%s.range.overflow", throwLabel, hnd->descriptor->members[memberIdx].name);
  }

  if (num < 0 && !hnd->descriptor->members[memberIdx].isSigned)
    return gpsee_throw(cx, "%s.setInteger.%s.range.underflow: Cannot store a negative number in an unsigned field",
		       throwLabel, hnd->descriptor->members[memberIdx].name);

  switch(hnd->descriptor->members[memberIdx].size | (hnd->descriptor->members[memberIdx].isSigned << 5))
  {
#define set(X, Y, Z) \
    case sizeof(int ## X) | (Y << 5): \
    { \
      Z ## X i ## X = num; \
      if (i ## X != num) \
	return gpsee_throw(cx, "%s.setInteger.%s.range.overflow", throwLabel, hnd->descriptor->members[memberIdx].name); \
      memcpy(hnd->buffer + hnd->descriptor->members[memberIdx].offset, &i ## X, sizeof(i ## X)); \
      break; \
    }

    set(8,  0, int);
    set(16, 0, int);
    set(32, 0, int);
    set(64, 0, int);
    set(8,  1, uint);
    set(16, 1, uint);
    set(32, 1, uint);
    set(64, 1, uint);
#undef set
    default:
      return gpsee_throw(cx, "%s.setInteger.%s.size.invalid: Invalid integer size", throwLabel, hnd->descriptor->members[memberIdx].name);
  }

  return JS_TRUE;
}

/** Generic string getter. Uses memberIdx to figure out which member. String here implies ASCIZ.
 *  Two string representations are possible with the structs.decl syntax -- either arrays of characters
 *  or pointers to strings. We determine treatment here based on the element size.
 */
JSBool struct_getString(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  JSString		*str;
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);

  if ((hnd->descriptor->members[memberIdx].typeSize) == 1) /* Character array */
    str = JS_NewStringCopyZ(cx, (char *)hnd->buffer + hnd->descriptor->members[memberIdx].offset);
  else	/* Pointer to char */
  {
    char *s;

    GPSEE_ASSERT((hnd->descriptor->members[memberIdx].typeSize) == sizeof(char *));

    s = *(char **)((char *)hnd->buffer + hnd->descriptor->members[memberIdx].offset);
    if (!s)
    {
      *vp = JSVAL_NULL;
      return JS_TRUE;
    }
	
    str = JS_NewStringCopyZ(cx, s);
  }

  if (!str)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(str);

  return JS_TRUE;
}

/** Generic string setter. Uses memberIdx to figure out which member. Character-array strings are overwritten
 *  with bounds-checking. Char-Pointer strings re-use the memory in the JSString and are managed by the GC.
 */
JSBool struct_setString(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);
  char			*str;
  JSString		*jsstr;

  if (JSVAL_IS_NULL(*vp))
  {
    str = NULL;
    jsstr = NULL;
  }
  else
  {
    jsstr = JSVAL_IS_STRING(*vp) ? JSVAL_TO_STRING(*vp) : JS_ValueToString(cx, *vp);
    str = JS_GetStringBytes(jsstr);
  }
  
  if ((hnd->descriptor->members[memberIdx].typeSize) == 1) /* Array of Chars */
  { 
    char *end;

    if (!str)
      return gpsee_throw(cx, "%s.setString.%s.null: Can't set character array to NULL", throwLabel, hnd->descriptor->members[memberIdx].name);

    end = gpsee_cpystrn((char *)hnd->buffer + hnd->descriptor->members[memberIdx].offset, str, hnd->descriptor->members[memberIdx].size);

    if ((end - (char *)(hnd->buffer + hnd->descriptor->members[memberIdx].offset)) == hnd->descriptor->members[memberIdx].size)
      if (strlen(str) >= hnd->descriptor->members[memberIdx].size)
	return gpsee_throw(cx, "%s.setString.%s.overflow", throwLabel, hnd->descriptor->members[memberIdx].name);
  }
  else /* Pointer to Chars */
  {
    if (jsstr && !JSVAL_IS_NULL(*vp) && !JSVAL_IS_STRING(*vp))
    {
      /* Root the string which was derived via .toString() */
      *vp = STRING_TO_JSVAL(jsstr);
    }

    *(char **)((char *)hnd->buffer + hnd->descriptor->members[memberIdx].offset) = str;		/* Has same GC lifetime as *vp */
  }

  return JS_TRUE;
}

/** Generic pointer getter. Uses memberIdx to figure out which member. Pointers in JavaScript are
 *  just hexadecimal numbers encoded with String.  Can't do numbers / valueof because
 *  there are 2^64 pointers in 64-bit land and only 2^53 integer doubles.
 */
JSBool struct_getPointer(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);

  return pointer_toString(cx, *(void **)(hnd->buffer + hnd->descriptor->members[memberIdx].offset), vp);
}

/** Generic pointer setter. Uses memberIdx to figure out which member. */
JSBool struct_setPointer(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel)
{
  struct_handle_t	*hnd = gpsee_getInstancePrivate(cx, obj, mutableStruct_clasp, immutableStruct_clasp);

  return pointer_fromString(cx, *vp, (void **)(hnd->buffer + hnd->descriptor->members[memberIdx].offset), throwLabel);
}

/** Generic C array getter, returns Memory objects to JS */
JSBool struct_getArray(JSContext *cx, JSObject *thisObj, int memberIdx, jsval *vp, const char *throwLabel)
{
  JSObject 		*robj;
  struct_handle_t	*structHnd = gpsee_getInstancePrivate(cx, thisObj, mutableStruct_clasp, immutableStruct_clasp);
  memory_handle_t	*memHnd;
  jsval 		argv[] = { JSVAL_TO_INT(0), JSVAL_FALSE };
  JSObject              *memory_proto;

  if (gpsee_getModuleData(cx, memory_clasp, (void **)&memory_proto, throwLabel) == JS_FALSE)
    return JS_FALSE;

  robj = JS_NewObject(cx, memory_clasp, memory_proto, thisObj);
  if (Memory_Constructor(cx, robj, sizeof(argv) / sizeof(argv[0]), argv, vp) == JS_FALSE)
    return JS_FALSE;

  memHnd = JS_GetPrivate(cx, robj);
  GPSEE_ASSERT(memHnd);
  if (!memHnd)
    return gpsee_throw(cx, "%s.getArray.impossible: impossible error processing returned Memory object", throwLabel);

  memHnd->buffer = structHnd->buffer + structHnd->descriptor->members[memberIdx].offset;
  memHnd->length = structHnd->descriptor->members[memberIdx].size;
  memHnd->memoryOwner = thisObj;

  return JS_TRUE;
}

/**
 *  Generic C array setter.
 *
 *  Set the contents of a struct field which happens to be an array (declared with [n]).
 *  If the passed object is a byteThing, we copy its contents (as much as will fit) into
 *  the struct field.
 *
 *  Otherwise, the passed jsval is taken as a String; if the array happens to be an array of 
 *  16-bit things, we do a 16-bit string copy. Otherwise we demote to 8 bit following the
 *  JS_CStringsAreUTF8() rules and to a byte-wise copy.
 */
JSBool struct_setArray(JSContext *cx, JSObject *thisObj, int memberIdx, jsval *vp, const char *throwLabel)
{
  JSString 		*str;
  struct_handle_t	*structHnd = gpsee_getInstancePrivate(cx, thisObj, mutableStruct_clasp, immutableStruct_clasp);
  void			*fieldAddr;
  size_t		fieldSize;

  fieldAddr = (char *)structHnd->buffer + structHnd->descriptor->members[memberIdx].offset;
  fieldSize = structHnd->descriptor->members[memberIdx].size;

  if (JSVAL_IS_STRING(*vp))
  {
    str = JSVAL_TO_STRING(*vp);
    goto stringPointer;
  }

  if (JSVAL_IS_OBJECT(*vp))
  {
    memory_handle_t	*memHnd;
    JSObject 		*obj = JSVAL_TO_OBJECT(*vp);

    if (gpsee_isByteThing(cx, obj))
    {
      memHnd = JS_GetPrivate(cx, obj);
      if (memHnd)
      {
	memcpy(fieldAddr, memHnd->buffer, min(fieldSize, memHnd->length));
	return JS_TRUE;
      }
    }
  }

  if (JSVAL_IS_VOID(*vp))
    return gpsee_throw(cx, "%s.invalid: cannot convert an undefined argument to an array", throwLabel);

  if (*vp == JSVAL_TRUE || *vp == JSVAL_FALSE)
    return gpsee_throw(cx, "%s.invalid: cannot convert a bool argument to an array", throwLabel);

  if (JSVAL_IS_NUMBER(*vp))
    return gpsee_throw(cx, "%s.invalid: cannot convert a numeric argument to an array", throwLabel);

  if (JSVAL_IS_NULL(*vp))
    return gpsee_throw(cx, "%s.invalid: cannot convert null to an array", throwLabel);

  str = JS_ValueToString(cx , *vp);
  if (!str)
    return JS_FALSE;
  *vp = STRING_TO_JSVAL(str);

  stringPointer:
  if (structHnd->descriptor->members[memberIdx].typeSize == 2)
  {
    /* A string is being passed to an array of int16 - assume we're looking
     * to copy the String data directly rather than converting to 8-bit
     */
    memcpy(fieldAddr, JS_GetStringChars(str), min(structHnd->length, JS_GetStringLength(str) * 2));
  }
  else
  {
    gpsee_cpystrn(fieldAddr, JS_GetStringBytes(str), fieldSize);
  }

  return JS_TRUE; 
}

/** Locate and return the named struct descriptor.
 *
 *  @param	name	The name of the struct, including the word 'struct' if it is
 *			not a typedef'd type.
 *  @returns	The struct descriptor upon success, NULL on failure.
 */
structShape *struct_findShape(const char *name)
{
#define beginStruct(a,b)		static struct member_s b ## _members[] = {
#define member(ctype, name, like)	  { #name,	selt_ ## like,	sizeof(ctype),	((ctype)-1) < 0,	member_offset(name),	member_size(name) },
/*					    name  	type		typeSize	isSigned	    	offset			size */	
#define endStruct(a)			};
#include "structs.incl"
#undef beginStruct
#undef member
#undef endStruct

  static structShape shapes[] = {
#define beginStruct(a,b)	{ #a, sizeof(a), b ##_members, sizeof(b ##_members) / sizeof(b ##_members[0]) },
#define member(a,b,c) /* */
#define endStruct(a) /* */
#include "structs.incl"
#undef beginStruct
#undef member
#undef endStruct
  };

  int i;

  for (i=0; i < sizeof(shapes) / sizeof(shapes[0]); i++)
  {
    if (strcmp(shapes[i].name, name) == 0)
      return &shapes[i];
  }

  return NULL;
}

/** Locate a member within a struct.
 *
 *  @param	descriptor	Struct descriptor to search for the member
 *  @param	name		Name of the member we're searching for, case-sensitive
 *
 *  @returns -1 on failure, member number otherwise. First member is #0.
 */
int struct_findMemberIndex(structShape *descriptor, const char *name)
{
  int i;

  for (i=0; i < descriptor->memberCount; i++)
  {
    if (strcmp(descriptor->members[i].name, name) == 0)
      return i;
  }

  return -1;
}
