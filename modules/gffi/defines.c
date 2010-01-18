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
 *  @version	$Id: defines.c,v 1.3 2010/01/18 22:07:02 wes Exp $
 */

#include <gpsee.h>
#include "gffi_module.h"
#include <math.h>

#if !defined(GPSEE_MAX_TRANSITIVE_MACRO_RESOLUTION_DEPTH)
# define GPSEE_MAX_TRANSITIVE_MACRO_RESOLUTION_DEPTH 100
#endif

#if !defined(nan)
# if defined(NAN)
#  define nan NAN
# else
#  define nan __builtin_nanf("")
# endif
#endif

typedef enum { cdeft_signedInt, cdeft_unsignedInt, cdeft_floatingPoint, cdeft_string, cdeft_alias, cdeft_expr } type_e;
typedef union 
{
  uint64	unsignedInt;
  int64		signedInt;
  const char	*string;
  jsdouble	floatingPoint;
} value_u;

struct define
{ 
  const char	*name;		/**< Name of the define */
  size_t	size;		/**< Size of the value */ 
  type_e	type;		/**< Type of the value */
  value_u	value;		/**< The value (union) */
};

typedef struct define define_s;

/* Declare a series of arrays containing defines.
 * Each array is from a different header group.  Different
 * header groups allow not only conflicting symbols, but 
 * conflicting standards.  For example, one header group might
 * use "platform-default" I/O whereas another one might force
 * long files -- which has affects all over the API, in particular,
 * on constants.
 */
#define startDefines(a)		static struct define a ## _defines[]={
#define	haveInt(a,b,c,d)	{ #a, d, c ? cdeft_signedInt : cdeft_unsignedInt, { .unsignedInt = b } },
#define haveFloat(a,b,c)	{ #a, c, cdeft_floatingPoint, { .floatingPoint = b } },
#define haveString(a,b)		{ #a, sizeof(b), cdeft_string, { .string = b } },
#define endDefines(a)		};
#define haveExpr(a,b)		{ #a, 0, cdeft_expr,  { .string = #b } },
#define haveAlias(a,b)		{ #a, 0, cdeft_alias, { .string = #b } },
#define haveArgMacro(a,b,c)	/**/
#define haveDefs(a, ...) 	/**/
#define haveDef(a)		/**/
#include "defines.incl"
#undef haveExpr
#undef haveArgMacro
#undef haveAlias
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveDefs
#undef haveDef
#undef endDefines

/* Groups of defines, declared above, indexed by name */
typedef struct 
{  	
  const char 		*name;		/**< Name of define group */
  struct define		*defines;	/**< Array of defines */
  size_t 		length;		/**< Size of the defines array */
} defineGroup_s;

defineGroup_s cdefGroupList[] = {
#define startDefines(a)		/**/
#define haveInt(a, ...) 	/**/
#define haveFloat(a, ...) 	/**/
#define haveString(a, ...) 	/**/
#define haveExpr(a,b)		/**/
#define haveAlias(a,b)		/**/
#define haveArgMacro(a,b,c)	/**/
#define haveDefs(a, ...) 	/**/
#define endDefines(a)		/**/
#define haveDef(a)		{ #a, a ## _defines, sizeof(a ## _defines) / sizeof(a ## _defines[0]) },
#include "defines.incl"
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveArgMacro
#undef haveExpr
#undef haveAlias
#undef haveDefs
#undef haveDef
#undef endDefines
};

/** Compare the name of a define against a string.  Used as 
 *  bsearch comparison function.
 *
 *  @param	vKey	String containing name of define we want
 *  @param	vDefine	Define structure we want to check
 *  @returns	-1 when smaller, 0 when same, 1 when bigger
 */
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
 *  @param	defs		Which array to search
 *  @param	name		Which constant to search for (case-sensitive)
 *  @param	transDepth	Current transitive macro resolution depth (to prevent recursion loops)
 *  @returns	A struct define pointer describing the constant if it was found, NULL otherwise.
 */
static const struct define *getDefine(const defineGroup_s *cdefGroup, const char *name, size_t transDepth)
{
  struct define *found;

  found = (struct define *)bsearch(name, cdefGroup->defines, cdefGroup->length, sizeof(cdefGroup->defines[0]), strcmpDefineName);

  if (found && found->type == cdeft_alias)
  {
    if (transDepth >= GPSEE_MAX_TRANSITIVE_MACRO_RESOLUTION_DEPTH)
      return NULL;

    return getDefine(
	cdefGroup, 
	(const char *)(found->value.string), 
	++transDepth);
  }

  return found;
}

