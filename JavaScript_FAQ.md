# Introduction #

There are a few questions about JavaScript which seem to come up time and again on the Internet.  While this page has little or nothing to do with GPSEE, they are relevant to those using it.

This document is geared toward intermediate JavaScript programmers, particularly those who have significant experience with other programming languages. We do not discuss every feature of the language; rather those features which tend to confuse people.

# Variable Scopes #
```
var b=2;
{
  var b = 1;
}
print(b);
```

**Q:** Output is 1. Why?

&lt;BR&gt;


**A:** JavaScript variable scopes are function-wide. Curly braces are used only to group statements. It is not an error to use the `var` statement more than once in a function.

If you are a C programmer and wrote the above, here is what you probably meant to write:
```
var b=2;
(function ()
{
  var b = 1;
})();
print(b);
```
(Output is 2)

Of course, that looks horrible, and isn't really a great JavaScript idiom as presented.

What is shown is a function expression which is immediately executed. Since variable scope is function-wide in JavaScript, using the `var` statement gives us a new variable b to write to.

Remember, though, without the `var` statement variables are read up the scope chain:
```
var b=2; 
(function () 
 { 
   b = 1; 
 })(); 
print(b); 
```
(Output is 1)

But, surprisingly, the location of the `var` statement within the function block is irrelevant:
```
var b=2; 
(function () 
 { 
   b = 1; 
   var b; 
 })(); 
print(b); 
```
(Output is 2)

# Types #

There are only six types in JavaScript (seven in E4X):
  1. boolean
  1. number
  1. string
  1. undefined
  1. function
  1. object
  1. xml (E4X only)

There are multiple aspects by which one might infer "type". Strictly, there are three primitive immutable types: number, boolean, and string. Then there are first-class functions and objects, of which there are as many varieties as you wish to invent.

You cannot make new types in JavaScript. You can create something like "classes" in other languages, but their instances will always have typeof object. These so-called "classes" are really just functions with a prototype property, which provide templates (prototypes) and mechanisms (constructors) for instanciating objects. Arrays, dates, regular expressions -- these are all objects.

To find the type of something, use the `typeof` operator; it yields a string.

## boolean ##
The keywords `false` and `true` are the only boolean literals.

## number ##
Numbers in JavaScript are the set of 64-bit IEEE-748 floating point numbers. This is a range about the same size as a signed 53-bit integer.

Some engines, like SpiderMonkey, can represent smaller integral numbers internally with integer data types, but this is not required by the specification and this abstraction does not leak out at the JavaScript programmer. SpiderMonkey is optimized for 31-bit integers.

There are three more numbers as well: `NaN` ("not a number"), `+Infinity` (largest number) and `-Infinity` (smallest number). `+Infinite` may written without the leading plus.

Positive and Negative zero in JavaScript are sometimes treated as distinct numbers, particularly by the Math object. You can think of `-0` and `+0` as numbers on a line which approach the origin from either the left or the right. Writing `0` implies positive zero.

## string ##
Strings in JavaScript are interned, immutable vectors of unsigned 16-bit "characters". A String literal looks like `"hello"` or `'world'`. String escape sequences, such as `\u0123` and `\n` have been borrowed from Java to allow the JavaScript programmer to insert characters into a String literal which might otherwise be difficult to type, or not allowed by the JavaScript compiler.

Any two identical String literals are strictly equal.

## function ##
Functions are the basic building blocks of JavaScript - it is, after all, a functional programming language.

Functions encapsulate re-useable blocks of code and variable scope. You can think of a function declaration like a function literal (or function expression).  Variables can be assigned function literals, just like any other kind of literal (boolean, number, string, etc).

```
var hello = function() { 
  print("hello"); 
}

function world() {
  print("world");
}

hello();
world();
```

LISP users will recognize immediately that JavaScript functions can be used as lambda functions; unnamed functions which are used inline in code, often as arguments to other functions:
```
quicksort(myList, 
  function(a,b) { 
    return a < b;
  }
); 
```

