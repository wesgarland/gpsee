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
 * ***** END LICENSE BLOCK ***** 
 */

/**
 *  @file	curses_keyboard.c	GPSEE object for manipulating terminal keyboard via ncurses
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @version	$Id: curses_keyboard.c,v 1.1 2009/03/30 23:55:44 wes Exp $
 *  @date	March 2009
 */

static __attribute__((unused)) const char rcsid[]="$Id: curses_keyboard.c,v 1.1 2009/03/30 23:55:44 wes Exp $";
 
#include "gpsee.h"
#include <curses.h>
#include "curses_module.h"

#define OBJECT_ID MODULE_ID ".keyboard"

JSString *keyCode_toString(JSContext *cx, int ch)
{
  char		keyLabelBuf[4];
  const char	*keyLabel;

  switch(ch)
  {
    case ERR:
      return NULL;

    case KEY_BREAK:		keyLabel="break";		  	break;
    case KEY_DOWN:		keyLabel="down arrow";		  	break;
    case KEY_UP:		keyLabel="up arrow";		  	break;
    case KEY_LEFT:		keyLabel="left arrow";		  	break;
    case KEY_RIGHT:		keyLabel="right arrow";		  	break;
    case KEY_HOME:		keyLabel="home ";		  	break;
    case KEY_BACKSPACE:		keyLabel="backspace";		  	break;
    case KEY_DL:		keyLabel="delete line";		  	break;
    case KEY_IL:		keyLabel="insert line";		  	break;
    case KEY_DC:		keyLabel="delete character";	  	break;	
    case KEY_IC:		keyLabel="ins char/enter mode";	  	break;
    case KEY_EIC:		keyLabel="rmir or smir in ins mode";	break;
    case KEY_CLEAR:		keyLabel="clear screen or erase";	break;
    case KEY_EOS:		keyLabel="clear-to-end-of-screen";	break;
    case KEY_EOL:		keyLabel="clear-to-end-of-line";	break;
    case KEY_SF:		keyLabel="scroll-forward/down";		break;
    case KEY_SR:		keyLabel="scroll-backward/up";		break;
    case KEY_NPAGE:		keyLabel="next-page";			break;
    case KEY_PPAGE:		keyLabel="previous-page";		break;	
    case KEY_STAB:		keyLabel="set-tab";			break;
    case KEY_CTAB:		keyLabel="clear-tab";			break;
    case KEY_CATAB:		keyLabel="clear-all-tabs";		break;
    case KEY_ENTER:		keyLabel="enter";			break;
    case KEY_SRESET:		keyLabel="soft reset";			break;
    case KEY_RESET:		keyLabel="reset";			break;
    case KEY_PRINT:		keyLabel="print";			break;
    case KEY_LL:		keyLabel="home-down";			break;
    case KEY_A1:		keyLabel="keypad upper left";		break;
    case KEY_A3:		keyLabel="keypad upper right";		break;
    case KEY_B2:		keyLabel="keypad center";		break;
    case KEY_C1:		keyLabel="keypad lower left";		break;
    case KEY_C3:		keyLabel="keypad lower right";		break;
    case KEY_BTAB:		keyLabel="Back tab";			break;
    case KEY_BEG:		keyLabel="beginning";			break;
    case KEY_CANCEL:		keyLabel="cancel";			break;
    case KEY_CLOSE:		keyLabel="close";			break;
    case KEY_COMMAND:		keyLabel="command";			break;
    case KEY_COPY:		keyLabel="copy";			break;
    case KEY_CREATE:		keyLabel="create";			break;
    case KEY_END:		keyLabel="end";				break;
    case KEY_EXIT:		keyLabel="exit";			break;
    case KEY_FIND:		keyLabel="find";			break;
    case KEY_HELP:		keyLabel="help";			break;
    case KEY_MARK:		keyLabel="mark";			break;
    case KEY_MESSAGE:		keyLabel="message";			break;
    case KEY_MOVE:		keyLabel="move";			break;
    case KEY_NEXT:		keyLabel="next";			break;
    case KEY_OPEN:		keyLabel="open";			break;
    case KEY_OPTIONS:		keyLabel="options";			break;
    case KEY_PREVIOUS:		keyLabel="previous";			break;
    case KEY_REDO:		keyLabel="redo";			break;
    case KEY_REFERENCE:		keyLabel="reference";			break;
    case KEY_REFRESH:		keyLabel="refresh";			break;
    case KEY_REPLACE:		keyLabel="replace";			break;
    case KEY_RESTART:		keyLabel="restart";			break;
    case KEY_RESUME:		keyLabel="resume";			break;
    case KEY_SAVE:		keyLabel="save";			break;
    case KEY_SBEG:		keyLabel="shift+beginning";		break;
    case KEY_SCANCEL:		keyLabel="shift+cancel";		break;
    case KEY_SCOMMAND:		keyLabel="shift+command";		break;
    case KEY_SCOPY:		keyLabel="shift+copy";			break;
    case KEY_SCREATE:		keyLabel="shift+create";		break;
    case KEY_SDC:		keyLabel="shift+delete char";		break;
    case KEY_SDL:		keyLabel="shift+delete line";		break;
    case KEY_SELECT:		keyLabel="select";			break;
    case KEY_SEND:		keyLabel="shift+end";			break;
    case KEY_SEOL:		keyLabel="shift+clear line";		break;
    case KEY_SEXIT:		keyLabel="shift+exit";			break;
    case KEY_SFIND:		keyLabel="shift+find";			break;
    case KEY_SHELP:		keyLabel="shift+help";			break;
    case KEY_SHOME:		keyLabel="shift+home";			break;
    case KEY_SIC:		keyLabel="shift+input";			break;
    case KEY_SLEFT:		keyLabel="shift+left arrow";		break;
    case KEY_SMESSAGE:		keyLabel="shift+message";		break;
    case KEY_SMOVE:		keyLabel="shift+move";			break;
    case KEY_SNEXT:		keyLabel="shift+next";			break;
    case KEY_SOPTIONS:		keyLabel="shift+options";		break;
    case KEY_SPREVIOUS:		keyLabel="shift+prev";			break;
    case KEY_SPRINT:		keyLabel="shift+print";			break;
    case KEY_SREDO:		keyLabel="shift+redo";			break;
    case KEY_SREPLACE:		keyLabel="shift+replace";		break;
    case KEY_SRIGHT:		keyLabel="shift+right arrow";		break;
    case KEY_SRSUME:		keyLabel="shift+resume";		break;
    case KEY_SSAVE:		keyLabel="shift+save";			break;
    case KEY_SSUSPEND:		keyLabel="shift+suspend";		break;
    case KEY_SUNDO:		keyLabel="shift+undo";			break;
    case KEY_SUSPEND:		keyLabel="suspend";			break;
    case KEY_UNDO:		keyLabel="undo";			break;
    case KEY_MOUSE:		keyLabel="Mouse event has occured";	break;

    case KEY_F(0) ... KEY_F(63):	
    {
      sprintf(keyLabelBuf, "F%i", ((ch - KEY_F0) & 63) + 1); /* & 63 is a buffer-overrun safety net */
      keyLabel = keyLabelBuf;
      break;
    }

    default:
    {
      keyLabelBuf[0] = (char)(ch & 0xff);
      keyLabelBuf[1] = (char)0;
      keyLabel = keyLabelBuf;
      break;
    }
  }

  return JS_NewStringCopyZ(cx, keyLabel);
}




