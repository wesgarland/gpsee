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
 *
 *  @file	fs-base.js	Implementation of filesystem/a/0 for GPSEE.
 *  @author	Wes Garland
 *  @date	Aug 2009
 *  @version	$Id: fs-base.js,v 1.16 2010/10/05 01:16:48 wes Exp $
 */

const binary = require("binary");
const ffi = require("gffi");
const dl = ffi;		/**< Dynamic lib handle for pulling symbols */
const dh = ffi.gpsee	/**< Header collection for #define'd constants */

const _umask		= new dl.CFunction(ffi.mode_t,  "umask",		ffi.mode_t);
const _open 		= new dl.CFunction(ffi.int, 	"open", 		ffi.pointer, ffi.int, ffi.int);
const _close		= new dl.CFunction(ffi.int,	"close",		ffi.int);
const _fdopen 		= new dl.CFunction(ffi.pointer, "fdopen", 		ffi.int, ffi.pointer);
const _fclose		= new dl.CFunction(ffi.int,	"fclose",		ffi.pointer);
const _stat 		= new dl.CFunction(ffi.int, 	"stat", 		ffi.pointer, ffi.pointer);
const _fstat 		= new dl.CFunction(ffi.int, 	"fstat", 		ffi.int, ffi.pointer);
const _lstat 		= new dl.CFunction(ffi.int, 	"lstat", 		ffi.pointer, ffi.pointer);
const _access		= new dl.CFunction(ffi.int,	"access",		ffi.pointer, ffi.int);
const _chmod		= new dl.CFunction(ffi.int,	"chmod",		ffi.pointer, ffi.mode_t);
const _chown		= new dl.CFunction(ffi.int,	"chown",		ffi.pointer, ffi.uid_t, ffi.gid_t);
const _strerror		= new dl.CFunction(ffi.pointer, "strerror",		ffi.int);
const _fgets 		= new dl.CFunction(ffi.pointer, "fgets", 		ffi.pointer, ffi.sint, ffi.pointer);
const _fputs		= new dl.CFunction(ffi.int,	"fputs",		ffi.pointer, ffi.pointer);
const _strtok		= new dl.CFunction(ffi.pointer, "strtok",		ffi.pointer, ffi.pointer);
const _rename		= new dl.CFunction(ffi.int,	"rename",		ffi.pointer, ffi.pointer);
const _unlink		= new dl.CFunction(ffi.int,	"unlink",		ffi.pointer);
const _utime		= new dl.CFunction(ffi.int,	"utime",		ffi.pointer, ffi.pointer);
const _mkdir		= new dl.CFunction(ffi.int,	"mkdir",		ffi.pointer, ffi.int);
const _rmdir		= new dl.CFunction(ffi.int,	"rmdir",		ffi.pointer);
const _gpsee_resolvepath= new dl.CFunction(ffi.int, 	"gpsee_resolvepath", 	ffi.pointer, ffi.pointer, ffi.size_t);
const _getcwd		= new dl.CFunction(ffi.pointer, "getcwd",		ffi.pointer, ffi.size_t);
const _chdir		= new dl.CFunction(ffi.int,	"chdir",		ffi.pointer);
const _getpwuid		= new dl.CFunction(ffi.pointer, "getpwuid",		ffi.uid_t);
const _getpwnam		= new dl.CFunction(ffi.pointer, "getpwnam",		ffi.pointer);
const _link		= new dl.CFunction(ffi.int,	"link",			ffi.pointer, ffi.pointer);
const _symlink		= new dl.CFunction(ffi.int,	"symlink",		ffi.pointer, ffi.pointer);
const _readlink		= new dl.CFunction(ffi.ssize_t,	"readlink",		ffi.pointer, ffi.pointer, ffi.size_t);
const _readdir 		= new dl.CFunction(ffi.pointer, "readdir", 		ffi.pointer);
const _opendir 		= new dl.CFunction(ffi.pointer, "opendir", 		ffi.pointer);
const _fwrite		= new dl.CFunction(ffi.size_t,	"fwrite",		ffi.pointer, ffi.size_t, ffi.size_t, ffi.pointer);
const _fread		= new dl.CFunction(ffi.size_t,	"fread",		ffi.pointer, ffi.size_t, ffi.size_t, ffi.pointer);
const _ftello		= new dl.CFunction(ffi.off_t,	"ftello", 		ffi.pointer);