Mozilla has also extended the function declaration syntax to allow for tidyier lambda functions, referred to as _expression closures_. If you omit the opening brace, the next expression is taken as the entire content of the function, and its return value:
```
quicksort(myList, function(a,b) a < b);
```
Adding extra parenthesis here can improve readability, and score extra points with the LISP crowd:
```
quicksort(myList, (function(a,b) a < b));
```

Functions in JavaScript can also have properties. In this way, functions are a lot like objects which happen to be callable, however, there is no syntax for declaring a function literal with arbitrary properties (or an object literal which is callable).

## object ##

Objects in JavaScript are essentially arbitrary collections of properties. Each property will have a value of one of the six types listed above.  Users used to other programming languages might wonder about methods -- this is a false dichotomy.  A property which has a type of function _is_ a method.

JavaScript properties can be accessed either with the common dot notation, or with a string argument inside square brackets (like associative arrays from other languages). These statements are equivalent:

```
print(myObject.prop);
print(myObject["prop"]);
```

Although the first statement tends to be more convenient and readable for the programmer, the second is far more powerful: any expression yielding a string can be used to select properties.

Object property lists are very flexible in JavaScript. As with variables, the type of any property is not fixed; additionally, any object can have properties added to it, or taken away, at any time. Properties can be added simply by assigning to them, and removed with the `delete` operator.

There are four types of object-literals: "regular" object literals, array literals, regular expressions, and the keyword null.

To declare a "regular" object literal, construct a braced expression like so:
```
var myObject = {
  myProp1: "someValue",
  myProp2: someVariablesValue,
  "myProp3": "anotherValue"
  myMethod: function() { print("hello, world"); }
};
```
The statement above declares an object with the `Object.prototype` prototype, and four properties (one of which is a function, i.e. a method). `Object.prototype` is normally an empty object, however JavaScript programmers have been known to change that.

To declare an Array object literal, construct a comma-delimited list surrounded by square brackets:
```
var myArray = [ 0, 1, 2,,,"five" ];
```
The statement above declares an object with the `Array.prototype` protoype, and six properties named "0", "1", "2", "3", "4", and "5".

Two of those properties have values which are undefined, three are Numbers, and one is a String. `Array.prototype`, in turn, provides the properties (like `length`) which differentiate instances of Array from other objects.

To declare a RegExp object literal, surround a regular expression with forward-slashes:
var myRegExp = /[a-z0-9]**/;**

## undefined ##
This is the type of any variable or property which has not been defined. It is also the type of an expression which has been prefixed with the word `void`.

There is a global variable named `undefined` which has the type undefined when the interpreter initializes a fresh context.  However, this variable can be reassigned and as such shouldn't really be used. To find out if a variable or property is undefined, you must explicitly use the typeof operator.

To make a variable or property which is defined have the type undefined, take advantage of the `void` operator, and assign a "value" of `void 0`.  Alternatively, you can use the `delete` operator.

Note that these two techniques are subtly different, because JavaScript has two concepts of undefinedness.  The first technique, assigning a void value, yields a property which is defined and a value which is undefined. The second technique, deleting the property, makes property itself undefined.

In practice, these are often interchangeable, but there is an important difference -- de-referencing a property which is defined to have an undefined value does not generate a strict-mode warning, whereas accessing an undefined property does.

## xml ##

This type is only used by E4X, ECMA-357. This language is an imperfect superset of JavaScript, and allows XML literals. While E4X is supported by GPSEE, you're on your own with this one. But, here's a taste:

```
var name = "Wes";
var document = <xhtml><body>Hello, {name}</body></xhtml>;
print(document);
```

# Inheritance #

Forget everything you ever learned about classical inheritance when you attended Java U -- JavaScript doesn't do that.  JavaScript uses _prototypal inheritance_, which is a shallow inheritance system based on templating object instances from an object prototype.

Combined with weak-typing, prototypal inheritance can be very powerful and easy to use when properly understood. When writing JavaScript programs, developers should not be concerned whether a first-baseman is a baseball player, is an athlete, is a human, is a mammal is an animal. The JavaScript programmer only cares whether or not he can catch a ball.

