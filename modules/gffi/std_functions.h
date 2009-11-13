#include "gpsee_config.h"

#if defined(GPSEE_STD_SUSV3)
# define _XOPEN_SOURCE 600
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
#endif

#if defined(GPSEE_STD_SUSV2)
# define _XOPEN_SOURCE 500
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
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

/* Pull feature-detection code in with unistd / stdlib */
#include <unistd.h>
#include <stdlib.h>

/* Enable POSIX realtime and thread extensions if OS standard supports them */
#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
# define GPSEE_STDEXT_POSIX_REALTIME
#endif
#if defined(_POSIX_SOURCE) && (_POSIX_C_SOURCE >= 199506L)
# define GPSEE_STDEXT_POSIX_THREADS
# if !defined(_REENTRANT)
#  define _REENTRANT
# endif
#endif

/* Sanity checking */
#if !defined(GPSEE_STD_P92) && !defined(GPSEE_STD_U95) && !defined(GPSEE_STD_SUSV3) && !defined(GPSEE_STD_C99) && !defined(GPSEE_STD_SVID) && !defined(GPSEE_STD_BSD)
# warning "Could not figure out what kind of C standard your platform supports"
#endif

/* Make sure we get right iconv.h */
#include "gpsee_iconv.h"

#include <stdio.h>
#include <sys/types.h>
#include <syslog.h>
#include <assert.h>
#include <setjmp.h>
#include <langinfo.h>
#include <ulimit.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <termios.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/stat.h>
#include <net/if.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <aio.h>
#include <libgen.h>
#include <strings.h>
#include <string.h>
#include <nl_types.h>
#include <sys/types.h>
#include <sys/types.h>
#include <stdlib.h>
#include <grp.h>
#include <netdb.h>
#include <utmpx.h>
#include <fmtmsg.h>
#include <fnmatch.h>
#include <sys/ipc.h>
#include <ftw.h>
#include <ucontext.h>
#include <grp.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <glob.h>
#include <search.h>
#include <arpa/inet.h>
#include <math.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <spawn.h>
#include <poll.h>
#include <pthread.h>
#include <sys/uio.h>
#include <regex.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <pwd.h>
#include <sys/shm.h>
#include <monetary.h>
#include <wchar.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <wordexp.h>
#include <complex.h>
#include <fenv.h>
#include <utime.h>

/* Pull in ndbm, picking out the right implementatiuon
 * on Linux and resolving macro collisions with POSIX 
 */
#if defined(_GNU_SOURCE)
#define FIND FIND_RENAMED
#define ENTER ENTER_RENAMED
#define ACTION ACTION_RENAMED
#define ENTRY ENTRY_RENAMED
#define entry entry_RENAMED
#include <db.h>
#undef FIND
#undef ENTER
#undef ACTION
#undef ENTRY
#undef entry
#else
#include <ndbm.h>
#endif

/* statvfs interfaces on SVR4 have hidden deps */
#if defined(__svr4__)
# if defined(GPSEE_HPUX_SYSTEM)
#  include <sys/fstyp.h>
# else
#  include <sys/fstyp.h>
#  include <sys/fsid.h>
# endif
#endif
#include <sys/statvfs.h>

