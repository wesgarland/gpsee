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
 * Copyright (c) 2007, PageMail, Inc. All Rights Reserved.
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
 *  @file	curses_Window.c 	A class for manipulating ncurses windows
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2008
 *  @version	$Id: curses_Window.c,v 1.1 2009/03/30 23:55:44 wes Exp $
 */

static const char __attribute__((unused)) rcsid[]="$Id: curses_Window.c,v 1.1 2009/03/30 23:55:44 wes Exp $";
#include "gpsee.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include "curses_module.h"

#define CLASS_ID  MODULE_ID ".window"

typedef struct
{
  WINDOW	*window;
} win_hnd_t;

static JSClass *WindowClass;

#define CKHND(func) do { \
  if (!hnd || !hnd->window) return gpsee_throw(cx, CLASS_ID "." #func ".invalidObject: Invalid Window object!"); \
  } while(0)

static JSBool getIntProp(JSContext *cx, JSObject *obj, const char *prop, int32 *propp,
			 const char *winThrowLabel /* cursorPositions.setter.parent */)
{
  jsval v;

  if ((JS_GetProperty(cx, obj, prop, &v) != JS_TRUE) || (v == JSVAL_VOID))
    return gpsee_throw(cx, CLASS_ID ".%s.%s.undefined", winThrowLabel, prop);

  if (JS_ValueToInt32(cx, v, propp) == JS_FALSE)
    return gpsee_throw(cx, CLASS_ID "%s.%s.NaN", winThrowLabel, prop);

  return JS_TRUE;
}

/** Implements curses.Window.prototype.scroll().
 *  Optional argument specifies number of lines to scroll. Negative numbers scroll backwards.
 */
static JSBool window_scroll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int32		scrollAmount = 1;
  jsrefcount	depth;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(scroll);

  if (argc)
    (void)JS_ValueToInt32(cx, argv[0], &scrollAmount);

  depth = JS_SuspendRequest(cx);
  *rval = wscrl(hnd->window, scrollAmount) == ERR ? JSVAL_FALSE : JSVAL_TRUE;
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** Setter for curses.Window.prototype.scrollRegion */
static JSBool window_scrollRegion_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  win_hnd_t	*hnd;
  JSObject	*vobj = JSVAL_TO_OBJECT(*vp);
  jsval		top, bottom;
  int32		itop, ibottom;
  jsrefcount	depth;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, NULL);
  CKHND(scrollRegion.__setter__);

  if (JS_GetProperty(cx, vobj, "top", &top) != JS_TRUE)
    return JS_FALSE;

  if (JS_GetProperty(cx, vobj, "bottom", &bottom) != JS_TRUE)
    return JS_FALSE;

  if (JS_ValueToInt32(cx, top, &itop) == JS_FALSE)
    top = JSVAL_VOID;

  if (JS_ValueToInt32(cx, bottom, &ibottom) == JS_FALSE)
    bottom = JSVAL_VOID;

  if (top == JSVAL_VOID)
  {
    if (JS_GetProperty(cx, obj, "top", &top) != JS_TRUE)
      return JS_FALSE;
    if (JS_ValueToInt32(cx, top, &itop) == JS_FALSE)
      return gpsee_throw(cx, CLASS_ID ".scrollRegion.__setter__.top.NaN");
  }

  if (bottom == JSVAL_VOID)
  {
    if (JS_GetProperty(cx, obj, "bottom", &bottom) != JS_TRUE)
      return JS_FALSE;
    if (JS_ValueToInt32(cx, bottom, &ibottom) == JS_FALSE)
      return gpsee_throw(cx, CLASS_ID ".scrollRegion.__setter__.bottom.NaN");
  }

  depth = JS_SuspendRequest(cx);
  wsetscrreg(hnd->window, itop, ibottom);
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