Prototypal inheritance is powerful and expressive enough to implement a variety of other inheritance patterns, including classical inheritance. Many programmers coming to JavaScript from languages like Java and C++ will often think that implementing their own inheritance scheme is a good idea. They are almost always wrong.

## Creating New Kinds of Objects ##
To declare a new kind of object in JavaScript (i.e. a "class"), we declare a function. A reference to this function is effectively a reference to this class.

When called with the `new` operator, this function acts as the constructor -- it is responsible for initializing the internal state of the new object (instance). References to the keyword `this` when the constructor function was called with `new` actually refer to the new object we're creating.

It is possible to call the constructor without the `new` operator -- i.e., to treat it as a plain function.  Doing so will cause `this` to refer to something other than a new object instance (usually the global object, unless another `this` exists on the prototype chain), and can be quite confusing. This practice should be avoided.

The constructor also provides a convenient parking place for the `.prototype` object, which provides a sort of template for object instances. A reference to a property which is not defined directly on the object (either by assignment to the object user or during construction) will be looked up on the prototype and used if found.

## ownProperties ##
When a property is defined directly on the object itself -- either by direct assignment or by assignment to `this` during construction -- we say that this property is an _ownProperty_. Other properties come from the prototype.

Mozilla's extensions to JavaScript and the [ES5](ES5.md) draft specification both provide `hasOwnProperty` methods on Object.prototype which can be used to tell whether a property is an ownProperty or not from JavaScript

## Methods ##
**Methods of object instances are simply properties which happen to be functions.** When a method is called, a special variable named `this`, which refers to precisely that instance, will be present in its scope. It does not matter whether the function was "found" as an ownProperty or was on the prototype. Or, for that matter, if the JavaScript programmer "jammed" the method on another object somehow, like by copying methods from one prototype to another, or by using the `call` or `apply` keywords.

## Simple Example: Ball class ##
```
function Ball(diameter) {
  this.radius = diameter / 2;

  this.calcDiameter = function() {
    return this.radius * 2;
  }
}

var baseball = new Ball(3.5);
baseball.colour = "white";

var basketball = new Ball(10);
basketball.colour = "orange";

var volleyball = new Ball(8);

Ball.prototype.calcVolume = function()
{
  return 4 * Math.PI * this.radius * this.radius * this.radius;
}

Ball.prototype.toString = function Ball_toString()
{
  var desc = "ball which is " + this.radius + " inches in radius";

  if (this.colour)
    return this.colour + " " + desc;
  else
    return desc;
}

print("a baseball is a " + baseball);
print("a basketball is a " + basketball);
print("a volleyball is a " + volleyball);
print("a baseball occupies " + baseball.calcVolume() + " cubic inches");
```

Output:
```
a baseball is a white ball which is 1.75 inches in radius
a basketball is a orange ball which is 5 inches in radius
a volleyball is a ball which is 4 inches in radius
a baseball occupies 67.34789251133118 cubic inches
```

This example shows several useful things
  * Creating an object instance with `new`
  * Assignment to ownProperty radius via the constructor
  * Addition of ownProperty colour by direct assignment
  * Addition of properties to the prototype work, _even after instance construction_ (calcVolume)
  * Poor coding style for declaration of calcDiameter (each instance gets its own copy of the function)

# Polymorphism #

JavaScript does not have function overloading based on argument types like C++ or Java, however it still possible (and useful) to implement polymorphic functions.

By virtue of weak-typing, polymorphism can seamless and automatically with well-designed objects and functions.

```
function Skier(team)
{
  this.glove = { colour: team.colours[0] };
}

function BaseballPlayer()
{
  this.glove = { colour:  "tan" };
}

function isFancyGloveColour(person)
{
  if (!person.glove || !person.glove.colour)
    return false;

  switch(person.glove.colour)
  {
    case "black":
    case "white":
    case "tan":
      return false;
  }

  return true;
}

var teamCanada = { colours: [ "red", "white" ];
var kucera = new Skier(teamCanada);
var scutaro = new BaseballPlayer();

print("John Kucera " + (isFancyColour(kucera) ? "has" : "does not have") + "fancy gloves");

print("Jay Scutaro " + (isFancyColour(scutaro) ? "has" : "does not have") + "a fancy glove");
```

