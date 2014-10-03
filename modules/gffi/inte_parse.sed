s/  */ /g
s/^\(#define \)\([^ ][^ ]*\)\(.*\)/\
#ifdef \2\
  printf("haveInt(\2,"); \
  printf(((\2 < 0) ? "%lld,1," GPSEE_SIZET_FMT ")\\n":"%llu,0," GPSEE_SIZET_FMT ")\\n"),(long long)(\2),sizeof(\2));\
#else\
# warning "Missing integer define \2"\
#endif/