/**
 *  Return a string documenting the most recent OS-level error, if there was one.
 *  @param 	force	Always return an error, even if it's (No Error)
 *  @returns	A string, possibly empty.
 */
function syserr(force)
{
  if (ffi.errno || force)
    return ' (' + _strerror.call(ffi.errno).asString() + ')';
  else
    return '';
}

/**
 *  Return a mutable struct containing the stat buffer matching thing.
 *
 *  @param	thing	Either a path or an open Stream.
 *  @returns	A mutable struct stat object.
 */
function stat(thing)
{
  var sb = new ffi.MutableStruct("struct stat");

  if (thing instanceof Stream)
  {
    if (_fstat.call(thing.fd, sb) != 0)
      throw new Error("Cannot stat stream" + syserr());
  }
  else
  {
    if (_stat.call(thing, sb) != 0)
      throw new Error("Cannot stat path '"+path+"'" + syserr());
  }

  return sb;
}

/** Open an existing file descriptor for use with fs-base
 *  @param	fd	Number of file descriptor
 *  @param	mode	fs-base mode object
 *  @returns	An open Stream
 */
exports.openDescriptor = function openDescriptor(fd, mode)
{
  var streamMode;

  if (mode.append)
    streamMode = mode.read ? "a+" : "a";
  else
  {
    if (mode.read && mode.write)
      streamMode = "r+";
    else
      if (mode.read)
	streamMode = "r";
      else
	streamMode = "w";
  }

  var stream = _fdopen.call(fd, streamMode);
  if (!stream)
  {
    _close.call(fd);
    throw new Error("Unable to create stdio file stream" + syserr());
  }

  stream.finalizeWith(_fclose, stream);

  return new Stream(stream, fd, mode);
}

/**
 *  Open a  raw byte stream with the given mode and permissions from the system. 
 *  @param	permissions	Used to modify the Permissions.default object by passing to the Permissions constructor. 
 *				The resultant Permissions instance is used when creating this file.  This parameter is optional.
 *
 *  @param	mode	Mode to open the stream in. The mode object's properties are interpreted as follows:
 *			- read:		open for reading
 * 			- write: 	open for writing
 * 			- append: 	open for writing: the file position is set to the end of the file before every write. 
 *			- create: 	create the file if it does not exist
 *			- exclusive: 	used only in conjunction with create, specifies that the if the file already exists 
 *			  		that the open should fail. Implemented with atomic filesystem primitives. 
 *			- truncate: 	used only in conjunction with write or append, specifies that if the path exists, it 
 *					is truncated (not replaced) upon open. 
 *  @throws 	when path is a symbolic link, or when exclusive is used without create.
 *  @throws	when append is not used in conjunction with write.
 *  @throws	when truncate is used without write or append. 
 */
exports.openRaw = function(path, mode, permissions) 
{
  var p = new Permissions(permissions);
  var m = 0;
  var fd;
  var oldUmask;

  if (mode.read && mode.write)
    m |= dh.O_RDWR;
  else
  {
    if (mode.read) m |= dh.O_RDONLY;
    if (mode.write) m |= dh.O_WRONLY;
  }

  if (mode.append)	m |= dh.O_APPEND;
  if (mode.create)	m |= dh.O_CREAT;
  if (mode.exclusive)	m |= dh.O_EXCL;
  if (mode.truncate)	m |= dh.O_TRUNC;

  if (mode.append && !mode.write)
    throw new Error("Cannot open '" + path + "' for append without write");

  if (mode.exclusive && !mode.create)
    throw new Error("Cannot open '" + path + "' for exclusive without create");

  if (mode.truncate && !mode.write)
    throw new Error("Cannot open '" + path + "' for truncate without write");

  oldUmask = _umask.call(0);
  fd = _open.call(path, m, p.toUnix());
  _umask.call(oldUmask);

  if (fd == -1)
    throw new Error("Unable to open file '" + path + "'" + syserr());

  if (!mode.write && exports.isDirectory(path)) /* open(2) already threw if writing a dir */
  {
    _close.call(fd);
    throw new Error("Cannot open '" + path + "' - is a directory");
  }

  return exports.openDescriptor(fd, mode);
};

