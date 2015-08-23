# Introduction #

This page is under construction.

# Globals #

  * "the context's global" means the JSObject returned by JS\_GetGlobalObject()
  * "the super global" means the JSObject which is at the end of the scope chain: standard classes are described here
  * "the module global" means the JSObject which serves as the var object (scope) for a module
  * The context's global is usually the super global, but during module load/initialization it could be the module global
  * From a module's POV, the super-global is not technically part of the scope chain, due to implementation details of the Tracing JIT. However, we proxy unfound properties from the module global to the super-global for resolution, giving the effect that it is.
  * Corollary - don't use global variables for frequently-accessed-from-a-module data in performance critical code