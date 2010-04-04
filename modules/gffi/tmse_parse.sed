/./h
s/\"/\\\\\\\"/g
/^\(#define \)\([^ ][^ ]*\)\(  *\)\([A-Za-z0-9_]*\)$/b simple
:complex
s/^\(#define \)\([^ ][^ ]*\)\(  *\)\(.*\)$/puts("haveExpr(\2,\\"\4\\")");/
/./p
/./d
:simple
s/^\(#define \)\([^ ][^ ]*\)\(  *\)\([A-Za-z0-9_]*\)$/#ifdef \4/
/./p
/./g
s/^\(#define \)\([^ ][^ ]*\)\( *\)\(.*\)/puts("haveAlias(\2, \4)");/
/./p
/./g
/./i\
#else 
s/^\(#define \)\([^ ][^ ]*\)\( *\)\(.*\)$/puts("haveExpr(\2,\\"\4\\")");/
/./a\
#endif
