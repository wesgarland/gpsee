s/^\(#define \)\([^ ][^ ]*\)\(.*\)/  printf("haveString(\2,\\\"%s\\\")\\n",(\2));/
