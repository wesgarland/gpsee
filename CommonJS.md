# Introduction #

CommonJS is JavaScript ecosystem for web servers, desktop and command line apps and in the browser.

  * http://wiki.commonjs.org/wiki/CommonJS
  * http://wiki.commonjs.org/wiki/Implementations/GPSEE

# CommonJS in GPSEE #

GPSEE implements the following CommonJS features:

| **Feature** | **Summary** | **Specification** |
|:------------|:------------|:------------------|
| require     | securable modules loader | http://wiki.commonjs.org/wiki/Modules/1.1 |
| [binary/b](ModuleBinaryB.md) | ByteString, ByteArray | http://wiki.commonjs.org/wiki/CommonJS/Binary/B |
| [fs-base](ModuleFSBase.md) | Base-level filesystem module | http://wiki.commonjs.org/wiki/Filesystem/A/0 |
| [system](ModuleSystem.md) | System module | http://wiki.commonjs.org/wiki/System/1.0 |