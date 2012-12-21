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
 *  @file	gpsee_private.h		Private interfaces for use within GPSEE.
 *  @author	Wes Garland, PageMail, Inc., wes@page.ca
 *  @date	March 2009
 *  @version	$Id: gpsee_private.h,v 1.10 2010/09/01 18:12:35 wes Exp $
 *  @ingroup    internal
 */

#ifndef GPSEE_PRIVATE_H
#define GPSEE_PRIVATE_H

#include "jsapi.h"
JSBool                  gpsee_initializeModuleSystem    (JSContext *cx, gpsee_realm_t *realm);
void 			gpsee_shutdownModuleSystem      (JSContext *cx, gpsee_realm_t *realm);
void			gpsee_moduleSystemCleanup       (JSContext *cx, gpsee_realm_t *realm);
JSBool                  gpsee_initializeMonitorSystem   (JSContext *cx, gpsee_runtime_t *grt);      
void                    gpsee_shutdownMonitorSystem     (gpsee_runtime_t *grt);
gpsee_realm_t *         gpsee_getModuleScopeRealm       (JSContext *cx, JSObject *moduleScope);
JSBool                  gpsee_operationCallback         (JSContext *cx);
JSBool                  gpsee_gcCallback                (JSContext *cx, JSGCStatus status);
#define AT_STRINGIFY_HELPER_1(s) #s
#define AT_STRINGIFY_HELPER_2(s) AT_STRINGIFY_HELPER_1(s)
#define AT __FILE__ ":" AT_STRINGIFY_HELPER_2(__LINE__) ": "

#endif /*GPSEE_PRIVATE_H*/
