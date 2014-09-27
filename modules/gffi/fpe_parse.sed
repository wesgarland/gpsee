s/  */ /g
s/^\(#define \)\([^ ][^ ]*\)\(.*\)/  printf("haveFloat(\2,%100e," GPSEE_SIZET_FMT ")\\n",(\2),sizeof(\2));/

