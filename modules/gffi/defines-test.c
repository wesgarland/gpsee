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
 *  @file	defines-test.c	Debug code to see how gffi will resolve
 *				specific C defines
 *
 *  @author	Wes Garland
 *              PageMail, Inc.
 *		wes@page.ca
 *  @date	Jan 2010
 *  @version	$Id: defines-test.c,v 1.1 2010/01/18 22:06:23 wes Exp $
 */

#include "defines.c"

int main(int argc, const char *argv[])
{
  const char		*name = argv[1];
  const define_s 	*def;

  if ((def = getDefine(find_cdefGroup("std"), name, 0)))
  {
    switch(def->type)
    {
      case cdeft_signedInt:
	printf("The C value for %s is %i, it is a signed integer\n", name, (signed int)def->value.signedInt);
	break;
      case cdeft_unsignedInt:
	printf("The C value for %s is %i, it is an unsigned integer\n", name, (unsigned int)def->value.unsignedInt);
	break;
      case cdeft_floatingPoint:
	printf("The C value for %s is %f, it is a floating point number\n", name, (double)def->value.floatingPoint);
	break;
      case cdeft_string:
	printf("The C value for %s is %s, it is a string\n", name, def->value.string);
	break;
      case cdeft_alias:
	printf("The C value for %s an alias, it points to the value for %s\n", name, def->value.string);
	break;
      case cdeft_expr:
	printf("The C value for %s is %s, it is an expression\n", name, def->value.string);
	break;
    }
  }
  else
    printf("Couldn't find a value for %s in std defines group\n", name);

  return 0;
}