static JSBool window_cursorPosition_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int iX, iY;
  JSObject	*windowObject = JS_GetParent(cx, obj);
  win_hnd_t	*hnd;
  jsval		windowWidth;

  hnd = JS_GetInstancePrivate(cx, windowObject, WindowClass, NULL);
  CKHND(cursorPosition.xy.__getter__);
  
  getyx(hnd->window, iY, iX);

  if ((JS_GetProperty(cx, windowObject, "width", &windowWidth) != JS_TRUE) || (windowWidth == JSVAL_VOID))
    return gpsee_throw(cx, CLASS_ID ".cursorPosition.valueOf.parent.width");

  *rval = INT_TO_JSVAL((JSVAL_TO_INT(windowWidth) * iY) + iX);	/* Distance in chars from (0,0) */

  return JS_TRUE;
}

/** Getter for curses.Window.prototype.cursorPosition.x and curses.Window.prototype.cursorPosition.y
 */
static JSBool window_cursorPosition_xy_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int iX, iY;
  JSObject	*windowObject = JS_GetParent(cx, obj);
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, windowObject, WindowClass, NULL);
  CKHND(cursorPosition.xy.__getter__);
  
  getyx(hnd->window, iY, iX);

  switch(JSVAL_TO_INT(id) * -1)
  {
    case 'x':
      *vp = INT_TO_JSVAL(iX);
      break;
    case 'y':
      *vp = INT_TO_JSVAL(iY);
      break;
    default:
      GPSEE_NOT_REACHED("impossible");
      break;
  }

  return JS_TRUE;
}

/** Setter for curses.Window.prototype.cursorPosition.x and curses.Window.prototype.cursorPosition.y
 */
static JSBool window_cursorPosition_xy_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  int 		iX, iY, iYCopy, result;
  int32 	i;
  JSObject	*windowObject = JS_GetParent(cx, obj);
  jsrefcount	depth;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, windowObject, WindowClass, NULL);
  CKHND(cursorPosition.xy.__setter__);

  getyx(hnd->window, iY, iX);
  iYCopy = iY;

  if (JS_ValueToInt32(cx, *vp, &i) == JS_FALSE)
   return gpsee_throw(cx, CLASS_ID ".cursorPosition.xy.setter");

  switch(JSVAL_TO_INT(id) * -1)
  {
    case 'x':
      iX = i;
       break;
    case 'y':
      iY = i;
      break;
    default:
      GPSEE_NOT_REACHED("impossible");
      break;
  }

  depth = JS_SuspendRequest(cx);
  result = wmove(hnd->window, iY, iX);
  JS_ResumeRequest(cx, depth);

  if (result == ERR)	/* "movement fault": Simple move didn't work, let's do some math */
  {
    /* Implement cursor wrapping in the most pedestrian sense. If the application
     * programmer does not want the cursor to move beyond a certain point, he should
     * trap that himself.
     */

    int32	windowWidth, windowHeight;

    if (getIntProp(cx, windowObject, "width", &windowWidth, "cursorPosition.xy.setter.parent") == JS_FALSE)
      return JS_FALSE;

    if (getIntProp(cx, windowObject, "height", &windowHeight, "cursorPosition.xy.setter.parent") == JS_FALSE)
      return JS_FALSE;

    while (iX < 0)
    {
      iX += windowWidth;
      iY--;
    }

    while (iX >= windowWidth)
    {
      iX -= windowWidth;
      iY++;
    }

    if ((iY < 0) || (iY >= windowHeight))
    {
      jsval v = JSVAL_FALSE;

      if ((JS_GetProperty(cx, windowObject, "scrolling", &v) != JS_TRUE) || (v != JSVAL_TRUE))
      {
	while (iY < 0)
	  iY += windowHeight;

	if (iY >= windowHeight)
	  iY %= windowHeight;
      }
      else
      {
	jsval rv;

	v = INT_TO_JSVAL((iY - windowHeight) + 1);
	if (window_scroll(cx, windowObject, 1, &v, &rv) == JS_FALSE)
	  return JS_FALSE;

	iY = iYCopy;
      }
    }

    depth = JS_SuspendRequest(cx);
    (void)wmove(hnd->window, iY, iX);
    JS_ResumeRequest(cx, depth);
  }

  return JS_TRUE;
}

