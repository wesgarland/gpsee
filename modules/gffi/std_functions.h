#include "gpsee_config.h"
#include "gpsee-iconv.h"

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
#include <sys/socket.h>
#include <net/if.h>
#include <sys/wait.h>
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
#if defined(HAVE_NDBM)
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

/* Solaris 10 is missing these definitions */
#if defined(GPSEE_SUNOS_SYSTEM)
extern size_t confstr(int, char *, size_t);
#endif
