#include "jsfun.h"

JS_BEGIN_EXTERN_C

JS_PUBLIC_API(JSBool)
JS_SetFunctionNative(JSContext *cx, JSFunction *fun, JSNative funp)
{
    if (FUN_NATIVE(fun) == NULL)
	return JS_FALSE;

    fun->u.n.native = funp;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetFunctionFastNative(JSContext *cx, JSFunction *fun, JSFastNative funp)
{
    if (FUN_FAST_NATIVE(fun) == NULL)
	return JS_FALSE;

    fun->u.n.native = (JSNative)funp;
    return JS_TRUE;
}

JS_END_EXTERN_C

