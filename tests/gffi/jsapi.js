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

const globalObject 	= require("vm").globalObject;
const cx		= require("vm").cx;
const code 		= "print('it works');";
const chars		= require("vm").jschars(code);
const rval		= new ffi.Memory(8);

var script = JS_CompileUCScript(cx, globalObject, chars, code.length, "no_filename", 1);
print("script = " + script);
print("globalObject = " + ffi.Memory(globalObject));

JS_ExecuteScript(cx, globalObject, script, rval);