/** Getter for curses.Window.prototype.cursorPosition */
static JSBool window_cursorPosition_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  JSObject 	*point;
  jsrefcount	depth;
  int32		iX, iY;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, NULL);
  CKHND(cursorPosition.__getter__);

  /* Since modifying the passed object would result in calling the
   * setter (through JS_SetProperty), which in turn would move the
   * cursor, we just re-create the object every time the getter
   * is called.
   *
   * _IDEALLY_ we could just tweak the data in the shadow property
   * slot, but there doesn't appear to be an analogue to JS_GetProperty()'s
   * JS_LookupProperty() for JS_SetProperty().
   */

  point = JS_NewObject(cx, NULL, NULL, obj);
  *vp = OBJECT_TO_JSVAL(point);

  depth = JS_SuspendRequest(cx);
  getyx(hnd->window, iY, iX);
  JS_ResumeRequest(cx, depth);

  if (JS_DefinePropertyWithTinyId(cx, point, "x", -1 * 'x', INT_TO_JSVAL(iX),
				  window_cursorPosition_xy_getter, window_cursorPosition_xy_setter,
				  JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID "cursorPosition.getter.define.x");

  if (JS_DefinePropertyWithTinyId(cx, point, "y", -1 * 'y', INT_TO_JSVAL(iY),
				  window_cursorPosition_xy_getter, window_cursorPosition_xy_setter,
				  JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID "cursorPosition.getter.define.y");

  if (JS_DefineFunction(cx, point, "valueOf", window_cursorPosition_valueOf, 0, 0) == NULL)
    return gpsee_throw(cx, CLASS_ID "cursorPosition.getter.define.valueOf");

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.gotoXY()
 *  Called with two arguments: x and y
 *  Called with one argument: object with x and y properties.
 *  Movement is relative to the window's position
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
JSBool window_gotoXY(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  jsrefcount	depth;
  int32		iX, iY;
  jsval		x, y;
  win_hnd_t 	*hnd  = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(gotoXY);

  switch(argc)
  {
    case 1:
      if ((JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[0]), "x", &x) != JS_TRUE) || 
	  (JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[0]), "y", &y) != JS_TRUE))
	return gpsee_throw(cx, "gotoXY.window.coordinates.invalid");

      if ((y == JSVAL_VOID) || (x == JSVAL_VOID) || 
	  (JS_ValueToInt32(cx, y, &iY) == JS_FALSE) || 
	  (JS_ValueToInt32(cx, x, &iX) == JS_FALSE))
	return gpsee_throw(cx, "gotoXY.coordinates.NaN");
      break;

    case 2:
      if ((JS_ValueToInt32(cx, argv[0], &iX) == JS_FALSE) ||
	  (JS_ValueToInt32(cx, argv[1], &iY) == JS_FALSE))
	return gpsee_throw(cx, CLASS_ID ".gotoXY.coordinates.NaN");
      break;

    default:
      return gpsee_throw(cx, CLASS_ID ".gotoXY.arguments.count");
  }

  depth = JS_SuspendRequest(cx);
  *rval = (wmove(hnd->window, iY, iX) == ERR) ? JSVAL_FALSE : JSVAL_TRUE;
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** Setter for curses.Window.prototype.cursorPosition */
static JSBool window_cursorPosition_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  jsval v;

  if (JS_LookupProperty(cx, obj, "x", &v) != JS_TRUE)	/* Lookup != Get, does not run getter, just looks at the slot */
  {
    if (JS_DefinePropertyWithTinyId(cx, obj, "x", -1 * 'x', JSVAL_VOID, window_cursorPosition_xy_getter, window_cursorPosition_xy_setter,
				    JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
      return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.define.x");
  }

  if (JS_LookupProperty(cx, obj, "y", &v) != JS_TRUE)	/* Lookup != Get, does not run getter, just looks at the slot */
  {
    if (JS_DefinePropertyWithTinyId(cx, obj, "y", -1 * 'y', JSVAL_VOID, window_cursorPosition_xy_getter, window_cursorPosition_xy_setter,
				    JSPROP_ENUMERATE | JSPROP_PERMANENT) != JS_TRUE)
      return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.define.y");
  }

  if (JS_DefineFunction(cx, obj, "valueOf", window_cursorPosition_valueOf, 0, 0) == NULL)
    return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.define.valueOf");

  if (JSVAL_IS_INT(*vp))
  {
    v = INT_TO_JSVAL(0);
    JS_SetProperty(cx, obj, "y", &v);
    JS_SetProperty(cx, obj, "x", vp);
    return JS_TRUE;
  }

  return window_gotoXY(cx, obj, 1, vp, &v);

  if ((JS_GetProperty(cx, JSVAL_TO_OBJECT(*vp), "x", &v) != JS_TRUE) || (v == JSVAL_VOID))
    return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.x.undefined");
  else
    if (JS_SetProperty(cx, obj, "x", vp) != JS_TRUE)
      return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.x");
    
  if ((JS_GetProperty(cx, JSVAL_TO_OBJECT(*vp), "y", &v) != JS_TRUE) || (v == JSVAL_VOID))
    return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.y.undefined");
  else
    if (JS_SetProperty(cx, obj, "y", vp) != JS_TRUE)
      return gpsee_throw(cx, CLASS_ID "cursorPosition.setter.y");

  return JS_TRUE;
}

/** Setter for curses.Window.prototype.scrolling */
static JSBool window_scrolling_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, NULL);
  CKHND(scrolling.__setter__);

  if (scrollok(hnd->window, *vp == JSVAL_FALSE ? FALSE : TRUE) == ERR)
    *vp = JSVAL_FALSE;

  return JS_TRUE;
}

