// interpose_hook.cpp - a preload library with an alternative adjust() that returns 100.
// Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/interpose_check.sh
extern "C" int adjust(){return 100;}
