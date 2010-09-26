/*
 *  Test the ability to run JSAPI from GFFI in the current context
 */

const ffi = require("gffi");

const JS_CompileUCScript = new ffi.CFunction(
	ffi.pointer, 		// JSScript *
	"JS_CompileUCScript", 
	ffi.pointer, 		// JSContext *cx
	ffi.pointer,		// JSObject *obj
	ffi.pointer, 		// const jschar *chars
	ffi.size_t, 		// size_length
	ffi.pointer, 		// const char *filename
	ffi.uint		// uintN lineno
	);
JS_CompileUCScript.jsapiCall = true;

const JS_ExecuteScript = new ffi.CFunction(
	ffi.int,		// JSBool 
	"JS_ExecuteScript",
	ffi.pointer,		// JSContext *cx
	ffi.pointer, 		// JSObject *obj
	ffi.pointer,		// JSScript *script
	ffi.pointer		// jsval *rval
);
JS_ExecuteScript.jsapiCall = true;

const JS_ClearPendingException = new ffi.CFunction(
    ffi.void,
    "JS_ClearPendingException",
    ffi.pointer                 // JSContext *cx
);
JS_ClearPendingException.jsapiCall = true;

const globalObject 	= require("vm").globalObject;
const rval		= new ffi.Memory(8);

exports.checkStatement = function checkStatement(code)
{
  var cx	= require("vm").cx;
  var chars	= require("vm").jschars(code);
  var script    = JS_CompileUCScript(cx, globalObject, chars, code.length, "no_filename", 1);

  if (script == null)
  {
    JS_ClearPendingException(cx);
  }

  return script != null;
}

