#include <stdio.h>

static const char *aux_types[]={
#define aux_type(t)	#t,
#include "aux_types.decl"
#undef aux_type
};

int main(int argc, char **argv)
{
  int	i;

  for (i=0; i < sizeof(aux_types) / sizeof(aux_types[0]); i++)
  {
    printf(
	"#define FFI_TYPES_INTEGERS_ONLY\n"
	"#define ffi_type(ftype, ctype)\\\n"
	"if ((sizeof(%s) == sizeof(ctype)) && \\\n"
	"    ((%s)-1 < 0) == ((ctype)-1 < 0)) {\\\n"
	"  if (JS_DefineProperty(cx, moduleObject, \"%s\", jsve_ ## ftype, \\\n"
	"                         NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY) == JS_FALSE) \\\n"
	"    return NULL;\\\n"
	"  else goto done_%s; \\\n"
	"}\n"
	"#include \"ffi_types.decl\"\n"
	"#undef ffi_type\n"
	"#undef FFI_TYPES_INTEGERS_ONLY\n"
	"  done_%s:\n",
	aux_types[i], aux_types[i], aux_types[i], aux_types[i], aux_types[i]
	);
  }

  return 0;
}