/** Get the value of a define, coerced into an appropriate JavaScript type.
 *
 *  @param	cx		JSAPI Context
 *  @param	cdefGroup	Which c #define group to search
 *  @param	cdefGroupObject	JavaScript object representing the define group
 *  @param	define		Name of the value
 *  @param	vp [out]	Value pointer
 *
 *  @returns	JS_TRUE unless we throw
 */
static JSBool getDefineValue(JSContext *cx, const defineGroup_s *cdefGroup, JSObject *cdefGroupObj, const char *name, jsval *vp)
{
  const struct define *def = getDefine(cdefGroup, name, 0);

  if (def == NULL)
  {
    *vp = JSVAL_VOID;
    return JS_TRUE;
  }

  switch(def->type)
  {
    case cdeft_signedInt:
      if ((jsint)def->value.signedInt == def->value.signedInt && INT_FITS_IN_JSVAL(def->value.signedInt))
      {
	*vp = INT_TO_JSVAL((jsint)def->value.signedInt);
	return JS_TRUE;
      }
      return JS_NewNumberValue(cx, (jsdouble)def->value.signedInt, vp);

    case cdeft_unsignedInt:
      if ((jsint)def->value.unsignedInt == def->value.unsignedInt && INT_FITS_IN_JSVAL(def->value.unsignedInt))
      {
	*vp = INT_TO_JSVAL((jsint)def->value.unsignedInt);
	return JS_TRUE;
      }
      return JS_NewNumberValue(cx, (jsdouble)def->value.unsignedInt, vp);

    case cdeft_floatingPoint:
      return JS_NewNumberValue(cx, (jsdouble)def->value.floatingPoint, vp);

    case cdeft_string:
    {
      JSString *s;

      if (!def->value.string)
      {
	*vp = JSVAL_NULL;
	return JS_TRUE;
      }

      if ((s = JS_InternString(cx, def->value.string)))
	return STRING_TO_JSVAL(s);

      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }

    case cdeft_expr:
    {
      static char 	buf[1024];
      int		i;

      i = snprintf(buf, sizeof(buf),
		   "(function _gffi_def_expr_%s() { with(%s) { return (%s); } })()",
		   name, cdefGroup->name, def->value.string);

      if (i <= 0 || i == sizeof(buf))
	return gpsee_throw(cx, MODULE_ID "defines.%s.%s: cannot evaluate define (buffer size too small)",
			   cdefGroup->name, name);

      return JS_EvaluateScript(cx, cdefGroupObj, buf, i, __FILE__, __LINE__, vp);
    }

    case cdeft_alias:
      GPSEE_NOT_REACHED("impossible");
  }

  GPSEE_NOT_REACHED("impossible");
  return JS_TRUE;
}

/** Locate a specific group of defines
 *  @param	name	Name of the group 
 *  @returns	The group, if found, or NULL
 */
static const defineGroup_s *find_cdefGroup(const char *name)
{
  int i;

  i = sizeof(cdefGroupList) / sizeof(cdefGroupList[0]);
  while(i--)
  {
    if (strcmp(cdefGroupList[i].name, name) == 0)
      return cdefGroupList + i;
  }

  return NULL;
}

/** JSAPI resolve operator which operates on instances of define groups. Lazily resolves
 *  specific defines within the groups.
 */
static JSBool cdefGroup_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
  const defineGroup_s *group = JS_GetPrivate(cx, obj);
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

      if ((getDefineValue(cx, group, obj, name, &v) == JS_TRUE) && (v != JSVAL_VOID))
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
  if (!(obj = JS_NewObject(cx, &cdefGroup_class, NULL, moduleObject))) return JS_FALSE; JS_SetPrivate(cx, obj, find_cdefGroup(#a));
#define endDefines(a) \
  if (JS_TRUE != JS_DefineProperty(cx, moduleObject, #a, OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE)) return JS_FALSE;

#define	haveInt(a,b,c,d)	/**/
#define haveFloat(a,b,c)	/**/
#define haveString(a,b)		/**/
#define haveExpr(a,b)		/**/
#define haveAlias(a,b)		/**/
#define haveArgMacro(a,b,c)	/**/
#define haveDefs(a, ...) 	/**/
#define haveDef(a)		/**/
#include "defines.incl"
#undef startDefines
#undef haveInt
#undef haveFloat
#undef haveString
#undef haveExpr
#undef haveAlias
#undef haveArgMacro
#undef haveDefs
#undef haveDef
#undef endDefines

  return JS_TRUE;
}

