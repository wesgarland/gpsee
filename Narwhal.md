# Introduction #

Narwhal is a cross-platform, multi-interpreter, general purpose JavaScript platform. It aims to provide a solid foundation for building JavaScript applications, primarily outside the web browser. Narwhal includes a package manager, module system, and standard library for multiple JavaScript interpreters. Currently Narwhal's Rhino support is the most complete, but other engines are available too.

Narwhal's standard library conforms to the CommonJS standard. It is designed to work with multiple JavaScript interpreters, and to be easy to add support for new interpreters. Wherever possible, it is implemented in pure JavaScript to maximize reuse of code among engines.

Combined with Jack, a Rack-like JSGI compatible library, Narwhal provides a platform for creating server-side JavaScript web applications and frameworks such as Nitro.

# Details #

Homepage:

  * http://narwhaljs.org/

Source & Download:

  * http://github.com/tlrobinson/narwhal/