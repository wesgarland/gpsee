#define _xstr(s) _str(s)
#define _str(s) #s
#define GPSEE_MAJOR_VERSION_NUMBER              0
#define GPSEE_MINOR_VERSION_NUMBER              2
#define GPSEE_MICRO_VERSION_NUMBER              0
#define GPSEE_PATCHLEVEL                        "rc1"
#define GPSEE_CURRENT_VERSION_STRING		_xstr(GPSEE_MAJOR_VERSION_NUMBER) "." _xstr(GPSEE_MINOR_VERSION_NUMBER) "." _xstr(GPSEE_MICRO_VERSION_NUMBER) "-" GPSEE_PATCHLEVEL