Here we have implemented a function named isFancyGloveColour() which can operate any object containing a glove.colour property. This is sometimes called parametric polymorphism or generic programming -- and is the key concept behind JavaScript polymorphism: we care only about the properties that we're interested in at the moment.

## Variadic Functions ##

Parameters passed to JavaScript functions can be referred to by their names, or by examining an array-like object named "arguments".  The arguments object has a length property, and properties numbered `0` through `n-1` when `n` arguments are passed.

```
function hello()
{
  for (var i=0; i < arguments.length i++)
   print("argument #" + (i+1) + " is: " + arguments[i]);
}
```

# Iteration and Enumeration #

JavaScript objects generally have enumerable properties, and ownProperties do not affect enumeration. In ES3, it is not possible to make non-enumerable properties from script, although ES5 may change this. This means that, generally, you can enumerate all properties of objects which are implemented in pure JavaScript, but that native objects may choose to not enumerate certain methods. Not enumerating certain properties is often a wise design choice; for example, when iterating over an object which is an instanceof Array, the programmer expects to iterate over the elements of the array, and not the length property or the Array methods (like `push`, `pop`, `slice`, and `join`).

The easiest way iterate over the enumerable properties of an object is to use a `for-in` loop. The names of the properties will be returned in the enumeration:

```
var propName;

for (propName in myObject) {
  print("myObject has a property named " + propName 
        " whose value is " + propName[myObject]);
}
```

E4X (and thus GPSEE) adds another enumerator, `for-each-in`, which returns the property values rather than their names. `for-each` is expected to appear officially in a later version of the ECMAScript specification.

```
var propVal;

for each (propVal in myObject) {
  print("myObject has a property whose value is " + propVal);
}
```

## Generators ##

Mozilla-based JavaScript engines (SpiderMonkey, Rhino) have iterator generators which are very powerful, and will look familiar to the Python crowd. These generators are functions (which usually loop) and use the `yield` operator to yield a value to the consumer. This yield value is computed on-demand.

```
function count() {
  for (var i=0; i < 5; i++)
    yield i * 2;
}

for (a in count()) 
  print(a);
```

Output:
0
2
4
6
8

# Equality #

JavaScript has two concepts of equality. One uses symbols you are familiar with and semantics which surprise you. The other uses symbols which surprise you and semantics you are familiar with. These surprises are unfortunate, but a result of the language's evolution: Netscape "fixed" them in JavaScript 1.2, and it broke the web, so 1.3 went back to what we have today.

| **symbol** | **meaning** |
|:-----------|:------------|
| ==         | equivalent  | ===         | strictly equal |
| !=         | not equivalent | !==         | not strictly equal |

The equilvant/not equivalent operators (called equal and not equal in most literature) do type coercion based on the type of the left-hand operand and yield a booleant indicating whether the coerced objects are equal or not.

The strict equivalence operators, on the other hand, tell you if the operands are indistinguishable.  If the operands are objects or functions, this means that they are not just alike, it means that they are _the exact same object_.

# Coercion #

When JavaScript does comparisons with the equivalence operators (== and !=), the right-hand expression is coerced to match the left-hand expression, when the left-hand expression is a defined intrinsic type (string, boolean, number).

When the left-hand expression is a non-intrinsic type (or undefined), no coercion is performed, and no non-intrinsic types are considered equivalent in JavaScript. This means that `{} == {}` yields false, even though the objects appear to be the same.  The correct way to compare objects is with the equals method.

**Note:**
> GPSEE's `require("gffi").Memory` objects violate ECMAScript on this point. We  allow equivalence comparisons on instances of Memory, so that users can meaningfully compare returned pointers from FFI, both against each other and against null and Memory(-1) -- the latter being particular useful if you are interfacing with the mmap - it is MAP\_FAILED on most (all?) POSIX systems.

