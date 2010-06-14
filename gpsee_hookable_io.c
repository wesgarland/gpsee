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
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
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
 *  @file       gpsee_hookable_io.c     
 *  @brief      Hookable I/O for GPSEE internals. All user-oriented I/O (such as stack dumps)
 *              should use these routines, so that JavaScript programs (like TUI development
 *              environments) can hook them to avoid screen corruption etc.
 *  @author     Wes Garland, wes@page.ca
 *  @date       April 2010
 *  @version    $Id:$
 */

#include "gpsee.h"

/** GC Callback for hookable user I/O.  Insure the GC does not collect functions
 *  which are unreferenced in script, but used by the user io hooks.
 *
 *  @param      cx      Any context in the runtime
 *  @param      realm   Unused (hookable I/O is per-runtime, not per-realm)
 *  @param      status  Which phase of the GC tracer we are in
 */
static JSBool uio_GCCallback(JSContext *cx, gpsee_realm_t *unused, JSGCStatus status)
{
  size_t                i;
  gpsee_runtime_t       *grt = (gpsee_runtime_t *)JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (status != JSGC_MARK_END)
    return JS_TRUE;

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);
  for (i = 0; i < grt->user_io.hooks_len; i++)
  {
    if (grt->user_io.hooks[i].input != JSVAL_VOID)
      JS_MarkGCThing(cx, (void *)grt->user_io.hooks[i].input, "input hook", NULL);
    if (grt->user_io.hooks[i].output != JSVAL_VOID)
      JS_MarkGCThing(cx, (void *)grt->user_io.hooks[i].output, "output hook", NULL);
  }
  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  return JS_TRUE;
}

/**
 *  Code to output data to JS instead of stdio functions.
 *
 *  @param      cx      Any context in the runtime
 *  @param      realm   The invocation realm of the I/O hook
 *  @param      hookFn  The hook function for the relevant file descriptor
 *  @param      buf     The buffer to output
 *  @param      bufLen  The number of characters in the buffer.
 *
 *  @returns    Ideally, the number of bytes written -- we actually return bufLen if the return value of
 *              the JS hook function is not an integer; otherwise, we return that value.
 */
static size_t uio_fwrite_js(JSContext *cx, gpsee_realm_t *realm, jsval hookFn, const char *buf, size_t bufLen)
{
  jsval       rval;
  JSString    *str;
  jsval       argv[1];

  if (JS_EnterLocalRootScope(cx) == JS_FALSE)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: could not enter local root scope");

  str = JS_NewStringCopyN(cx, buf, bufLen);
  if (!str)
    panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: Out of Memory");
  argv[0] = STRING_TO_JSVAL(str);

  if (JS_CallFunctionValue(cx, realm->globalObject, hookFn, 1, argv, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    rval = JSVAL_ZERO;
  }

  JS_LeaveLocalRootScope(cx);

  return JSVAL_IS_INT(rval) ? JSVAL_TO_INT(rval) : bufLen;
}

/** 
 *  Fwrite Callback data - stored in a GPSEE Data Store, this data is used either by
 *  the realm destructor or an async callback to output pending data which could not
 *  be written earlier (normally because the context was throwing an exception at the
 *  time).
 */
struct pendingWrite_data
{
  char          *buf;
  size_t        len;
  jsval         hookFn;
};

/** DataStore callback to dump, then destroy, a specific piece of pending fwrite data */
static JSBool uio_dumpPendingWrites_cb(JSContext *cx, const void *key, void *value, void *private)
{
  struct pendingWrite_data      *data = (void *)key;
  gpsee_realm_t                 *realm = private;

  uio_fwrite_js(cx, realm, data->hookFn, data->buf, data->len);

  JS_free(cx, data->buf);
  JS_free(cx, data);

  return JS_TRUE;
}

/**
 *  Dump all pending fwrite data for a particular realm.
 *
 *  @param      cx      The current context (any context in the runtime)
 *  @param      realm   The realm for which we wish to dump the pending outut
 */
void gpsee_uio_dumpPendingWrites(JSContext *cx, gpsee_realm_t *realm)
{
  gpsee_ds_forEach(cx, realm->user_io_pendingWrites, uio_dumpPendingWrites_cb, realm);
  gpsee_ds_empty(realm->user_io_pendingWrites);
}

static JSBool uio_fwrite_dump_cb(JSContext *cx, void *vdata, GPSEEAsyncCallback *cb)
{
  gpsee_uio_dumpPendingWrites(cx, (gpsee_realm_t *)vdata);
  gpsee_removeAsyncCallback(cx, cb);

  return JS_TRUE;
}

#ifdef HAVE_CONST_CORRECT_FWRITE
# define CONST const
#else
# define CONST  /* */
#endif

static size_t uio_fwrite_hook(CONST void *ptr, size_t size, size_t nitems, FILE *file, JSContext *cx)
{
  gpsee_runtime_t       *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm;
  int                   fd = fileno(file);
  size_t                sz;

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  if (grt->user_io.hooks_len <= fd || grt->user_io.hooks[fd].output == JSVAL_VOID)
  {
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return fwrite(ptr, size, nitems, file);
  }

  realm = grt->user_io.hooks[fd].realm;

  if (JS_IsExceptionPending(cx) == JS_TRUE)
  {
    /* We cannot re-enter the JSAPI from the error reporter, so we post the event
     * to the async facility (operation callback) and then trigger it. Note that
     * cx->throwing does not guarantee that we are in the reporter, but that's okay.
     */
    struct pendingWrite_data *data = JS_malloc(cx, sizeof(*data));

    data->len = size * nitems;
    data->buf = JS_malloc(cx, data->len);;
    memcpy(data->buf, ptr, data->len);
 
    if (!data)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: out of memory allocating callback data");

    gpsee_addAsyncCallback(cx, uio_fwrite_dump_cb, realm);

    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return size * nitems;
  }

  sz = uio_fwrite_js(cx, realm, grt->user_io.hooks[fd].output, ptr, size * nitems);
  gpsee_leaveAutoMonitor(grt->monitors.user_io);
  return sz;
}

static int uio_vfprintf_hook(JSContext *cx, FILE *file, const char *fmt, va_list ap)
{
  int buflen = vsnprintf(NULL, 0, fmt, ap);
  char *buf;

  if (buflen <= 0)
    return buflen ? -1 : 0;

  buf = malloc(buflen + 1);     /* Can't JS_malloc, can't propagate OOM */
  vsnprintf(buf, buflen + 1, fmt, ap);

  return uio_fwrite_hook(buf, 1, buflen, file, cx);
}

static int uio_printf_hook(JSContext *cx, const char *fmt, ...)
{
  va_list               ap;
  int                   ret;
  gpsee_runtime_t       *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  va_start(ap, fmt);

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);
  if (grt->user_io.hooks == NULL)
    ret = vprintf(fmt, ap);
  else
    ret = uio_vfprintf_hook(cx, stdout, fmt, ap);
  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  va_end(ap);
  return ret;
}