/** Implements curses.Window.prototype.drawHorizontalLine().
 *  Function takes one or two arguments; the length of the line, 
 *  and (optionally) the character to use to draw the line.
 *
 *  If no second argument is supplied, the nicest character available
 *  for the active terminal type will be chosen.
 */
static JSBool window_drawHorizontalLine(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int32		length;
  char 		lineChar = (char)0;
  jsrefcount	depth;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(drawHorizontalLine);

  if (!argc)
  {
    *rval = JSVAL_FALSE;
    return JSVAL_TRUE;
  }

  (void)JS_ValueToInt32(cx, argv[0], &length);

  if (argc > 1)
    lineChar = JS_GetStringBytes(JS_ValueToString(cx, argv[1]))[0];

  depth = JS_SuspendRequest(cx);
  *rval = whline(hnd->window, lineChar, length) == ERR ? JSVAL_FALSE : JSVAL_TRUE;
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** Implements curses.Window.prototype.drawVerticalLine().
 *  Function takes one or two arguments; the length of the 
 *  line, and (optionally) the character to use to draw the line.
 *
 *  If no second argument is supplied, the nicest character available
 *  for the active terminal type will be chosen.
 */
static JSBool window_drawVerticalLine(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int32		length;
  char 		lineChar = (char)0;
  jsrefcount	depth;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(drawVerticalLine);

  if (!argc)
  {
    *rval = JSVAL_FALSE;
    return JSVAL_TRUE;
  }

  (void)JS_ValueToInt32(cx, argv[0], &length);

  if (argc > 1)
    lineChar = JS_GetStringBytes(JS_ValueToString(cx, argv[1]))[0];

  depth = JS_SuspendRequest(cx);
  *rval = wvline(hnd->window, lineChar, length) == ERR ? JSVAL_FALSE : JSVAL_TRUE;
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** Implements curses.Window.prototype.drawBorder().
 *  Function takes one optional argument, a string eight characters long.
 *  If specified, the characters in the string correspond to the characters
 *  used to draw the border, in the order left-to-right, top-to-bottom,
 *  sides first, then corners.  So "||--++++" might draw something useful.
 *
 *  The default behaviour is to use the nicest characters available for 
 *  the active terminal type.
 *
 *  In the unusual case that the JS programmer wants to only override SOME
 *  of the border characters from their defaults, a NUL character (0) in 
 *  the string will instruct curses to use the default character for that
 *  position. This behaviour may not be portable.
 */
static JSBool window_drawBorder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  char borderChars[8];
  jsrefcount	depth;
  win_hnd_t	*hnd;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(drawBorder);

  memset(borderChars, 0, sizeof(borderChars));
  if (argc)
  {
    /* JS_GetStringBytes() not used because we want to allow NUL in string */

    JSString *str = JS_ValueToString(cx, argv[0]);
    jschar *chars = str ? JS_GetStringChars(str) : NULL;
    size_t numChars = str ? JS_GetStringLength(str) : 0;

    if (numChars)
    {
      int i;

      for (i = 0; i < min(8, numChars); i++)
	borderChars[i] = chars[i] & ~0xff ? 0 : chars[i];	/* only useful unicode conversion? */
    }
  }

  depth = JS_SuspendRequest(cx);
  *rval = wborder(hnd->window, borderChars[0], borderChars[1], borderChars[2], borderChars[3],
		  borderChars[4], borderChars[5], borderChars[6], borderChars[7]) == OK ? JSVAL_TRUE : JSVAL_FALSE;
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** Implements curses.Window.prototype.getKey().
 *  Optional argument specifies timeout in ms.
 */
static JSBool window_getKey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int		ch;
  jsrefcount	depth;
  win_hnd_t	*hnd;
  int32		timeo = 0;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(getKey);

  if (argc && (JS_ValueToInt32(cx, argv[0], &timeo) == JS_FALSE))
    return gpsee_throw(cx, CLASS_ID ".getKey.timeout.NaN");

  depth = JS_SuspendRequest(cx);
  waddstr(hnd->window, "");

  if (timeo)
    wtimeout(hnd->window, timeo);
  ch = wgetch(hnd->window);
  JS_ResumeRequest(cx, depth);

  if (ch == ERR)
    *rval = JSVAL_NULL;
  else
  {
    const char	*name = keyname(ch);

    if ((name[0] == 'K') && 
	(name[1] == 'E') && 
	(name[2] == 'Y') && 
	(name[3] == '_'))
      name += 4;

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, name));
  }

  return JS_TRUE;
}

