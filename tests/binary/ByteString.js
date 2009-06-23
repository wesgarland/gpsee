#!/usr/bin/gsr -zzdd

var verbose = 0;
for each(var arg in arguments) switch (arg)
{
    case '-v':
        verbose++;
}

const ByteString = require("binary").ByteString;
const Binary     = require("binary").Binary;

function values(ob)
{
    var values = [];
    for(var key in ob)
        if (ob.hasOwnProperty(key))
            values.push(ob[key]);
    return values;
}

/* Provide some scaffolding for individual tests */
var testUtils = {
    /* eq compares two or more values and expects that they will pass an equality test without type coercion (===) */
    'eq': function()
    {
        if (arguments.length < 2) throw 'too few arguments to testUtils.eq()';
        for(var i=1, l=arguments.length; i<l; i++)
        {
            if (('object' == typeof arguments[0]) && ('object' == typeof arguments[i]))
            {
                var vals0 = values(arguments[0]);
                var valsi = values(arguments[i]);
                if (vals0.length != valsi.length)
                {
                    print('OBJECT SIZE MISMATCH');
                    print('V1', vals0);
                    print('V2', valsi);
                    return false;
                }
                for (var j=0, jl=vals0.length; j<jl; j++)
                {
                    /* TODO recursion instead of one level-deep bs */
                    if (vals0[j] !== valsi[j])
                    {
                        print('OBJECT VALUE MISMATCH');
                        print('V1', arguments[0]);
                        print('V2', arguments[i]);
                        return false;
                    }
                }
            } else {
                if (arguments[0] !== arguments[i])
                {
                    print('VALUE MISMATCH');
                    print('V1', arguments[0]);
                    print('V2', arguments[i]);
                    return false;
                }
            }
        }
        return true;
    },
    'dbg': function(level, msg)
    {
        if (level <= verbose)
            print(msg);
    },
    /* 'startswith' string function for gpsee exception handling */
    'sw': function(part)
    {
        return function(full)
        {
            testUtils.dbg(1, 'testing exception: '+full);
            if (full.indexOf(part) == 0) return true;
            print('EXCEPTION MISMATCH');
            print('V1', full);
            print('V2', part);
            return false;
        }
    },
    /* 'endswith' string function for gpsee exception handling */
    'ew': function(part)
    {
        return function(full)
        {
            testUtils.dbg(1, 'testing exception: '+full);
            if (full.indexOf(part) == (full.length - part.length)) return true;
            print('EXCEPTION MISMATCH');
            print('V1', full);
            print('V2', part);
            return false;
        }
    },
};

/* A little scaffolding for dispatching individual tests */
function runtest(test)
{
    /* The test can set testUtils.ex to an expected exception, if you're into that sort of thing */
    delete testUtils.ex;
    testUtils.dbg(2, 'Running following test:');
    testUtils.dbg(2, test);
    var rval;
    try {
        rval = test(testUtils);
        if ('undefined' == typeof testUtils.ex)
            return rval;
        throw 'no exception thrown!'
    }
    catch (ex)
    {
        /* Is an exception expected? */
        if (testUtils.ex)
        {
            /* Is it to be tested against a function? */
            if ('function' == typeof testUtils.ex)
                return testUtils.ex(ex);
            /* Otherwise just compare */
            return testUtils.ex == ex;
        }
        /* This will all end in tears */
        print('Exception thrown:', ex);
        return false;
    }
}

/* Test material */
const gobbledygook = '2~`E 6_BV JGrK 4*I<f 0^B,g Ctks ;~Sn dA:Z )#h/qp4 I3p^C';
/* Exceptions */
//function mkexs(){return arguments.reduce(function(a,b)a+'.'+b)}
const BASE = 'gpsee.module.ca.page';
const BYTESTRING = BASE + '.binary.ByteString';
const SUBSTR = BYTESTRING + '.substr';
const EX_SUBSTR_OVERFLOW = function(n){return SUBSTR+'.arguments.'+n+'.overflow'};
const EX_SUBSTR_UNDERFLOW = function(n){return SUBSTR+'.arguments.'+n+'.underflow'};
const SUBSTRING = BYTESTRING + '.substring';
const EX_SUBSTRING_OVERFLOW = function(n){return SUBSTRING+'.arguments.'+n+'.overflow'};
const EX_SUBSTRING_UNDERFLOW = function(n){return SUBSTRING+'.arguments.'+n+'.underflow'};
const EX_SUBSTRING_INVALID_RANGE = 'gpsee.module.ca.page.binary.ByteString.substring.range.invalid';
const EX_INDEXOF_INVALID_BYTE = BYTESTRING + '.indexOf.arguments.0.range';
const EX_LASTINDEXOF_INVALID_BYTE = BYTESTRING + '.lastIndexOf.arguments.0.range';