static JSBool keyboard_getChar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  int		ch;
  jsrefcount	depth;
  JSString	*s;

  depth = JS_SuspendRequest(cx);
  ch = getch();
  JS_ResumeRequest(cx, depth);

  if ((s = keyCode_toString(cx, ch)))
    *rval = STRING_TO_JSVAL(s);
  else
    *rval = JSVAL_NULL;

  return JS_TRUE;
}

JSObject *keyboard_InitObject(JSContext *cx, JSObject *obj)
{
  JSObject	*keyboard;

  static JSClass keyboard_class = 
  {
    MODULE_ID ".Keyboard",
    0,
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,  
    JS_PropertyStub,
    JS_EnumerateStub, 
    JS_ResolveStub,   
    JS_ConvertStub,   
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  static JSFunctionSpec keyboard_methods[] = 
  {
    { "getChar",		keyboard_getChar,		0, 0, 0 },	
    { NULL,			NULL,				0, 0, 0 },
  };

  static JSPropertySpec keyboard_props[] = 
  {
    { NULL, 0, 0, NULL, NULL }
  };

  keyboard = JS_DefineObject(cx, obj, "keyboard", &keyboard_class, NULL, 0);
  if (!keyboard)
    return NULL;

  if (JS_DefineProperties(cx, keyboard, keyboard_props) != JS_TRUE)
    return NULL;


  if (JS_DefineFunctions(cx, keyboard, keyboard_methods) != JS_TRUE)
    return NULL;

  return keyboard;
}