/** Move a file at one path to another. If necessary, copies then removes the original. 
 *  All moves (renames) are performed atomically. Directory Moves across filesystems are 
 *  not supported, but files are copy/unlinked.
 *
 *  @param	source		Source filename
 *  @param	target		Target filename
 */
exports.move = function move(source, target)
{
  if (_rename.call(source, target) != 0)
    throw new Error("Cannot rename '" + source + "' to '" + target + "'" + syserr());
}

/** 
 *  Remove the file at the given path.
 */
exports.remove = function remove(path)
{
  var sb = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb) != 0)
    throw new Error("Cannot remove '" + path + "'" + syserr());

  if ((sb.st_mode & (dh.S_IFREG | dh.S_IFLNK)) == 0)
    throw new Error("Cannot remove '" + path + "' - not a regular file nor a symbolic link");

  if (_unlink.call(path) != 0)
    throw new Error("Cannot remove '" + path + "'" + syserr());
}

/** 
 *  Set the modification time of a file or directory at a given path
 *  to a specified time, or the current time. Creates an empty file at
 *  the given path if no file (special or otherwise) or directory exists.
 *
 *  @param	path			Path to touch
 *  @param	when	[optional]	Instance of Date, or DuckOf-Date responding to valueOf. 
 */
exports.touch = function touch(path, when)
{
  var tb;
  var sb = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb) != 0)
  {
    if (ffi.errno != dh.ENOENT)
      throw new Error("Cannot touch '" + path + "'" + syserr());
    exports.openRaw(path, { write: true, create: true, exclusive: true }).close();
  }

  if (typeof(when) == "undefined")
  {
    tb = null;
  }
  else
  {
    tb = new ffi.MutableStruct("struct utimbuf");
    
    tb.actime = when.valueOf() / 1000;
    tb.modtime = tb.actime;
  }

  if (_utime.call(path, tb) != 0)
    throw new Error("Cannot touch '" + path + "'" + syserr());
}

/**
 *  Create a single directory specified by path. If the directory cannot be created
 *  for any reason an exception will be thrown. This includes if the parent directories 
 *  of "path" are not present. The permissions object passed to this method is used as 
 *  the argument to the Permissions constructor. The resultant Permissions instance is
 *  applied to the given path during directory creation. 
 *
 *  We create the directory with the exact permissions given, rather than applying the 
 *  permissions after directory creation. 
 *
 *  @param	path				The name of directory to create
 *  @param	permissions	[optional]	A Permissions-like object
 */
exports.makeDirectory = function makeDirectory(path, permissions)
{
  var p = new Permissions(permissions);
  
  if (_mkdir.call(path, p.toUnix()) != 0)
    throw new Error("Cannot create directory '" + path + "'" + syserr());
}

/**
 *  Removes a directory if it is empty. If path is not empty, not a directory, or cannot 
 *  be removed for another reason an exception will be thrown. If path is a link, the 
 *  link will be removed if the canonical name of the link is a directory. 
 *
 *  @param	path		The name of the directory to remove
 */
exports.removeDirectory = function removeDirectory(path)
{
  if (_rmdir.call(path) != 0)
  {
    if (exports.isLink(path) && exports.isDirectory(exports.canonical(path)))
      if (_unlink.call(path) == 0)
	return;
    throw new Error("Cannot remove directory '" + path + "'" + syserr());
  }
}

/**
 *  Return the canonical path to a given abstract path. Canonical paths are both absolute 
 *  and intrinsic, such that all paths that refer to a given file (whether it exists or not)
 *  have the same corresponding canonical path. This function is equivalent to joining the
 *  given path to the current working directory (if the path is relative), joining all 
 *  symbolic links along the path, and normalizing the result to remove relative path (. or ..) 
 *f  references. 
 */
