
./make_errno.py is a minimal python script that defines JS constants 

someobj.EAGAIN which maps to "9"


Given a class object named 'someobj', you would add errno constants with:

    if (!JS_DefineConstDoubles(cx, someobj, errno_constants)) {
        return false;
    }


This also defines a C function  'errno2name' which takes an errno and returns the 'symbolic name'
i.e. 9 maps to the string "EAGAIN" (this always annoyed me that posix didn't have this function.. or does it?)

Typically "strerror" returns text but not the errno name.

static const char* errno2name(int e) {
switch (e) {

}....
}


The code has a million ifdefs so it should run correctly on any platform.

Enjoy!

--nickg

