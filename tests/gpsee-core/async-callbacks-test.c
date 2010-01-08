#include <stdio.h>
#include "gpsee.h"

JSBool callback(JSContext *cx, void *ignored)
{
  printf("callback() called\n");
  return JS_TRUE;
}
/** GPSEE uses panic() to panic, expects embedder to provide */
JS_FRIEND_API(void) __attribute__((noreturn)) panic(const char *message)
{
  printf("fatal error: %s\n", message);
  abort();
}

int main(int argc, char **argv)
{
  gpsee_interpreter_t *jsi;
  JSContext *cx;
  GPSEEAsyncCallback *cb;

  jsi = gpsee_createInterpreter(NULL, NULL);
  if (!jsi)
    panic("UNEXPECTED: could not instantiate gpsee_interpreter_t\n");
  cx = JS_NewContext(jsi->rt, 65536);
  if (!cx)
    panic("UNEXPECTED: could not instantiate JSContext\n");
  gpsee_addAsyncCallback(cx, callback, (void*)1);
  gpsee_addAsyncCallback(cx, callback, (void*)2);
  gpsee_addAsyncCallback(cx, callback, (void*)3);
  for (cb = jsi->asyncCallbacks; cb && cb->cx != cx; cb = cb->next);
  if (cb && cb->cx == cx)
    printf("EXPECTED: callback exists\n");
  else
    printf("UNEXPECTED: callback does not exist\n");
  JS_DestroyContext(cx);
  for (cb = jsi->asyncCallbacks; cb && cb->cx != cx; cb = cb->next);
  if (cb && cb->cx == cx)
    printf("FAILURE: all callbacks not destroyed\n");
  else
    printf("SUCCESS: all callbacks destroyed\n");

  return 0;
}