exports.canonical = function canonical(path) 
{
  var	i;
  var	buf = new ffi.Memory(dh.FILENAME_MAX);

  if (_gpsee_resolvepath.call(path, buf, buf.size) == -1)
    throw new Error("Cannot resolve path '"+path+"'" + syserr());

  return buf.asString(-1);
};

/** 
 *  Returns the current working directory as a String
 *
 *  @returns	String
 *  @throws	Error	if it cannot determine the cwd
 */
exports.workingDirectory = function workingDirectory()
{
  var 	buf = new ffi.Memory(dh.FILENAME_MAX);
  var	dirp = _getcwd.call(buf, buf.size);

  if (!dirp)
    throw new Error("Cannot determine working directory" + syserr());

  return dirp.asString(-1);
};

/**
 *  Changes the current working directory to the given path, resolved on the current working
 *  directory. Throws an error if the change failed.
 */
exports.changeWorkingDirectory = function changeWorkingDirectory(path)
{
  if (_chdir.call(path) != 0)
    throw new Error("Cannot change working directory to '" + path + "'" + syserr());
}

/** 
 *   Returns the name of the owner of a file. Where the owner name is not defined, the 
 *   numeric user id is used instead.
 *
 *   @param	path	The file or directory to query
 *   @returns	a String when the owner name is known, a Number when uid is returned instead.
 */
exports.owner = function owner(path)
{
  var sb = stat(path);
  var pwent;

  ffi.errno = 0;
  if ((pwent = getpwuid(sb.st_uid)))
    return pwent.pw_name.asString();
  else
    return sb.st_uid;
}

/**
 *  Set the owner of a given file. 
 */
exports.changeOwner = function changeOwner(path, owner)
{
  var pwent;

  if (typeof owner !== "number")
  {
    ffi.errno = 0;
    if ((pwent = getpwnam(owner)))
      owner = pwent.pw_uid;
    else
      throw new Error("Cannot determine user ID for user '" + owner + "'" + syserr());
  }

  if (_chown.call(path, owner, -1) != 0)
    throw new Error("Cannot change ownership of  '" + path + "'" + syserr());
}

/**
 *  Return a Permissions object describing the current permissions for a given path. 
 */
exports.permissions = function permissions(path)
{
  var sb = stat(path);

  return Permissions.fromUnix(sb.st_mode);
}

/** 
 *  Set the permissions for a given path. The permissions object passed to this method
 *  is used as the argument to the Permissions constructor. The resultant Permissions 
 *  instance is applied to the given path. 
 */
exports.changePermissions = function changePermissions(path, permissions)
{
  var p = new Permissions(permissions);

  if (_chmod.call(path, p.toUnix()) != 0)
    throw new Error("Cannot change permissions on  '" + path + "'" + syserr());
}

/**
 *  Create a symbolic link at the target path that refers to the source path. 
 */
exports.link = function link(source, target)
{
  if (_symlink.call(source, target) != 0)
    throw new Error("Cannot create symbolic link '" + target + "'" + syserr());
}

/** 
 *  Creates a hard link at the target path that shares storage with the source path. 
 *  Throws an error if this is not possible, such as when the source and target are on separate
 *  logical volumes or hard links are not supported by the volume. 
 */
exports.hardLink = function hardLink(source, target)
{
  if (_link.call(source, target) != 0)
    throw new Error("Cannot create hard link '" + target + "'" + syserr());
}

/**
 *  Return the target of a symbolic link at a given path.
 */
exports.readLink = function readLink(path) 
{
  var buf = new ffi.Memory(dh.FILENAME_MAX);

  if (_readlink.call(path, buf, buf.size) != 0)
    throw new Error("Cannot read link '" + path + "'" + syserr());

  return buf.asString(-1);
}

/**
 *  Return true if a file (of any type) or a directory exists at a given path. 
 *  If the file is a broken symbolic link, returns false. 
 */
