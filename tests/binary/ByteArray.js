#!/usr/bin/gsr -zzdd

var verbose = 0, quiet = false;
for each(var arg in arguments) switch (arg) {
    case '-v':
        verbose++;
    case '-q':
        quiet = true;
}

const ByteArray  = require("binary").ByteArray;
const Binary     = require("binary").Binary;

function values(ob) {
    var values = [];
    for(var key in ob)
        if (ob.hasOwnProperty(key))
            values.push(ob[key]);
    return values;
}

/* Provide some scaffolding for individual tests */
var testUtils = {
    /* eq compares two or more values and expects that they will pass an equality test without type coercion (===) */
    'eq': function() {
        if (arguments.length < 2) throw 'too few arguments to testUtils.eq()';
        for(var i=1, l=arguments.length; i<l; i++) {
            if (('object' == typeof arguments[0]) && ('object' == typeof arguments[i])) {
                var vals0 = values(arguments[0]);
                var valsi = values(arguments[i]);
                if (vals0.length != valsi.length) {
                    print('OBJECT SIZE MISMATCH');
                    print('V1 "'+vals0+'"');
                    print('V2 "'+valsi+'"');
                    return false;
                }
                for (var j=0, jl=vals0.length; j<jl; j++) {
                    /* TODO recursion instead of one level-deep bs */
                    if (vals0[j] !== valsi[j]) {
                        print('OBJECT VALUE MISMATCH');
                        print('V1 "'+arguments[0]+'"');
                        print('V2 "'+arguments[i]+'"');
                        return false;
                    }
                }
            } else {
                if (arguments[0] !== arguments[i]) {
                    print('VALUE MISMATCH');
                    print('V1 "'+arguments[0]+'"');
                    print('V2 "'+arguments[i]+'"');
                    return false;
                }
            }
        }
        return true;
    },
    'dbg': function(level, msg) {
        if (level <= verbose)
            print(msg);
    },
    /* 'startswith' string function for gpsee exception handling */
    'sw': function(part) {
        return function(full) {
            testUtils.dbg(1, 'testing exception: '+full);
            if (full.indexOf(part) == 0) return true;
            print('EXCEPTION MISMATCH');
            print('V1', full);
            print('V2', part);
            return false;
        }
    },
    /* 'endswith' string function for gpsee exception handling */
    'ew': function(part) {
        return function(full) {
            testUtils.dbg(1, 'testing exception: "'+full+'"');
            full = full.toString();
            if (full.toString().indexOf(part) == (full.length - part.length)) return true;
            print('EXCEPTION MISMATCH');
            print('V1', full);
            print('V2', part);
            return false;
        }
    },
};

/* A little scaffolding for dispatching individual tests */
function runtest(test) {
    /* The test can set testUtils.ex to an expected exception, if you're into that sort of thing */
    delete testUtils.ex;
    testUtils.dbg(2, 'Running following test:');
    testUtils.dbg(2, test);
    try { return test(testUtils) }
    catch (ex) {
        /* Is an exception expected? */
        if (testUtils.ex) {
            /* Is it to be tested against a function? */
            if ('function' == typeof testUtils.ex)
                return testUtils.ex(ex);
            /* Otherwise just compare */
            return testUtils.ex == ex;
        }
        /* This will all end in tears */
        print('Exception thrown:', ex);
    }
}

/* Test material */
const gobbledygook = '2~`E 6_BV JGrK 4*I<f 0^B,g Ctks ;~Sn dA:Z )#h/qp4 I3p^C';
/* Exceptions */
//function mkexs(){return arguments.reduce(function(a,b)a+'.'+b)}
const BASE = 'gpsee.module.ca.page';
const BYTEARRAY = BASE + '.binary.ByteArray';
const SUBSTR = BYTEARRAY + '.substr';
const EX_SUBSTR_OVERFLOW = function(n){return SUBSTR+'.arguments.'+n+'.overflow'};
const EX_SUBSTR_UNDERFLOW = function(n){return SUBSTR+'.arguments.'+n+'.underflow'};
const SUBARRAY = BYTEARRAY + '.substring';
const EX_SUBARRAY_OVERFLOW = function(n){return SUBARRAY+'.arguments.'+n+'.overflow'};
const EX_SUBARRAY_UNDERFLOW = function(n){return SUBARRAY+'.arguments.'+n+'.underflow'};
const EX_SUBARRAY_INVALID_RANGE = 'gpsee.module.ca.page.binary.ByteArray.substring.range.invalid';
const EX_INDEXOF_INVALID_BYTE = BYTEARRAY + '.indexOf.arguments.0.byte.invalid';
const EX_LASTINDEXOF_INVALID_BYTE = BYTEARRAY + '.lastIndexOf.arguments.0.byte.invalid';
const EX_SETTER_INDEX_RANGE = BYTEARRAY + '.setter.index.range';
const EX_SETTER_INDEX_INVALID = BYTEARRAY + '.setter.index.invalid';


