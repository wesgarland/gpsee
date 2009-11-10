#include "gpsee_config.h"
//#define GPSEE_STD_SUSV3

#if defined(GPSEE_GNU_SOURCE)
# define GPSEE_STD_SOURCE
# define GPSEE_STD_SUSV3
#endif

#if defined(GPSEE_STD_P92)
# define _POSIX_SOURCE
# define _POSIX_C_SOURCE 2
#endif

#if defined(GPSEE_STD_U95)
# define _POSIX_SOURCE
# define _POSIX_C_SOURCE 199506L
#endif

#if defined(GPSEE_STD_SUSV3)
# define _XOPEN_SOURCE 600
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
#endif

#if defined(GPSEE_STD_SUSV3)
# define GPSEE_STD_C99
#endif

#if defined(GPSEE_STD_C99)
# define _ISOC99_SOURCE
# define _ISOC9X_SOURCE
#endif

#if defined(GPSEE_STD_SVID)
# define _SVID_SOURCE
#endif

#if defined(GPSEE_STD_BSD)
# define _BSD_SOURCE
#endif

#include <unistd.h>

#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
# define GPSEE_STDEXT_POSIX_REALTIME
#endif
#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199506L)
# define GPSEE_STDEXT_POSIX_THREADS
# if !defined(_REENTRANT)
#  define _REENTRANT
# endif
#endif

#if !defined(GPSEE_STD_P92) && !defined(GPSEE_STD_U95) && !defined(GPSEE_STD_SUSV3) && !defined(GPSEE_STD_C99) && !defined(GPSEE_STD_SVID) && !defined(GPSEE_STD_BSD)
# warning "Could not figure out what kind of C standard your platform supports"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#if defined(GPSEE_SUNOS_SYSTEM)
# include <sys/fstyp.h>
# include <sys/fsid.h>
#endif
#if defined(GPSEE_HPUX_SYSTEM)
# include <sys/fstyp.h>
# include <sys/fsid.h>
#endif
#include "std_headers.h"
#include <syslog.h>
#include <assert.h>
#include <crypt.h>
#include <stropts.h>
#include <setjmp.h>
#include <langinfo.h>
#include <ndbm.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <wchar.h>
#include <wctype.h>