exports.exists = function exists(path)
{
  var sb = new ffi.MutableStruct("struct stat");

  return _stat.call(path, sb) == 0;
};

/**
 *  Return true if a path exists and that it, after resolution of symbolic links, 
 *  corresponds to a regular file. 
 */
exports.isFile = function isFile(path)
{
  var sb = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb) == 0)
    return (sb.st_mode & dh.S_IFREG) != 0;
  return false;
};

/**
 *  Return true if a path exists and that it, after resolution of symbolic links, 
 *  corresponds to a directory. 
 */
exports.isDirectory = function isDirectory(path)
{
  var sb = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb) == 0)
    return (sb.st_mode & dh.S_IFDIR) != 0;
  return false;
};

/**
 *  Return true if a symbolic link exists at target.
 */
exports.isLink = function isLink(path)
{
  var sb = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb) == 0)
    return (sb.st_mode & dh.S_IFLNK) != 0;
  return false;
};

/**
 *  Return if path exists, that it does not correspond to directory, and that it
 *  can be opened for reading by "fs.open". 
 */
exports.isReadable = function isReadable(path)
{
  return (_access.call(path, dh.R_OK) == 0);
};

/**
 *  If a path exists, returns whether a file may be opened for writing, or entries 
 *  added or removed from an existing directory. If the path does not exist, returns 
 *  whether entries for files, directories, or links can be created at its location. 
 */
exports.isWritable = function isWritable(path) 
{
  return _access.call(path, dh.W_OK) == 0;
};

/**
 *  Determine whether two paths refer to the same storage (file or directory), either by 
 *  virtue of symbolic or hard links, such that modifying one would modify the other. In 
 *  the case where either some or all paths do not exist, we return false. If we are 
 *  unable to verify if the storage is the same (such as by having insufficient permissions), 
 *  an exception is thrown.
 *
 *  @param	pathA		A file or directory name
 *  @param	pathB		A file or directory name
 *  @returns	boolean
 */
exports.same = function same(pathA, pathB)
{
  var sb1 = new ffi.MutableStruct("struct stat");
  var sb2 = new ffi.MutableStruct("struct stat");

  if (_stat.call(path, sb1) != 0)
    sb1 = false;
  else
  {
    if (_stat.call(path, sb1) != 0)
      sb2 = false;
  }

  if (!sb1 || !sb2)
  {
    if (ffi.errno == ffi.dh.ENOENT)
      return false;

    throw new Error("Unable to compare '" + pathA + "' and '" + pathB + "'" + syserr());
  }

  if (typeof sb1.st_dev != "undefined")
    if (sb1.st_dev != sb2.st_dev)
      return false;

  if (typeof sb1.st_rdev != "undefined")
    if (sb1.st_rdev != sb2.st_rdev)
      return false;

  return sb1.st_ino == sb2.st_ino;
}

exports.sameFilesystem = function sameFilesystem(pathA, pathB)
{
  function filesystem(path)
  {
    var sb = new MutableStruct("struct stat");
    var a;

    do
    {
      if (_stat.call(path) == 0)
	return sb.st_dev;

      if (ffi.errno != dh.ENOENT)
	throw new Error("Could not determine filesystem for '" + path + "'" + syserr());

      if (!a)
	a = path.split('/');
      a.pop();
      path = a.join('/');
    } while(path.length);

    return null;
  }

  return filesystem(pathA) == filesystem(pathB);
}

/**
 *  Return the size of a file in bytes
 */
exports.size = function size(path)
{
  var sb = stat(path);

  if ((sb.st_mode & (dh.S_IFLNK | dh.S_IFREG)) == 0)
    throw new Error("Cannot determine size of '" + path + "' - not a regular file or link");

  return sb.st_size;
}

/**
 *  Returns the time that a file was last modified as a Date object.
 */
exports.lastModified = function lastModified(path)
{
  return new Date(stat(path).st_mtime * 1000);
}

