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
 *  @file	defines.c	Support code for GPSEE's gffi module which
 *				exposes C preprocessor macro constants
 *				to the runtime so they can be used by 
 *				JavaScript.
 *
 * @todo	handle expression macros by making them candiates for eval or something
 * @todo	handle function macros as though they were inline functions somehow
 *
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jun 2009
 *  @version	$Id: defines.c,v 1.2 2009/07/28 16:43:48 wes Exp $
 */

#include <gpsee.h>
#include "gffi_module.h"

typedef enum { cdeft_signedInt, cdeft_unsignedInt, cdeft_floatingPoint, cdeft_string } type_e;
typedef union 
{
  uint64	unsignedInt;
  int64		signedInt;
  const char	*string;
  jsdouble	floatingPoint;
} value_u;

struct define
{ 
  const char	*name;
  size_t	size;
  type_e	type;
  value_u	value;
};

typedef struct define define_s;

#define startDefines(a)		static struct define a ## _defines[]={
#define	haveInt(a,b,c,d)	{ #a, d, c ? cdeft_signedInt : cdeft_unsignedInt, { .unsignedInt = b } },
#define haveFloat(a,b,c)	{ #a, c, cdeft_floatingPoint, { .floatingPoint = b } },
#define haveString(a,b)		{ #a, sizeof(b), cdeft_string, { .string = b } },
#define endDefines(a)		};
#define haveDefs(a, ...) 	/**/
#define haveDef(a)		/**/
#include "defines.incl"
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveDefs
#undef haveDef
#undef endDefines

struct 
{  
  const char 		*name;
  struct define		*defines;
  size_t 		length;
} cdefGroupList[] = {
#define startDefines(a)		/**/
#define haveInt(a, ...) 	/**/
#define haveFloat(a, ...) 	/**/
#define haveString(a, ...) 	/**/
#define haveDefs(a, ...) 	/**/
#define endDefines(a)		/**/
#define haveDef(a)		{ #a, a ## _defines, sizeof(a ## _defines) / sizeof(a ## _defines[0]) },
#include "defines.incl"
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveDefs
#undef haveDef
#undef endDefines
};

/** Find the number of rows in a defines list based on a pointer to said list.
 *  @param	cdefGroup	A pointer to the list
 *  @returns	The number of rows in the list, or zero if not found.
 */
static size_t cdefGroupLength(const define_s *cdefGroup)
{
  int i;

  i = sizeof(cdefGroupList) / sizeof(cdefGroupList[0]);
  while(i--)
  {
    if (cdefGroupList[i].defines == cdefGroup)
      return cdefGroupList[i].length;
  }

  return 0;
}

static int strcmpDefineName(const void *vKey, const void *vDefine)
{
  const char 		*key = vKey;
  const define_s 	*define = vDefine;

  return strcmp(key, define->name);
}

/** Find a value in an array of defs structs. 
 *  Relies on the array being sorted in stcmp order by name. Upon successful
 *  return, type_p and value_p are populated.
 *
 *  @param	defs	Which array to search
 *  @param	name	Which constant to search for (case-sensitive)
 *  @param	type_p	[out]	What data type the constant represents
 *  @param	value_p	[out]	The value of the constant
 *  @returns	JS_TRUE if the constant was found, JS_FALSE otherwise.
 */
JSBool getDefine(const define_s *cdefGroup, const char *name, type_e *type_p, value_u *value_p, size_t *size_p)
{
  struct define *found;

  found = (struct define *)bsearch(name, cdefGroup, cdefGroupLength(cdefGroup), sizeof(cdefGroup[0]), strcmpDefineName);

  if (!found)
    return JS_FALSE;

  *type_p  = found->type;
  *value_p = found->value;
  *size_p  = found->size;

  return JS_TRUE;
}

