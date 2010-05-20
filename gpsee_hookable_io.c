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
 *  @file       gpsee_hookable_io.c     Hookable I/O for GPSEE internals. All user-oriented I/O (such as stack dumps)
 *                                      should use these routines, so that JavaScript programs (like TUI development
 *                                      environments) can hook them to avoid screen corruption etc.
 *  @author     Wes Garland, wes@page.ca
 *  @date       April 2010
 *  @version    $Id:$
 */

#include "gpsee.h"

struct fwrite_callback_data
{
  char          *buf;
  size_t        len;
  FILE          *file;
};

static JSBool uio_fwrite_callback(JSContext *cx, void *vdata)
{
  struct fwrite_callback_data   *data = vdata;

  fwrite(data->buf, 1, data->len, data->file);
  JS_free(cx, data->buf);
  JS_free(cx, vdata);

#warning Small, rare, core-leak during race condition described in issue 66 
  gpsee_removeAsyncCallback(cx, uio_fwrite_callback);

  return JS_TRUE;
}

static size_t uio_fwrite_hook(const void *ptr, size_t size, size_t nitems, FILE *file, JSContext *cx)
{
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  int                   fd = fileno(file);

  if (!realm || jsi->user_io_hooks_len <= fd || jsi->user_io_hooks[fd].output == JSVAL_VOID)
    return fwrite(ptr, size, nitems, file);

  if (JS_IsExceptionPending(cx) == JS_TRUE)
  {
    /* We cannot re-enter the JSAPI from the error reporter, so we post the event
     * to the async facility (operation callback) and then trigger it. Note that
     * cx->throwing does not guarantee that we are in the reporter, but that's okay.
     */
    struct fwrite_callback_data *data = JS_malloc(cx, sizeof(*data));

    data->len = size * nitems;
    data->buf = JS_malloc(cx, data->len);;
    memcpy(data->buf, ptr, data->len);

    if (!data)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: out of memory allocating callback data");

    gpsee_addAsyncCallback(cx, uio_fwrite_callback, data);

    return size * nitems;
  }
  else
  {
    jsval       rval;
    JSString    *str;
    jsval       argv[1];

    if (JS_EnterLocalRootScope(cx) == JS_FALSE)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: could not enter local root scope");

    str = JS_NewStringCopyN(cx, ptr, size * nitems);
    if (!str)
      panic(GPSEE_GLOBAL_NAMESPACE_NAME ".user_io.fwrite: Out of Memory");
    argv[0] = STRING_TO_JSVAL(str);

    if (JS_CallFunctionValue(cx, realm->globalObject, jsi->user_io_hooks[fd].output, 1, argv, &rval) == JS_FALSE)
    {
      JS_ReportPendingException(cx);
      JS_ClearPendingException(cx);
      rval = JSVAL_ZERO;
    }

    JS_LeaveLocalRootScope(cx);

    return JSVAL_IS_INT(rval) ? JSVAL_TO_INT(rval) : size * nitems;
  }
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
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  va_start(ap, fmt);

  if (jsi->user_io_hooks == NULL)
    ret = vprintf(fmt, ap);
  else
    ret = uio_vfprintf_hook(cx, stdout, fmt, ap);

  va_end(ap);
  return ret;
}

static int uio_fprintf_hook(JSContext *cx, FILE *file, const char *fmt, ...)
{
  va_list               ap;
  int                   ret;
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  va_start(ap, fmt);

  if (jsi->user_io_hooks == NULL)
    ret = vfprintf(file, fmt, ap);
  else
    ret = uio_vfprintf_hook(cx, file, fmt, ap);

  va_end(ap);
  return ret;
}

static int uio_fputs_hook(const char *buf, FILE *file, JSContext *cx)
{
  return uio_fwrite_hook(buf, 1, strlen(buf), file, cx);
}

static int uio_puts_hook(const char *buf, JSContext *cx)
{
  return uio_fwrite_hook(buf, 1, strlen(buf), stdout, cx);
}

static int uio_fputc_hook(const char c, FILE *file, JSContext *cx)
{
  return uio_fwrite_hook(&c, 1, 1, file, cx);
}