/**
 *  List the names of all the files in a directory, in lexically sorted order. 
 *  Throws an Error if the directory is inaccessible or does not exist. 
 *  @note list("x") of a directory containing "a" and "b"  returns ["a", "b"], not ["x/a", "x/b"]. 
 */
exports.list = function list(path, sortArgument)
{
  var dirlist = [];
  var dent;
  var dirp;

  if (!exports.isDirectory(path))
    throw new Error("'" + path + "' is not a directory");
  
  if (!(dirp = _opendir.call(path)))
    throw new Error("Cannot open directory '" + path + "'" + syserr());

  while ((dent = _readdir.call(dirp)))
  {
    dent = ffi.MutableStruct("struct dirent", dent);
    dirlist.push(dent.d_name.asString(-1));
  }

  if (sortArgument)
    dirlist.sort(sortArgument);
  else
    dirlist.sort();

  return dirlist;
}

/**
 *  Return an iterator that lazily browses a directory, not backward, 
 *  only forward,  for the base names of entries in that directory. 
 */
exports.iterate = function iterate(path)
{
  if (!exports.isDirectory(path))
    throw StopIteration;

  if (!(dirp = _opendir.call(path)))
    throw new Error("Cannot open directory '" + path + "'" + syserr());

  while ((dent = _readdir.call(dirp)))
  {
    dent = ffi.MutableStruct("struct dirent", dent);
    yield dent.d_name.asString(-1);
  }
}

/**
 *  Permissions constructor. Inherits from Permissions.default and the passed argument.
 *
 *  @params	p	An object which is a duck-type of Permissions, used to template
 *			the constructor.
 */
function Permissions(p)
{
  function replicate(newObj, obj)
  {
    for (var el in obj)
    {
      switch(typeof(obj[el]))
      {
	case "undefined":
	  newObj[el] = (void 0);
	  break;
	case "number":
	case "string":
        case "boolean":
          newObj[el] = obj[el];
          break;
        case "object":
	  if (obj[el] == null)
            newObj[el] = null;
	  else
          {
            newObj[el] = {};
            replicate(newObj[el], obj[el]);
          }
          break;
        default:
          throw new TypeError(typeof(obj[el]) + " " + el + " doesn't belong in a Permissions object!");
      }
    }
  }

  replicate(this, Permissions.default);
  replicate(this, p);
}

/** Instance method to display a Permissions object as a traditional UNIX number. 
 *  Results should be displayed in octal when consumed by humans 
 *  @returns	Number
 */
Permissions.prototype.toUnix = function Permissions_instance_toUnix()
{
  var perms = 0;

  function boolsToNum(b)
  {
    var n = 0;

    if (b.read) n |= 4;
    if (b.write) n |= 2;
    if (b.execute) n |= 1;

    return n;
  }

  if (this.owner) perms |= boolsToNum(this.owner) << 6;
  if (this.group) perms |= boolsToNum(this.group) << 3;
  if (this.other) perms |= boolsToNum(this.other);

  return '0' + perms;
}

/** Static method to turn a traditional UNIX octal permission number
 *  into a Permissions-like object. 
 *  @param	u	The UNIX octal permissions (e.g 0664)
 *  @returns	Object which can duck-type as Permissions
 */
Permissions.fromUnix = function Permissions_static_fromUnix(u)
{
  function numToBools(n)
  {
    return { read: (n & 4) != 0, write: (n & 2) != 0, execute: (n & 1) != 0};
  }

  return { 
    owner: numToBools((u >> 6) & 7),
    group: numToBools((u >> 3) & 7),
    other: numToBools(u & 7)
  };
}

/** Static method to turn a traditional UNIX umask into a Permissions
 *  template object.
 *  @param	uu	The UNIX octal umask (e.g 0002)
 *  @returns	Object which can duck-type as Permissions
 */
Permissions.fromUnixUmask = function Permissions_static_fromUnixMask(uu)
{
  return Permissions.fromUnix(511 - (uu & 511)); /* 511 == 0777 */
}

/* Pull the current umask inline during module load, use to 
 * establish default permissions. From now on, umask will be
 * ignored unless default permissions are explicitly reset.
 */