static int uio_fprintf_hook(JSContext *cx, FILE *file, const char *fmt, ...)
{
  va_list               ap;
  int                   ret;
  gpsee_runtime_t   *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  va_start(ap, fmt);

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);
  if (grt->user_io.hooks == NULL)
    ret = vfprintf(file, fmt, ap);
  else
    ret = uio_vfprintf_hook(cx, file, fmt, ap);
  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  va_end(ap);
  return ret;
}

static int uio_fputs_hook(const char *buf, FILE *file, JSContext *cx)
{
  return uio_fwrite_hook((void *)buf, 1, strlen(buf), file, cx);
}

static int uio_puts_hook(const char *buf, JSContext *cx)
{
  return uio_fwrite_hook((void *)buf, 1, strlen(buf), stdout, cx);
}

static int uio_fputc_hook(int c, FILE *file, JSContext *cx)
{
  char ch = c;
  return uio_fwrite_hook(&ch, 1, 1, file, cx);
}

static char *uio_fgets_hook(char *buf, int len, FILE *file, JSContext *cx)
{
  int                   fd = fileno(file);
  jsval                 rval;
  jsval                 argv[2];
  gpsee_runtime_t       *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = gpsee_getRealm(cx);

  argv[0] = JSVAL_TRUE;         /* Read line */
  argv[1] = INT_TO_JSVAL(len);  /* Do not exceed len characters */

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  if (grt->user_io.hooks_len <= fd || grt->user_io.hooks[fd].output == JSVAL_VOID)
  {
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return fgets(buf, len, file);
  }

  realm = grt->user_io.hooks[fd].realm;

  if (JS_CallFunctionValue(cx, realm->globalObject, grt->user_io.hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return NULL;
  }

  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  if (!JSVAL_IS_STRING(rval))
    return NULL;

  memcpy(buf, JS_GetStringBytes(JSVAL_TO_STRING(rval)), min(len - 1, JS_GetStringLength(JSVAL_TO_STRING(rval))));
  buf[len - 1] = (char)0;

  return buf;
}

static int uio_fgetc_hook(FILE *file, JSContext *cx)
{
  int                   fd = fileno(file);
  jsval                 rval;
  jsval                 argv[2];
  gpsee_runtime_t       *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm;
  char                  c;

  argv[0] = JSVAL_FALSE;         /* Read exactly */
  argv[1] = INT_TO_JSVAL(1);     /* 1 byte */

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  if (grt->user_io.hooks_len <= fd || grt->user_io.hooks[fd].output == JSVAL_VOID)
  {
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return fgetc(file);
  }

  realm = grt->user_io.hooks[fd].realm;

  if (JS_CallFunctionValue(cx, realm->globalObject, grt->user_io.hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return -1;
  }

  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  if (!JSVAL_IS_STRING(rval))
    return -1;

  memcpy(&c, JS_GetStringBytes(JSVAL_TO_STRING(rval)), 1);

  return c;
}

static size_t uio_fread_hook(void *buf, size_t size, size_t nitems, FILE *file, JSContext *cx)
{
  int                   fd = fileno(file);
  jsval                 rval;
  jsval                 argv[2];
  gpsee_runtime_t       *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm;

  argv[0] = JSVAL_FALSE;                         /* Read exactly */
  argv[1] = INT_TO_JSVAL(size * nitems);         /* size * nitems bytes */

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  if (grt->user_io.hooks_len <= fd || grt->user_io.hooks[fd].output == JSVAL_VOID)
  {
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return fread(buf, size, nitems, file);
  }

  realm = grt->user_io.hooks[fd].realm;

  if (JS_CallFunctionValue(cx, realm->globalObject, grt->user_io.hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    gpsee_leaveAutoMonitor(grt->monitors.user_io);
    return 0;
  }

  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  if (!JSVAL_IS_STRING(rval))
    return 0;

  memcpy(buf, JS_GetStringBytes(JSVAL_TO_STRING(rval)), min(size * nitems, JS_GetStringLength(JSVAL_TO_STRING(rval))));

  return min(size * nitems, JS_GetStringLength(JSVAL_TO_STRING(rval)));
}

/** Initialize the user IO hooks to be as close to the bare
 *  metal as possible, freeing any previously-allocated 
 *  resources as we do so.
 */
void gpsee_resetIOHooks(JSContext *cx, gpsee_runtime_t *grt)
{
  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  grt->user_io.printf   = uio_printf_hook;
  grt->user_io.fprintf  = uio_fprintf_hook;
  grt->user_io.vfprintf = uio_vfprintf_hook;
  grt->user_io.fwrite   = (void *)fwrite;
  grt->user_io.fread    = (void *)fread;
  grt->user_io.fgets    = (void *)fgets;
  grt->user_io.fputs    = (void *)fputs;
  grt->user_io.fputc    = (void *)fputc;
  grt->user_io.puts     = (void *)puts;

  grt->user_io.hooks_len = 0;
  if (grt->user_io.hooks)
    JS_free(cx, grt->user_io.hooks);
  grt->user_io.hooks = NULL;

  gpsee_leaveAutoMonitor(grt->monitors.user_io);

  gpsee_addGCCallback(grt, NULL, uio_GCCallback);

  return;
}

/** Prepare a GPSEE runtime so that the IO hooks may be used.
 */
JSBool gpsee_initIOHooks(JSContext *cx, gpsee_runtime_t *grt)
{
  gpsee_resetIOHooks(cx, grt);
  return JS_TRUE;
}

static JSBool hookFileDescriptor(JSContext *cx, int fd, jsval ihook, jsval ohook)
{
  gpsee_runtime_t *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  gpsee_enterAutoMonitor(cx, &grt->monitors.user_io);

  if (grt->user_io.hooks_len <= min(fd, STDERR_FILENO))
  {
    void *old = grt->user_io.hooks;

    grt->user_io.hooks_len = 1 + min(fd, STDERR_FILENO);
    grt->user_io.hooks = JS_realloc(cx, grt->user_io.hooks, sizeof(grt->user_io.hooks[0]) * grt->user_io.hooks_len);

    if (grt->user_io.hooks == NULL)
    {
      grt->user_io.hooks = old;
      gpsee_leaveAutoMonitor(grt->monitors.user_io);
      return JS_FALSE;
    }

    if (old == NULL)    /* first-time allocation: init 3 fds before populating one */
    {
      size_t fd;

      for (fd = 0; fd < grt->user_io.hooks_len; fd++)
        grt->user_io.hooks[fd].input = grt->user_io.hooks[fd].output = JSVAL_VOID;
    }
  }

  grt->user_io.hooks[fd].input = ihook;
  grt->user_io.hooks[fd].output = ohook;
  grt->user_io.hooks[fd].realm = gpsee_getRealm(cx);

  grt->user_io.printf   = uio_printf_hook;
  grt->user_io.fprintf  = uio_fprintf_hook;
  grt->user_io.vfprintf = uio_vfprintf_hook;
  grt->user_io.fwrite   = uio_fwrite_hook;
  grt->user_io.fread    = uio_fread_hook;
  grt->user_io.fgets    = uio_fgets_hook;
  grt->user_io.fputs    = uio_fputs_hook;
  grt->user_io.fputc    = uio_fputc_hook;
  grt->user_io.fgetc    = uio_fgetc_hook;
  grt->user_io.puts     = uio_puts_hook;

  gpsee_leaveAutoMonitor(grt->monitors.user_io);
  return JS_TRUE;
}