Objects can be coerced to numbers or strings using their `valueOf()` or `toString()` methods. Transitive coercion does not occur; for example, JavaScript will not coerce an object to a number in order to coerce a number to a boolean. The `null` object is a special case which yields `false` when coercing to boolean; all other objects coerce to `true`.

Coercion happens in many scenarios, an exhaustive discussion would take several pages.  However, it is safe to think about what type the interpreter "wants", and to remember that what it "wants" _generally_ happens from left-to-right.

Here are a few points to remember -- this is a non-exhaustive list:
  * Coercion to string occurs during string concatenation
  * Coercion to number occurs during addition (and other arithmetic operators)
  * Whether you are doing string-concatenation or number addition depends on the expression to the immediate left of the expression to be coerced
  * Coercion to boolean occurs inside an if statement, the left-most term of a trinary operator, during the evaluation of a boolean arithmetic expression
  * Certain operators, like `+`, `-`, etc., "want" based on matching the left-and-right sides. In the case of the `+` operator, it "wants" strings more than numbers.

| expression | boolean | number | string |
|:-----------|:--------|:-------|:-------|
| 1          | true    |        | "1"    |
| 0          | false   |        | "0"    |
| true       |         | 1      | "true" |
| false      |         | 0      | "false" |
| "1"        | true    | 1      |        |
| "0"        | true    | 0      |        |
| "hello"    | true    | NaN    |        |
| ""         | false   | 0      |        |
| NaN        | false   |        | "NaN"  |
| null       | false   | 0      | "null" |
| void 0     | false   | NaN    | "undefined" |

If you are writing a JavaScript statement and cannot be sure how it will be coerced based on these rules, we suggest adding parentheses (or otherwise generating intermediate productions) until it is clear.  Code that can only be understood by carefully parsing the language specification is usually best left for obfuscated code contests.

## Truthiness ##

The JavaScript coercion rules have given rise to the colloquial concept of _truthiness_. An expression is said to be **falsey** if it evaluates to `false` when coerced to boolean; otherwise, it is said to be **truey**.

Here are a list of all falsey values in JavaScript:

| **type** | **value** |
|:---------|:----------|
| boolean  | false     |
| number   | 0         |
| number   | -0        |
| undefined | _(all)_   |
| string   | ""        |
| object   | null      |

## Tricky Comparisons ##

  * To test for NaN: _Do not use isNaN_; `isNaN("")` is false. Instead, use `if (X !== X)` -- this expression is only true when X is a NaN.
  * To test for negative Zero: use `if ((n === 0) && (1/x === -Infinity))`

### Testing if something is a function ###
Mark Miller posted this to es5-discuss in August of 2008:
> Q: I see here a test (typeof x === 'function'). Once that passes, can we assume x is a function?
> A: No. (typeof x === 'function') doesn't test whether x is a function, it only tests whether x is callable.
> Q: Ok, how about (x instanceof Function)?
> A: No, that only tests whether x inherits from Function.prototype.
> Q: How about (x.constructor === Function)?
> A: You gotta be kidding.
> Q: So how do I test whether x is a function?
> A: ({}).toString.call(x) === "[Function](object.md)"
> Q: Oh. That was intuitive. Why didn't I think of that?

This post shows how subtle details in language specifications can get into a real mess!
ECMAScript-3 has callable regular expressions and says these should have type of function. Fortunately, most JavaScript vendors currently consider regular expressions to have typeof object, making `typeof x == 'function'` correct....most of the time.

# Wrapped Types #
new String, new Number

# Pitfalls #

semicolon, return object
this

# Links #

  * Douglas Crockford's awesome book, [JavaScript: The Good Parts](http://www.amazon.com/gp/product/0596517742/gpsee)
  * Douglas Crockford on Inheritance: http://javascript.crockford.com/inheritance.html
  * Iterators and Generators: https://developer.mozilla.org/En/Core_JavaScript_1.5_Guide/Iterators_and_Generators