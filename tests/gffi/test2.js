#! /usr/bin/gsr -zzW

const ffi = require("gffi");

const _mmap 	= new ffi.CFunction(ffi.pointer, 	"mmap", 	ffi.pointer, ffi.size_t, ffi.int, ffi.int, ffi.int, ffi.off_t);
const _munmap 	= new ffi.CFunction(ffi.int, 		"munmap", 	ffi.pointer, ffi.size_t);
const _open 	= new ffi.CFunction(ffi.int, 		"open", 	ffi.pointer, ffi.int, ffi.int);
const _close 	= new ffi.CFunction(ffi.int,		"close", 	ffi.int);
const _strerror = new ffi.CFunction(ffi.pointer,	"strerror", 	ffi.int);
const _fstat 	= new ffi.CFunction(ffi.int, 		"fstat", 	ffi.int, ffi.pointer);

function perror(msg)
{
  if (ffi.errno)
    return msg + ": " + _strerror(ffi.errno).asString();

  return msg;
}

function mmap(filename)
{
  var fd = _open(filename, ffi.std.O_RDONLY, 0666);
  if (fd == -1)
    throw(new Error(perror("Cannot open file " + filename)));

  try
  {
    var sb = new ffi.MutableStruct("struct stat");
    if (_fstat(fd, sb) != 0)
      throw(new Error(perror("Cannot stat file " + filename)));

    var mem = _mmap(null, sb.st_size, ffi.std.PROT_READ, ffi.std.MAP_PRIVATE, fd, 0);
    if (mem == ffi.Memory(-1))
      throw(new Error(perror("Cannot read file " + filename)));

    mem.finalizeWith(_munmap, mem, sb.st_size);
    return mem;
  }
  finally
  {
    print(1);
    _close(fd);
    print(2);
  }
}

function munmap(mem)
{
  mem.destroy();
}

var m = mmap('/etc/passwd')
print(m.asString());
munmap(m);