var tests = [
/* Various decode/extract tests (preserved from old test code. can't have too many tests!) */
function(){return new ByteString("hello world").decodeToString("utf-8") === 'hello world'},
function(){return new ByteString("hello").toByteString().decodeToString("ascii") === 'hello'},
function(){return new ByteString("hello")[0].decodeToString("ascii") == 'h'},
function(){return new ByteString("hello").charAt(1).decodeToString("US-ASCII") === 'e'},
function(){return new ByteString("hello").byteAt(4) === 111},
function(){return new ByteString("hello").length === 5},
function(){return new ByteString("hello", "utf-8").toByteString("utf-8", "utf-16").length === 12},
function(){return (new ByteString("hello") instanceof Binary) === true},
/* toString tests */
function(t){return t.eq(new ByteString(gobbledygook)+'','[object gpsee.module.ca.page.binary.ByteString '+gobbledygook.length+']')},
/* decodeToString tests */
/* decodeToString is pretty much tested in every other test */
/* indexOf tests */
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0)), 0)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0),0), 0)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0),1), 8)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0),8), 8)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0),9), -1)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('W'.charCodeAt(0),1,7), -1)},
function(t){return t.eq(new ByteString("Where's Waldo?!").indexOf('!'.charCodeAt(0)), 14)},
function(t){t.ex=t.sw(EX_INDEXOF_INVALID_BYTE); new ByteString(":)").indexOf(256)},
/* lastIndexOf tests */
function(t){return t.eq(new ByteString("Where's Waldo?!").lastIndexOf('W'.charCodeAt(0)), 8)},
function(t){return t.eq(new ByteString("Where's Waldo?!").lastIndexOf('W'.charCodeAt(0), 0, 7), 0)},
function(t){t.ex=t.sw(EX_LASTINDEXOF_INVALID_BYTE); new ByteString(":)").lastIndexOf(256)},
/* charAt tests */
function(t){return t.eq(new ByteString("Where's Waldo?!").charAt(0).decodeToString('US-ASCII'), 'W') },
function(t){return t.eq(new ByteString("Where's Waldo?!").charAt(8).decodeToString('US-ASCII'), 'W') },
/* byteAt tests */
function(t){return t.eq(new ByteString("Where's Waldo?!").byteAt(0), 87) },
function(t){return t.eq(new ByteString("Where's Waldo?!").byteAt(8), 87) },
/* charCodeAt tests */
function(t){return t.eq(new ByteString("Where's Waldo?!").charCodeAt(0), 87) },
function(t){return t.eq(new ByteString("Where's Waldo?!").charCodeAt(8), 87) },
/* split tests */
function(t) { return t.eq([s.decodeToString('US-ASCII') for each (s in (new ByteString(gobbledygook).split(32)))], gobbledygook.split(' ')) },
function(t) { return t.eq([s.decodeToString('US-ASCII') for each (s in (new ByteString(gobbledygook).split(66)))], gobbledygook.split('B')) },
function(t) { return t.eq([s.decodeToString('US-ASCII') for each (s in (new ByteString('Hello World!').split(0)))], ['Hello World!']) },
/* slice tests */
//function(t) { return t.eq(new ByteString
/* substr tests */
function(t) { return t.eq(new ByteString('1234').substr(0,1).decodeToString('US-ASCII'), '1') },
function(t) { return t.eq(new ByteString('1234').substr(1,1).decodeToString('US-ASCII'), '2') },
function(t) { return t.eq(new ByteString('1234').substr(2,2).decodeToString('US-ASCII'), '34') },
function(t) { t.ex = t.sw(EX_SUBSTR_OVERFLOW(1)); new ByteString('1234').substr(2,3) },
function(t) { t.ex = t.sw(EX_SUBSTR_UNDERFLOW(0)); new ByteString('1234').substr(-1,3) },
function(t) { t.ex = t.sw(EX_SUBSTR_UNDERFLOW(0)); new ByteString('invalid').substr(0,0) },
/* substring tests */
function(t) { return t.eq(new ByteString('1234').substring(0,1).decodeToString('US-ASCII'), '1') },
function(t) { t.ex = t.sw(EX_SUBSTRING_OVERFLOW(0)); new ByteString('overflow').substring(17,19) },
function(t) { t.ex = t.sw(EX_SUBSTRING_UNDERFLOW(0)); new ByteString('underflow').substring(-10) },
function(t) { return t.eq(new ByteString('Institutionalized education is a test of obedience!').substring(17,31).decodeToString('US-ASCII'), ' education is ') },
/* toSource tests */
/* TODO add unit test support for string comparison intended for comparing source code. In this case, the quotes around 'binary' may be double or single! Stupid Javascript! */
function(t) { return t.eq(new ByteString('Bush rigged the elections!').toSource(), '(new require("binary").ByteString([66,117,115,104,32,114,105,103,103,101,100,32,116,104,101,32,101,108,101,99,116,105,111,110,115,33]))') },
/* undefined member method invocation */
function(t) { t.ex = t.ew(' is not a function'); new ByteString('hi').notAFunction('args'); },
];

/* Run all the tests! */
var passed = 0;
var failed = 0;
for each(var test in tests)
{
    if (runtest(test))
    {
        passed++;
    } else {
        failed++;
        print("the test that failed this way:");
        print(test);
    }
}

/* Report test results */
print('passed '+passed+' tests');
print('failed '+failed+' tests');

if (!failed)
    print('ALL TESTS PASSED');
else
    print('FAIL');

