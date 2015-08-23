
```
diff --git a/js/src/configure.in b/js/src/configure.in
--- a/js/src/configure.in
+++ b/js/src/configure.in
@@ -1708,18 +1708,20 @@ case "$target" in
     esac
     ;;
 
 *-darwin*) 
     MKSHLIB='$(CXX) $(CXXFLAGS) $(DSO_PIC_CFLAGS) $(DSO_LDOPTS) -o $@'
     MKCSHLIB='$(CC) $(CFLAGS) $(DSO_PIC_CFLAGS) $(DSO_LDOPTS) -o $@'
     MOZ_OPTIMIZE_FLAGS="-O3"
     _PEDANTIC=
-    CFLAGS="$CFLAGS -fpascal-strings -fno-common"
-    CXXFLAGS="$CXXFLAGS -fpascal-strings -fno-common"
+    if $CC --version 2>&1 | grep 'gcc version.*Apple.*Inc' >/dev/null; then
+      CFLAGS="$CFLAGS -fpascal-strings -fno-common"
+      CXXFLAGS="$CXXFLAGS -fpascal-strings -fno-common"
+    fi
     DLL_SUFFIX=".dylib"
     DSO_LDOPTS=''
     STRIP="$STRIP -x -S"
     _PLATFORM_DEFAULT_TOOLKIT='cairo-cocoa'
     TARGET_NSPR_MDCPUCFG='\"md/_darwin.cfg\"'
     LDFLAGS="$LDFLAGS -framework Cocoa"
     # The ExceptionHandling framework is needed for Objective-C exception
     # logging code in nsObjCExceptions.h. Currently we only use that in debug
```