var tests = [
/* Various decode/extract tests (preserved from old test code. can't have too many tests!) */
function(){return new ByteArray("hello world").decodeToString("utf-8") === 'hello world'},
function(){return new ByteArray("hello").toByteString().decodeToString("ascii") === 'hello'},
function(){return new ByteArray("hello")[0] == 'h'.charCodeAt(0)},
function(t){return t.eq(new ByteArray("hello").get(4), 111)},
function(){return new ByteArray("hello").length === 5},
function(){return new ByteArray("hello", "utf-8").toByteString("utf-8", "utf-16").length === 12},
function(){return (new ByteArray("hello") instanceof Binary) === true},
/* Test iconv(3) on empty string */
function(t){return t.eq(new ByteArray("").toByteString("ascii", "utf-8").decodeToString("ascii"), '')},
/* indexOf tests */
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0)), 0)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0),0), 0)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0),1), 8)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0),8), 8)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0),9), -1)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('W'.charCodeAt(0),1,7), -1)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").indexOf('!'.charCodeAt(0)), 14)},
function(t){t.ex=t.sw(EX_INDEXOF_INVALID_BYTE); new ByteArray(":)").indexOf(256)},
/* lastIndexOf tests */
function(t){return t.eq(new ByteArray("Where's Waldo?!").lastIndexOf('W'.charCodeAt(0)), 8)},
function(t){return t.eq(new ByteArray("Where's Waldo?!").lastIndexOf('W'.charCodeAt(0), 0, 7), 0)},
function(t){t.ex=t.sw(EX_LASTINDEXOF_INVALID_BYTE); new ByteArray(":)").lastIndexOf(256)},
/* slice tests */
function(t){return t.eq(new ByteArray("Hey! Pizza Land, huh? That's lot's of fun!").slice(5,10).decodeToString('ascii'), 'Pizza')},
/* splice tests */
function(t){var b=new ByteArray("PizzaLand"); return t.eq(b.splice(5).decodeToString('ascii'),'Land') && t.eq(b.decodeToString('ascii'),'Pizza') },
function(t){var b=new ByteArray("PizzaLand"); return t.eq(b.splice(5,9).decodeToString('ascii'),'Land') && t.eq(b.decodeToString('ascii'),'Pizza') },
/* length setter tests! */
function(t){var b=new ByteArray;b.length=1;return t.eq(b.length,1)},
function(t){var b=new ByteArray;b.length=1;return t.eq(b.decodeToString('ascii'),String.fromCharCode(0))},
/* extend/concat tests */
function(t){var b=new ByteArray;b.unshift(65, 66, [67, 68, 69], 70, [71, 72]);return t.eq(b.decodeToString('ascii'), "ABCDEFGH")},
function(t){var b=new ByteArray;b.unshift(65, 66, [67, 68, 69], 70, [71, 72]);b.unshift(b);return t.eq(b.decodeToString('ascii'), "ABCDEFGHABCDEFGH")},
function(t){var b=new ByteArray;b.unshift(67);b.unshift(66);b.unshift(65);b.unshift(10);return t.eq(b.shift(), 10) && t.eq(b.decodeToString('ascii'), 'ABC')},
function(t){var b=new ByteArray;b.extendLeft(65, 66, [67, 68, 69], 70, [71, 72]);return t.eq(b.decodeToString('ascii'), "ABCDEFGH")},
function(t){var b=new ByteArray;b.extendLeft(65, 66, [67, 68, 69], 70, [71, 72]);b.extendLeft(b);return t.eq(b.decodeToString('ascii'), "ABCDEFGHABCDEFGH")},
function(t){var b=new ByteArray;b.extendRight(65, 66, [67, 68, 69], 70, [71, 72]);return t.eq(b.decodeToString('ascii'), "ABCDEFGH")},
function(t){var b=new ByteArray;b.extendRight(65, 66, [67, 68, 69], 70, [71, 72]);b.extendRight(b);return t.eq(b.decodeToString('ascii'), "ABCDEFGHABCDEFGH")},
function(t){var b=new ByteArray;b.concat(65, 66, [67, 68, 69], 70, [71, 72]);return t.eq(b.decodeToString('ascii'), "ABCDEFGH")},
function(t){var b=new ByteArray;b.concat(65, 66, [67, 68, 69], 70, [71, 72]);b.concat(b);return t.eq(b.decodeToString('ascii'), "ABCDEFGHABCDEFGH")},
/* pop */
function(t){var b=new ByteArray('ABC');return t.eq(b.pop(), 67) && t.eq(b.pop(), 66) && t.eq(b.pop(), 65) && t.eq(b.length, 0)},
/* subscript operator tests (well, numeric indexing) */
function(t){var n='a'.charCodeAt(0)-'A'.charCodeAt(0),b=new ByteArray("ABC");b[0]+=n;return t.eq(b.decodeToString('ascii'),'aBC')},
function(t){t.ex=t.sw(EX_SETTER_INDEX_RANGE);new ByteArray('ABC')[3]=100;},
function(t){var b=new ByteArray('ABC');b[-1]=100; return t.eq(b[-1], 100)},
function(t){var b=new ByteArray('ABC');b.x=100; return t.eq(b.x, 100)},
/* shift tests */
function(t){return t.eq(new ByteArray().shift(), undefined)},
function(t){var b=new ByteArray("ABC"); return t.eq(b.shift(), 65) && t.eq(b.decodeToString('ascii'), 'BC')},
// freeze! function(t){var b=new ByteArray(gobbledygook); return t.eq(b.shift(), gobbledygook.charCodeAt(0)) && t.eq(b.decodeToString('ascii'), gobbledygook.substr(1))},
/* sort tests :| */
//infinite loop!? it eventually failed a mutex assert. ByteArray.sort() never even gets called, as you can see from the code
//function(t){var b=new ByteArray(gobbledygook), c=Array.prototype.map.call(gobbledygook, function(x)x); c.sort(); return t.eq(b.decodeToString('ascii'), c.join('')) },
/* forEach */
function(t){var b=new ByteArray("ABC"),s='';b.forEach(function(a,b){s+=','+a+','+b});return t.eq(s, ',65,0,66,1,67,2')},
/* filter */
function(t){var b=new ByteArray;for(var i=0;i<256;i++)b.push(i);var c=b.filter(function(x){return x==42}); return t.eq(c.length, 1) && t.eq(c[0], 42)},
/* map, some, and every */
function(t){var b=new ByteArray;for(var i=0;i<256;i++)b.push(i);var c=b.map(function(x){return x|7}); return t.eq(b.some(function(x){return x&7?true:false}), true) && t.eq(c.every(function(x){return x&7?true:false}), true)},
/* reduce */
function(t){var b=new ByteArray;for(var i=0;i<8;i++)b.push(1<<i);return t.eq(b.reduce(function(a,b){return a|b}), 255)},
/* reduce and reduceRight */
function(t){var b=new ByteArray([16,2,1]);return t.eq(b.reduce(function(a,b){return a/b}), 8)},
function(t){var b=new ByteArray([16,2,1]);return t.eq(b.reduceRight(function(a,b){return a/b}), 0.03125)},
/* undefined member method invocation */
function(t) { t.ex = t.ew(' is not a function'); new ByteArray('hi').notAFunction('args'); },
];

/* Run all the tests! */
var passed = 0;
var failed = 0;
for each(var test in tests) {
    if (runtest(test)) {
        passed++;
    } else {
        failed++;
        print("the test that failed this way:");
        print(test);
    }
}

/* Report test results */
if (!quiet) {
  print('passed '+passed+' tests');
  print('failed '+failed+' tests');

  if (!failed)
      print('ALL TESTS PASSED');
  else
      print('FAIL');
}