static char *uio_fgets_hook(char *buf, size_t len, FILE *file, JSContext *cx)
{
  int                   fd = fileno(file);
  jsval                 rval;
  jsval                 argv[2];
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = gpsee_getRealm(cx);

  argv[0] = JSVAL_TRUE;         /* Read line */
  argv[1] = INT_TO_JSVAL(len);  /* Do not exceed len characters */

  if (!realm || jsi->user_io_hooks_len <= fd || jsi->user_io_hooks[fd].output == JSVAL_VOID)
    return fgets(buf, len, file);

  if (JS_CallFunctionValue(cx, realm->globalObject, jsi->user_io_hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    return NULL;
  }

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
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = gpsee_getRealm(cx);
  char                  c;

  argv[0] = JSVAL_FALSE;         /* Read exactly */
  argv[1] = INT_TO_JSVAL(1);     /* 1 byte */

  if (jsi->user_io_hooks_len <= fd || jsi->user_io_hooks[fd].output == JSVAL_VOID)
    return fgetc(file);

  if (JS_CallFunctionValue(cx, realm->globalObject, jsi->user_io_hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    return -1;
  }

  if (!JSVAL_IS_STRING(rval))
    return -1;

  memcpy(&c, JS_GetStringBytes(JSVAL_TO_STRING(rval)), 1);

  return c;
}

static size_t uio_fread_hook(char *buf, size_t size, size_t nitems, FILE *file, JSContext *cx)
{
  int                   fd = fileno(file);
  jsval                 rval;
  jsval                 argv[2];
  gpsee_interpreter_t   *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));
  gpsee_realm_t         *realm = gpsee_getRealm(cx);

  argv[0] = JSVAL_FALSE;                         /* Read exactly */
  argv[1] = INT_TO_JSVAL(size * nitems);         /* size * nitems bytes */

  if (!realm || jsi->user_io_hooks_len <= fd || jsi->user_io_hooks[fd].output == JSVAL_VOID)
    return fread(buf, size, nitems, file);

  if (JS_CallFunctionValue(cx, realm->globalObject, jsi->user_io_hooks[fd].input, 0, NULL, &rval) == JS_FALSE)
  {
    JS_ReportPendingException(cx);
    JS_ClearPendingException(cx);
    return 0;
  }

  if (!JSVAL_IS_STRING(rval))
    return 0;

  memcpy(buf, JS_GetStringBytes(JSVAL_TO_STRING(rval)), min(size * nitems, JS_GetStringLength(JSVAL_TO_STRING(rval))));

  return min(size * nitems, JS_GetStringLength(JSVAL_TO_STRING(rval)));
}

/** Initialize the user IO hooks to be as close to the bare
 *  metal as possible, freeing any previously-allocated 
 *  resources as we do so.
 */
void gpsee_initIOHooks(JSContext *cx, gpsee_interpreter_t *jsi)
{
  jsi->user_io_printf   = uio_printf_hook;
  jsi->user_io_fprintf  = uio_fprintf_hook;
  jsi->user_io_vfprintf = uio_vfprintf_hook;
  jsi->user_io_fwrite   = (void *)fwrite;
  jsi->user_io_fread    = (void *)fread;
  jsi->user_io_fgets    = (void *)fgets;
  jsi->user_io_fputs    = (void *)fputs;
  jsi->user_io_fputc    = (void *)fputc;
  jsi->user_io_puts     = (void *)puts;

  jsi->user_io_hooks_len = 0;
  if (jsi->user_io_hooks)
    JS_free(cx, jsi->user_io_hooks);
  jsi->user_io_hooks = NULL;

  return;
}

static JSBool hookFileDescriptor(JSContext *cx, int fd, jsval ihook, jsval ohook)
{
  gpsee_interpreter_t *jsi = JS_GetRuntimePrivate(JS_GetRuntime(cx));

  if (jsi->user_io_hooks_len <= min(fd, STDERR_FILENO))
  {
    void *old = jsi->user_io_hooks;

#warning Need jsi mutex here
    jsi->user_io_hooks_len = 1 + min(fd, STDERR_FILENO);
    jsi->user_io_hooks = JS_realloc(cx, jsi->user_io_hooks, sizeof(jsi->user_io_hooks[0]) * jsi->user_io_hooks_len);

    if (jsi->user_io_hooks == NULL)
    {
      jsi->user_io_hooks = old;
      return JS_FALSE;
    }

    if (old == NULL)    /* first-time allocation: init 3 fds before populating one */
    {
      size_t fd;

      for (fd = 0; fd < jsi->user_io_hooks_len; fd++)
        jsi->user_io_hooks[fd].input = jsi->user_io_hooks[fd].output = JSVAL_VOID;
    }
  }

  jsi->user_io_hooks[fd].input = ihook;
  jsi->user_io_hooks[fd].output = ohook;

  jsi->user_io_printf   = uio_printf_hook;
  jsi->user_io_fprintf  = uio_fprintf_hook;
  jsi->user_io_vfprintf = uio_vfprintf_hook;
  jsi->user_io_fwrite   = uio_fwrite_hook;
  jsi->user_io_fread    = uio_fread_hook;
  jsi->user_io_fgets    = uio_fgets_hook;
  jsi->user_io_fputs    = uio_fputs_hook;
  jsi->user_io_fputc    = uio_fputc_hook;

  return JS_TRUE;
}