/** Implements curses.Window.prototype.getKeys() */
static JSBool window_getKeys(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int		result;
  jsrefcount	depth;
  win_hnd_t	*hnd;
  char		linebuf[256];

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(getKeys);

  depth = JS_SuspendRequest(cx);
  echo();
  waddstr(hnd->window, "");	/* Get the cursor in place */
  result = wgetnstr(hnd->window, linebuf, sizeof(linebuf));
  noecho();
  JS_ResumeRequest(cx, depth);  
  
  if (result == ERR)
    *rval = JSVAL_FALSE;
  else
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, linebuf));

  return JS_TRUE;
}

/** 
 *  Implements the Window constructor.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	Pre-allocated Window object
 *  @param	argc	Number of arguments passed to constructor
 *  @param	argv	Arguments passed to constructor
 *  @param	rval	The new object returned to JavaScript
 *
 *  @returns 	JS_TRUE on success
 */
static JSBool Window(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  win_hnd_t	*hnd;

  /* Window() called as function. */   
  if (JS_IsConstructing(cx) != JS_TRUE)
    return gpsee_throw(cx, CLASS_ID ".constructor.notFunction: Cannot call constructor as a function!");

  if (argc && argc != 5)
    return gpsee_throw(cx, CLASS_ID ".constructor.arguments.count");

  /* JS passed us a pre-made object to initialize. 
   * Make that object the constructor's return value.
   */
  *rval = OBJECT_TO_JSVAL(obj);

  hnd = JS_malloc(cx, sizeof(*hnd));
  if (!hnd)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  memset(hnd, 0, sizeof(*hnd));

  if (argc)
  {
    JSObject	*parentObj = JSVAL_TO_OBJECT(argv[0]);
    win_hnd_t	*parentHnd = parentObj ? JS_GetInstancePrivate(cx, parentObj, WindowClass, argv) : NULL;
    int		x,y;
    int		width, height;
    jsval	parentY, parentX;

    if (!parentHnd || 
	(JS_GetProperty(cx, parentObj, "y", &parentY) != JS_TRUE) ||
	(JS_GetProperty(cx, parentObj, "x", &parentX) != JS_TRUE))
      return gpsee_throw(cx, "constructor.arguments.0.typeof");

    if ((JS_ValueToInt32(cx, argv[1], &x) == JS_FALSE) ||
	(JS_ValueToInt32(cx, argv[2], &y) == JS_FALSE))
      return gpsee_throw(cx, CLASS_ID ".constructor.XY.coordinates.NaN");

    if ((JS_ValueToInt32(cx, argv[3], &width) == JS_FALSE) ||
	(JS_ValueToInt32(cx, argv[4], &height) == JS_FALSE))
      return gpsee_throw(cx, CLASS_ID ".constructor.dimensions.NaN");

    x += JSVAL_TO_INT(parentX);
    y += JSVAL_TO_INT(parentY);

    hnd->window = subwin(parentHnd->window, height, width, y, x);

    if (!hnd->window)
      return gpsee_throw(cx, CLASS_ID ".constructor.subwin: Unable to create sub window! (x,y,h,w)=(%i,%i,%i,%i)",
			 x, y, height, width);

    JS_SetParent(cx, obj, JSVAL_TO_OBJECT(argv[0]));
    JS_DefineProperty(cx, obj, "fullscreen", JSVAL_FALSE, JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );

    JS_DefineProperty(cx, obj, "height", INT_TO_JSVAL(height), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineProperty(cx, obj, "width", INT_TO_JSVAL(width), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineProperty(cx, obj, "y", INT_TO_JSVAL(y), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT);
    JS_DefineProperty(cx, obj, "x", INT_TO_JSVAL(x), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT);
  }
  else
  {
    /* Full-screen window */
    int iX, iY;

    hnd->window = newwin(0,0,0,0);
    JS_DefineProperty(cx, obj, "fullscreen", JSVAL_TRUE, JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );

    getmaxyx(hnd->window, iY, iX);
    JS_DefineProperty(cx, obj, "height", INT_TO_JSVAL(iY), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );
    JS_DefineProperty(cx, obj, "width", INT_TO_JSVAL(iX), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );
    JS_DefineProperty(cx, obj, "y", INT_TO_JSVAL(0), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );
    JS_DefineProperty(cx, obj, "x", INT_TO_JSVAL(0), JS_PropertyStub, JS_PropertyStub,
		      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY );
  }

  /* Use the private member to maintain separate windows */
  JS_SetPrivate(cx, obj, hnd);

  /* Supply default values when "undefined" is not acceptable and there is no prop getter */
  {
    jsval dummy = JSVAL_TRUE;
    JS_SetProperty(cx, obj, "scrolling", &dummy);
  }
  scrollok(hnd->window, TRUE);

  meta(hnd->window, TRUE);
  keypad(hnd->window, TRUE);
  wrefresh(hnd->window);
  immedok(hnd->window, TRUE);

  return JS_TRUE;
}  

/** 
 *  Window Finalizer.
 *
 *  Called by the garbage collector, this routine release all resources
 *  used by an instance of the class when the object is collected.
 *
 *  @note  The finalizer will also be called on the class prototype
 *         during shutdown.
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object to finalize
 */
static void Window_Finalize(JSContext *cx, JSObject *obj)
{
  win_hnd_t *hnd = JS_GetPrivate(cx, obj);

  if (hnd)
  {
    delwin(hnd->window);
    free(hnd);
  }

  return;
}

/** 
 *  Implements curses.Window.prototype.refresh()
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
JSBool window_refresh(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  win_hnd_t 	*hnd;
  jsrefcount	depth;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(refresh);

  depth = JS_SuspendRequest(cx);
  *rval = (touchwin(hnd->window) == ERR) ? JSVAL_FALSE : JSVAL_TRUE;
  redrawwin(hnd->window);
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.clear()
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
JSBool window_clear(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  win_hnd_t 	*hnd;
  jsrefcount	depth;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(refresh);

  depth = JS_SuspendRequest(cx);
  *rval = (touchwin(hnd->window) == ERR) ? JSVAL_FALSE : JSVAL_TRUE;
  wclear(hnd->window);
  JS_ResumeRequest(cx, depth);

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.getXY().valueOf()
 */
static JSBool window_getXY_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  JSObject	*windowObject = JS_GetParent(cx, obj);
  win_hnd_t	*hnd;
  jsval		windowWidth;
  jsval		x, y;
  int32		iX, iY;

  hnd = JS_GetInstancePrivate(cx, windowObject, WindowClass, NULL);
  CKHND(getXY.valueOf);
  
  if ((JS_GetProperty(cx, obj, "x", &x) != JS_TRUE) || 
      (JS_GetProperty(cx, obj, "y", &y) != JS_TRUE))
    return gpsee_throw(cx, "getXY.coordinates.invalid");

  if ((y == JSVAL_VOID) || (x == JSVAL_VOID) || 
      (JS_ValueToInt32(cx, y, &iY) == JS_FALSE) || 
      (JS_ValueToInt32(cx, x, &iX) == JS_FALSE))
    return gpsee_throw(cx, "getXY.coordinates.NaN");

  if ((JS_GetProperty(cx, windowObject, "width", &windowWidth) != JS_TRUE) || (windowWidth == JSVAL_VOID))
    return gpsee_throw(cx, CLASS_ID ".getXY().valueOf.parent.width");

  *rval = INT_TO_JSVAL((JSVAL_TO_INT(windowWidth) * iY) + iX);	/* Distance in chars from (0,0) */

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.getXY()
 */
JSBool window_getXY(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int 		x,y;
  JSObject	*point;
  win_hnd_t 	*hnd  = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(getXY);

  point = JS_NewObject(cx, NULL, NULL, obj);

  if (!point)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(point);

  getyx(hnd->window, y, x);

  if ((JS_DefineProperty(cx, point, "x", INT_TO_JSVAL(x), JS_PropertyStub, JS_PropertyStub, 
			 JSPROP_ENUMERATE | JSPROP_PERMANENT) == JS_FALSE) ||
      (JS_DefineProperty(cx, point, "y", INT_TO_JSVAL(y), JS_PropertyStub, JS_PropertyStub, 
			 JSPROP_ENUMERATE | JSPROP_PERMANENT) == JS_FALSE))
  {
    return gpsee_throw(cx, CLASS_ID ".getXY.defineProps"); /* impossible */
  }

  if (JS_DefineFunction(cx, point, "valueOf", window_getXY_valueOf, 0, 0) == NULL)
    return gpsee_throw(cx, CLASS_ID "getXY.valueOf");

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.getMaxXY()
 */
JSBool window_getMaxXY(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int 		x,y;
  int32		windowWidth, windowHeight;
  JSObject	*point;
  win_hnd_t 	*hnd  = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(getXY);

  point = JS_NewObject(cx, NULL, NULL, obj);
  if (!point)
  {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  *rval = OBJECT_TO_JSVAL(point);
  
  if (getIntProp(cx, obj, "width", &windowWidth, "getMaxXY.parent.width") == JS_FALSE)
    return JS_FALSE;

  if (getIntProp(cx, obj, "height", &windowHeight, "getMaxXY.parent.height") == JS_FALSE)
    return JS_FALSE;

  if ((JS_DefineProperty(cx, point, "x", INT_TO_JSVAL(x), JS_PropertyStub, JS_PropertyStub, 
			 JSPROP_ENUMERATE | JSPROP_PERMANENT) == JS_FALSE) ||
      (JS_DefineProperty(cx, point, "y", INT_TO_JSVAL(y), JS_PropertyStub, JS_PropertyStub, 
			 JSPROP_ENUMERATE | JSPROP_PERMANENT) == JS_FALSE))
  {
    return gpsee_throw(cx, CLASS_ID ".getXY.defineProps"); /* impossible */
  }

  if (JS_DefineFunction(cx, point, "valueOf", window_getXY_valueOf, 0, 0) == NULL)
    return gpsee_throw(cx, CLASS_ID "getXY.valueOf");

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.write()
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
JSBool window_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  win_hnd_t 	*hnd;
  jsrefcount	depth;
  uintN		i;
  char		*s;

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(write);

  for (i = 0; i < argc; i++)
  {
    s = JS_GetStringBytes(JS_ValueToString(cx, argv[i]));

    depth = JS_SuspendRequest(cx);
    waddstr(hnd->window, s);
    JS_ResumeRequest(cx, depth);
  }

  return JS_TRUE;
}

/** 
 *  Implements curses.Window.prototype.writeXY()
 *
 *  @param	cx	JavaScript context
 *  @param	obj	The object on which the method is being called
 *  @param	argc	The number of arguments passed to the method
 *  @param	argv	The arguments passed to the method
 *  @param	rval	The return value from the method
 *
 *  @returns	JS_TRUE	On success
 */
JSBool window_writeXY(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  win_hnd_t 	*hnd;
  JSBool	b;

  if (argc < 2)
    return gpsee_throw(cx, CLASS_ID ".writeXY: Insufficient arguments");

  hnd = JS_GetInstancePrivate(cx, obj, WindowClass, argv);
  CKHND(writeXY);

  if ((argc > 2) && ((JSVAL_IS_INT(argv[0]) == JS_TRUE) && (JSVAL_IS_INT(argv[1]) == JS_TRUE)))
  {
    b = window_gotoXY(cx, obj, 2, argv, rval);
    argc -= 2;
    argv += 2;
  }
  else
  {
    b = window_gotoXY(cx, obj, 1, argv, rval);
    argc -= 1;
    argv += 1;
  }

  if (b == JS_FALSE)
    return JS_FALSE;

  if (*rval != JSVAL_TRUE)
    return JS_TRUE;

  return window_write(cx, obj, argc, argv, rval);
}

/**
 *  Initialize the Window class prototype.
 *
 *  @param	cx	Valid JS Context
 *  @param	obj	The Global Object
 *
 *  @returns	Window.prototype
 */
JSObject *Window_InitClass(JSContext *cx, JSObject *obj)
{
  /** Description of this class: */
  static JSClass window_class = 
  {
    MODULE_ID ".Window",		/**< its name is Window */
    JSCLASS_HAS_PRIVATE,		/**< it uses JS_SetPrivate() */
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    Window_Finalize,			/**< it has a custom finalizer */
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  /** Methods of Window.prototype */
  static JSFunctionSpec win_methods[] = 
  {
    { "drawHorizontalLine",	window_drawHorizontalLine,	0, 0, 0 },
    { "drawVerticalLine",	window_drawVerticalLine,	0, 0, 0 },
    { "drawBorder",		window_drawBorder,		0, 0, 0 },
    { "getKey",			window_getKey,			0, 0, 0 },
    { "getKeys",		window_getKeys,			0, 0, 0 },
    { "scroll",			window_scroll,			0, 0, 0 },
    { "write",			window_write,			0, 0, 0 },
    { "writeXY",		window_writeXY,			0, 0, 0 },
    { "gotoXY",			window_gotoXY,			0, 0, 0 },
    { "getXY",			window_getXY,			0, 0, 0 },
    { "refresh",		window_refresh,			0, 0, 0 },
    { "clear",			window_clear,			0, 0, 0 },
    { NULL,			NULL,				0, 0, 0 }
  };

  /** Properties of Window.prototype */
  static JSPropertySpec win_props[] = 
  {
    { "cursorPosition",	0, JSPROP_ENUMERATE | JSPROP_PERMANENT, window_cursorPosition_getter, window_cursorPosition_setter },
    { "scrollRegion", 	0, JSPROP_ENUMERATE | JSPROP_PERMANENT, JS_PropertyStub, window_scrollRegion_setter },
    { "scrolling", 	0, JSPROP_ENUMERATE | JSPROP_PERMANENT, JS_PropertyStub, window_scrolling_setter },
    { "name", 		0, JSPROP_ENUMERATE | JSPROP_PERMANENT, JS_PropertyStub, JS_PropertyStub },
    { NULL, 0, 0, NULL, NULL }
  };

  JSObject *proto = 
      JS_InitClass(cx, 			/* JS context from which to derive runtime information */
		   obj, 		/* Object to use for initializing class (constructor arg?) */
		   NULL, 		/* parent_proto - Prototype object for the class */
 		   &window_class,	/* clasp - Class struct to init. Defs class for use by other API funs */
		   Window,		/* constructor function - Scope matches obj */
		   0,			/* nargs - Number of arguments for constructor (can be MAXARGS) */
		   win_props,		/* ps - props struct for parent_proto */
		   win_methods, 	/* fs - functions struct for parent_proto (normal "this" methods) */
		   NULL,		/* static_ps - props struct for constructor */
		   NULL); 		/* static_fs - funcs struct for constructor (methods like Math.Abs()) */

  GPSEE_ASSERT(proto);

  WindowClass = &window_class;
  return proto;
}
