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
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @file	gffi_module.h		Symbols shared between classes/objects in the gffi module.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @date	June 2009
 *  @version	$Id: gffi_module.h,v 1.3 2009/07/28 16:43:48 wes Exp $
 */

#include <dlfcn.h>

#define MODULE_ID GPSEE_GLOBAL_NAMESPACE_NAME ".module.ca.page.gffi"

JSObject *MutableStruct_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto);
JSObject *Memory_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto);
JSBool Memory_Constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSObject *CFunction_InitClass(JSContext *cx, JSObject *obj, JSObject *parentProto);
JSBool defines_InitObjects(JSContext *cx, JSObject *moduleObject);

/** Struct element types */
typedef enum { selt_integer, selt_array, selt_string, selt_pointer } sel_type_e;

/** Description of a struct member (field). */
typedef struct member_s
{
  const char	*name;		/**< Name of the field */
  sel_type_e	type;		/**< What type of field this is */ 
  int		isSigned;	/**< Whether or not the field is signed */
  size_t	typeSize; 	/**< size of field element; not redundant -- used to detect arrays */
  size_t	offset;		/**< How far away from the start of the struct it is */
  size_t	size;	  	/**< size of whole field: typeSize times number of elements */
} memberShape;

/** Description of a struct */
typedef struct struct_s
{
  const char		*name;		/**< Name of the struct, including 'struct' if it's not a typedef */
  size_t		size;		/**< Size of the struct */
  memberShape		*members;	/**< Description of each struct member */	
  size_t		memberCount;	/**< Number of members in the struct */
} structShape;

/** Private handle used to describe an instance of a memory object */
typedef struct
{
  size_t		length;		/**< Size of buffer or 0 if unknown */
  unsigned char 	*buffer;	/**< Pointer to memory */
  JSObject		*memoryOwner;	/**< Pointer to JSObject responsible for freeing memory */
} memory_handle_t;

/** Private handle used to describe an instance of a MutableStruct or ImmutableStruct object */
typedef struct
{
  size_t		length;		/**< Number of bytes allocated for the struct or 0 if we didn't allocate */
  unsigned char 	*buffer;	/**< Pointer to the start of the struct */
  JSObject		*memoryOwner;	/**< Pointer to JSObject responsible for freeing memory */
  structShape		*descriptor;	/**< Description of the struct this handle describes */
} struct_handle_t;

structShape *struct_findShape(const char *name);
int struct_findMemberIndex(structShape *shape, const char *name);

extern JSClass *mutableStruct_clasp;
extern JSClass *immutableStruct_clasp;
extern JSClass *cFunction_clasp;
extern JSClass *memory_clasp;
extern JSObject *memory_proto;
extern JSObject *mutableStruct_proto;

/** Magic numbers used in argument passing */
typedef enum
{
#define jsv(val)       inte_ ## val,
#include "jsv_constants.decl"
#undef jsv
} gffi_intv_e;
  
/** Magic jsvals used in argument passing */
typedef enum gffi_jsv
{
#define jsv(val)	jsve_ ## val = INT_TO_JSVAL(inte_ ## val),
#include "jsv_constants.decl"
#undef jsv
} gffi_jsv_e;

JSBool struct_getInteger(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_setInteger(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_getString(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_setString(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_getPointer(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_setPointer(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_getArray(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);
JSBool struct_setArray(JSContext *cx, JSObject *obj, int memberIdx, jsval *vp, const char *throwLabel);

GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, length) == offsetOf(memory_handle_t, length));
GPSEE_STATIC_ASSERT(offsetOf(byteThing_handle_t, buffer) == offsetOf(memory_handle_t, buffer));

GPSEE_STATIC_ASSERT(offsetOf(struct_handle_t, length) == offsetOf(memory_handle_t, length));
GPSEE_STATIC_ASSERT(offsetOf(struct_handle_t, buffer) == offsetOf(memory_handle_t, buffer));
