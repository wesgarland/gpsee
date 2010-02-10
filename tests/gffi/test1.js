#! /usr/bin/gsr -ddzz

// GFFI program which lightly tests:
//  - call method
//  - struct dirent, struct stat, struct passwd
//  - pre-processor integer
//  - struct character arrays which "fall off" the end as strings
//  - struct char pointers as strings
//  - pointer-to-struct cast
//  - falsey null pointers
//  - errno

const ffi = require("gffi");
const _opendir = new ffi.CFunction(ffi.pointer, "opendir", ffi.pointer);	/* void *opendir(void *) */
const _readdir = new ffi.CFunction(ffi.pointer, "readdir", ffi.pointer);	/* void *readdir(void *) */
const _stat = new ffi.CFunction(ffi.int, "stat", ffi.pointer, ffi.pointer);	/* int stat(void *, void *) */
const _strerror = new ffi.CFunction(ffi.pointer, "strerror", ffi.int);
const _getpwuid = new ffi.CFunction(ffi.pointer, "getpwuid", ffi.uid_t);

var dirp;
var dent;
var pwent;
var sb = new ffi.MutableStruct("struct stat");			/* Create a JS object reflecting struct stat */
var filename;
var owner;

dirp = _opendir.call("/etc");					/* String literal automatically becomes pointer for call () */
if (!dirp)
  throw("can't open dir");

while (dent = _readdir.call(dirp))				/* dent returned as instanceof Memory or null */
{
  dent = ffi.MutableStruct(dent, "struct dirent");		/* dent pointer casted to struct dirent JS reflection object (no copy),
								 * FFI layer holds weak reference to original dent via JSTraceOp 
								 * until this one is final.
								 */
  filename = "/etc/" + dent.d_name.asString(-1);		/* d_name falls off the struct: -1 tells FFI to believe strlen, not sizeof() */

  if (_stat.call(filename, sb) != 0)
  {				/* String and instanceof MutableStruct become pointer for call() */
    print("can't stat " + filename + ", errno=" + ffi.errno);
    continue;
  }

  pwent = _getpwuid.call(sb.st_uid);
  if (!pwent)
    throw("Could not turn UID " + statBuf.st_uid + " into a username: " + _strerror.call(ffi.errno));

  owner = ffi.MutableStruct(pwent, "struct passwd").pw_name;	/* Type cast (no copy), then dereference member */

  if (sb.st_mode & ffi.std.S_IFDIR)				/* S_IFDIR is an int jsval defined in header collection named posix */
    print(filename + " is a directory owned by " + owner)
  else
    print(filename + " is a " + sb.st_size + " byte file owned by " + owner);
}
