#include "std_functions.h"
#include "float.h"

#define xstr(s) str(s)
#define str(s) #s

int main(int argc, const char *argv[])
{
#define declareMacroConst(a)			\
  printf("#undef " #a "\n");                    \
  if (strcmp(#a, xstr(a)) != 0)			\
  {                                             \
    printf("#define " #a " " xstr(a) "\n");     \
  }                                             \
  else                                          \
  {                                             \
    if (a < 0)                                  \
      printf("#define " #a " %lld\n", (long long signed int)a);	\
    else                                        \
      printf("#define " #a " %llu\n", (long long unsigned int)a);	\
  }
#include "std_macro_consts.decl"
#undef declareMacroConst

 return 0;  
}