JSBool getDefineValue(JSContext *cx, const define_s *cdefGroup, const char *name, jsval *vp)
{
  type_e	type;
  value_u	value;
  size_t	size;

  if (getDefine(cdefGroup, name, &type, &value, &size) == JS_FALSE)
  {
    *vp = JSVAL_VOID;
    return JS_TRUE;
  }

  switch(type)
  {
    case cdeft_signedInt:
      if ((jsint)value.signedInt == value.signedInt && INT_FITS_IN_JSVAL(value.signedInt))
      {
	*vp = INT_TO_JSVAL((jsint)value.signedInt);
	return JS_TRUE;
      }
      return JS_NewNumberValue(cx, (jsdouble)value.signedInt, vp);

    case cdeft_unsignedInt:
      if ((jsint)value.unsignedInt == value.unsignedInt && INT_FITS_IN_JSVAL(value.unsignedInt))
      {
	*vp = INT_TO_JSVAL((jsint)value.unsignedInt);
	return JS_TRUE;
      }
      return JS_NewNumberValue(cx, (jsdouble)value.unsignedInt, vp);

    case cdeft_floatingPoint:
      return JS_NewNumberValue(cx, (jsdouble)value.floatingPoint, vp);

    case cdeft_string:
    {
      /* xxx which is better for this? -wg JSString *s = JS_NewStringCopyN(cx, value.string, size); */
      JSString *s;

      if (!value.string)
      {
	*vp = JSVAL_NULL;
	return JS_TRUE;
      }

      if ((s = JS_InternString(cx, value.string)))
	return STRING_TO_JSVAL(s);

      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }
  }

  GPSEE_NOT_REACHED("impossible");
  return JS_TRUE;
}

const define_s *cdefGroup(const char *name)
{
  int i;

  i = sizeof(cdefGroupList) / sizeof(cdefGroupList[0]);
  while(i--)
  {
    if (strcmp(cdefGroupList[i].name, name) == 0)
      return cdefGroupList[i].defines;
  }

  return NULL;
}

JSBool cdefGroup_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
  const define_s *group = JS_GetPrivate(cx, obj);
  JSString *s;

  *objp = NULL;

  if (JSVAL_IS_STRING(id))
    s = JSVAL_TO_STRING(id);
  else
    s = JS_ValueToString(cx, id);

  if (s)
  {
    const char *name = JS_GetStringBytes(s);

    if (group && name)
    {
      jsval v;

      if ((getDefineValue(cx, group, name, &v) == JS_TRUE) && (v != JSVAL_VOID))
      {
	JS_DefineProperty(cx, obj, name, v, NULL, NULL, 0);
	*objp = obj;
      }
    }
  }

  return JS_TRUE;
}

/** Class definition for non-JS-instanciable class cdefGroup */
static JSClass cdefGroup_class = 
{
  GPSEE_CLASS_NAME(cdefGroup),		/**< its name is cdefGroup */
  JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE, /**< private slot in use, new resolve op callback */
  JS_PropertyStub,  			/**< addProperty stub */
  JS_PropertyStub,  			/**< deleteProperty stub */
  JS_PropertyStub,			/**< getProperty stub */
  JS_PropertyStub,			/**< setProperty stub */
  JS_EnumerateStub, 			/**< enumerateProperty stub */
  (JSResolveOp)cdefGroup_resolve, 	/**< resolveProperty stub */
  JS_ConvertStub,   			/**< convertProperty stub */
  JS_FinalizeStub,			/**< finalize stub */

  JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool defines_InitObjects(JSContext *cx, JSObject *moduleObject)
{
  JSObject *obj;

#define startDefines(a) \
  if (!(obj = JS_NewObject(cx, &cdefGroup_class, NULL, moduleObject))) return JS_FALSE; JS_SetPrivate(cx, obj, cdefGroup(#a));
#define endDefines(a) \
  if (JS_TRUE != JS_DefineProperty(cx, moduleObject, #a, OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE)) return JS_FALSE;

#define	haveInt(a,b,c,d)	/**/
#define haveFloat(a,b,c)	/**/
#define haveString(a,b)		/**/
#define haveDefs(a, ...) 	/**/
#define haveDef(a)		/**/
#include "defines.incl"
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveDefs
#undef haveDef
#undef endDefines

  return JS_TRUE;
}

#ifdef TEST_DRIVER
int main(int argc, const char *argv[])
{
  const char	*name = argv[1];
  type_e	type;
  value_u	value;
  size_t	size;

  if (getDefine(posix_defines, name, &type, &value, &size) == JS_TRUE)
  {
    switch(type)
    {
      case cdeft_signedInt:
	printf("The value for %s is %i, it is a signed integer\n", name, (signed int)value.signedInt);
	break;
      case cdeft_unsignedInt:
	printf("The value for %s is %i, it is an unsigned integer\n", name, (unsigned int)value.unsignedInt);
	break;
      case cdeft_floatingPoint:
	printf("The value for %s is %f, it is a floating point number\n", name, (double)value.floatingPoint);
	break;
      case cdeft_string:
	printf("The value for %s is %s, it is a string\n", name, value.string);
	break;
    }
  }
  else
    printf("Couldn't find a value for %s in posix defines\n", name);

  return 0;
}
#endif
