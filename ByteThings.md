# Introduction #

A **byteThing** in GPSEE is a specialized native data type.  From script, they are interesting because they represent 8-bit data (in only 8 bits per byte) and are interchangeable in many circumstances. These characterstics make them suitable for implementing low-level interfaces with C code, I/O, and other "close to the metal" operations.

From the C API perspective, this data type has a private slot struct with members buffer, length, btFlags, and memoryOwner.
  * **buffer**: This is a block of memory which directly reflects (somehow) into script. There is no guarantee that this block was allocated in any particular way, just that it is usable.  This buffer address can change at any time when the byteThing is mutable.
  * **length**: This is how much of the memory which is in use.  In cases where we do not know the length (i.e. a plain pointer came from a C library), we use 0 as the length.
  * **btFlags**: A bitmask which describes specific attributes of the byteThing.  Currently, the only attribute is immutability (bt\_immutable).
  * **memoryOwner**: A JSObject pointer to the object which actually owns the memory.  That object will know to release buffer during finalization.  The usual case is that JS\_GetPrivate(cx,obj)->memoryOwner == obj, however it will not always be true in the case where we have immutably-cast objects, boxed objects, or are dealing with a function call() or apply().

## Currently implemented ByteThings ##
### binary module ###
  * ByteString  (immutable)
  * ByteArray
### gffi module ###
  * Memory
  * MutableStruct
  * _ImmutableStruct (immutable) (not implemented, currently an alias for MutableStruct)_

# Mutability #

ByteThings are considered mutable by default.  This means that, from script, you are not guaranteed the same answer for the same property access if you have run any code between the property accesses.

Immutable byteThings _do_ guarantee identical property access will yield equivalent results (there is no guarantee that they will be equal).  Imutable byteThings also agree to never modify buffer, buffer's contents, length, btFlags, or memoryOwner.

# Casting #

Casting in GPSEE refers to the act of reinterpreting underlying data and presenting it with a new JavaScript interface.  It is subtly different from a C cast and somewhat similar to a C++ reinterpret-cast.

Casting is performed by using a byteThing constructor as a function.  Casting to or from a mutable byteThing will always perform a memory-level copy. Casting to and from an immutable byteThing allows an internal optimization which re-uses the backing store. This closely resembles a C cast and is a very inexpensive operation.

Casting from other JavaScript types is dependent on the casting function.  Generally, instances of String, the null object, and maybe numbers will be special-cased. Strings will normally be converted to C character arrays (UTF-8 encoded when GPSEE is UTF-8 aware), and the resultant character array will be treated as a byteThing buffer.  The null object will often be treated as the C NULL pointer. Classes like Memory(), which wrap malloc(), will treat numbers as pointers. **Note:** not all pointers can be presented by numbers; however, -1 and and 0 are safe).

# call() / apply() #

These two JavaScript functions allow a method to be executed on a foreign object.  During a call to call() or apply(), we essentially have a period of limited immutability, and so can relax the mutable copying rules.  For example, a method from an immutable byteThing can be applied to a mutable byteThing; for example, to read C data encoded in UTF-8 and return it as a UTF-16 JavaScript String.

Many byteThing methods are available to other byteThings via .call and .apply.  Any method which
  1. does not allow us to violate the mutability rules after the function call ends (i.e. by returning memory from an immutable byteThing)
  1. reads only the four byteThing private slot struct properties (buffer, length, btFlags, memoryOwner)
  1. writes only into buffer
is a candidate for call() and apply().

**Note:** call() and apply() are not safe to call when two threads of JavaScript, acting on the same mutable objects, are in play.  The JavaScript programmer must provide external serialization (i.e. with a mutex) in order to safely using call() and apply() on mutable byteThings.