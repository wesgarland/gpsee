#!/usr/bin/env python

#
# Code to generate JS properties for posix errno macros ("EGAIN" -> 9)
#  and a function mapping the errno back to it's macro name
#  9 -> "EAGAIN"
#
# Nick Galbreath
# Copyright 2010
# MIT License
# http://www.opensource.org/licenses/mit-license.php

errnos = [
    'EPERM', 'ENOENT', 'ESRCH', 'EINTR', 'ENXIO', 'E2BIG', 'ENOEXEC', 'EBADF',
    'ECHILD', 'EAGAIN', 'ENOMEM','EACCES', 'EFAULT', 'ENOTBLK', 'EBUSY',
    'EEXIST', 'ENODEV', 'ENOTDIR', 'EISDIR', 'EINVAL', 'ENFILE', 'EMFILE',
    'ENOTTY', 'ETXTBSY', 'EFBIG', 'ENOSPC', 'ESPIPE', 'EROFS', 'EMLINK',
    'EPIPE', 'EDOM', 'ERANGE', 'EDEADLK', 'ENAMETOOLONG', 'ENOLCK', 'ENOSYS',
    'ENOTEMPTY', 'EWOULDBLOCK', 'ENOMSG', 'EIDRM', 'ECHRNG', 'EL2NSYNC',
    'EL3HLT', 'ELRST', 'ELNRNG', 'EUNATCH', 'ENOCSI', 'EL2HLT', 'EBADE',
    'EBADR', 'EXFULL', 'ENOANO', 'EBADRQC', 'EBADSLT', 'EDEADLOCK', 'EBFONT',
    'ENOSTR', 'ENODATA', 'ETIME', 'ENOSR', 'ENONET', 'ENOPKG', 'EREMOTE',
    'ENOLINK', 'EADV', 'ESRMNT', 'ECOMM', 'EPROTO', 'EMULTIHOP', 'EDOTDOT',
    'EBADMSG', 'EOVERFLOW', 'ENOTUNIQ', 'EBADFD', 'EREMCHG', 'ELIBACC',
    'ELIBBAD', 'ELIBMAX', 'ELIBEXEC', 'EILSEQ', 'ERESTART', 'ESTRPIPE',
    'EUSERS', 'ENOTSOCK', 'EDESTADDRREQ', 'EMSGSIZE',
    'EPROTOTYPE','ENOPROTOOPT', 'ESOCKTNOSUPPORT', 'EOPNOTSUPP',
    'EPFNOSUPPORT', 'EAFNOSUPPORT', 'EADDRINUSE','EADDRNOTAVAIL', 'ENETDOWN',
    'ENETUNREACH', 'ENETRESET',
    'ECONNABORTED', 'ECONNRESET', 'ENOBUFS', 'EISCONN', 'ENOTCONN',
    'ESHUTDOWN',
    'ETOOMANYREFS', 'ETIMEDOUT', 'ECONNREFUSED', 'EHOSTDOWN', 'EHOSTUNREACH',
    'EALREADY', 'EINPROGRESS', 'ESTALE', 'EUCLEAN', 'ENOTNAME', 'ENAVAIL',
    'EISNAME', 'EREMOTEIO', 'EDQUOT', 'ENOMEDIUM', 'EMEDIUMTYPE', 'ECANCELED',
    'ENOKEY', 'EKEYEXPIRED', 'EKEYREVOKED', 'EKEYREJECTED',
    'EOWNERDEAD', 'ENOTRECOVERABLE'
    ]

print "static JSConstDoubleSpec errno_constants[] = {"
for e in errnos:
    print """#ifdef %s
{%s, "%s", 0,{0,0,0}},
#endif""" % (e,e,e)
print "{0,0,0,{0,0,0}}"
print "};"

print "static const char* errno2name(int e) {"
print "switch (e) {"
for e in errnos:
    if e == 'EWOULDBLOCK':
        # some platforms define aliases -- want only one
        print "#if EWOULDBLOCK && (EWOULDBLOCK != EAGAIN)"
    elif e == 'EDEADLOCK':
        # some platforms define aliases -- want only one
        print "#if EDEADLOCK && (EDEADLOCK != EDEADLK)"
    else:
        print "#ifdef %s" % e
    print 'case %s: return "%s";\n#endif' % (e,e)
print "default: return NULL;";
print "}"  #end switch
print "}"  # end function