var umask = _umask.call(0);
_umask.call(umask);
Permissions.default = Permissions.fromUnixUmask(umask);

/****** Simple Streams to test fs-base ******/

/**
 *  Stream constructor. Must specify either stream or fd. Can specify both.
 *  Not specifying either, or specifying parameters which conflict, is an error.
 *  Falsey values, except 0 for fd, are considered "not specified".
 * 
 *  @param      stream          gffi Memory object pointing to a C FILE *
 *  @param      fd              File descriptor number
 *  @param      mode            fs-base mode object
 */
function Stream(stream, fd, mode)
{
  if (stream)
    this.stream = stream;
  if (fd || fd === 0)
    this.fd = fd;
  if (mode)
    this.mode = eval(uneval(mode));
}

exports.Stream = Stream;	/* temporary, socket depends on this */

/** Generator method which yields lines from a Stream as ByteStrings.
 *
 *  @note	Inefficient, should be optimized to take advantage of ByteString COW capabilities.
 *  @param	encoding	Character encoding of file
 */
Stream.prototype.readlines = function Stream_readlines(encoding)
{
  if (encoding)
  {
    var line;
    var buf = new ffi.Memory(1024);

    while((line = _fgets.call(buf, buf.size, this.stream)))
    {
      line = binary.ByteArray(line, -1);
      yield line.decodeToString(encoding);
    }
  }
  else
  {
    var line;
    var buf = new ffi.Memory(1024);
 
    while ((line = _fgets.call(buf, buf.size, this.stream)))
    {
      var a = binary.ByteString(line, -1);
      a.memory = line;
      yield a;
    }
  }
}

Stream.prototype.readln = function Stream_readln()
{
  var buf = new ffi.Memory(1024);

  if (_fgets.call(buf, buf.size, this.stream))
    return buf.asString(-1);
  else
    return null;
}

Stream.prototype.writeln = function Stream_writeln(buffer)
{
  if (_fputs.call(buffer + "\n", this.stream) == dh.EOF)
    throw new Error("Cannot write to stream!" + syserr());
}

Stream.prototype.write = function Stream_write(buffer, encoding)
{
  var bytesWritten;

  switch(typeof(buffer))
  {
    case "object":
      if (buffer instanceof ffi.MutableStruct)
	buffer = gffi.Memory(buffer);
      if (buffer instanceof ffi.Memory)
      {
	buffer.length = buffer.size;
	break;
      }
      if ((buffer instanceof require("binary").ByteString) || (buffer instanceof require("binary").ByteArray))
	break;
    default:
      buffer = buffer.toString();
    case "string":
      buffer = new (require("binary").ByteString)(buffer, encoding);
      encoding = null;
      break;
  }

  if (encoding)
    throw new Error("Cannot pass encoding parameter when writing " + typeof(buffer) + " values");

  bytesWritten = _fwrite.call(buffer, 1, buffer.length, this.stream);
  if (bytesWritten != buffer.length)
    throw new Error("Could not write entire buffer; only wrote " + bytesWritten + " of " + buffer.length + " bytes!" + syserr());
}

Stream.prototype.read = function Stream_read(howMuch)
{
  var sb = stat(this);
  var bytesRead;
  var bytesLeft;
  var pos;
  var buffer;

  if ((pos = _ftello.call(this.stream)) == -1)
    throw new Error("Could not determine current stream offset!" + syserr());

  bytesLeft = sb.st_size - pos;

  if (typeof howMuch === "undefined" || howMuch > bytesLeft)
    howMuch = bytesLeft;

  buffer = new ffi.Memory(howMuch);

  bytesRead = _fread.call(buffer, 1, howMuch, this.stream);
  if (bytesRead != buffer.size)
    throw new Error("Could not read enough to fill buffer; only read " + bytesRead + " of " + buffer.size + " bytes!" + syserr());

  buffer.toString = buffer.asString;

  return buffer;
}

Stream.prototype.close = function Stream_close()
{
  if (this.stream)
    this.stream.destroy();
  else
    _close.call(this.fd);
}


