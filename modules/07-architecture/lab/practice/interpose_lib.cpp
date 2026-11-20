// interpose_lib.cpp - a plugin whose answer() calls its own exported adjust(); built several ways by interpose_check.sh.
// Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/interpose_check.sh
extern "C" int adjust(){return 11;}
extern "C" int answer(){return adjust()+1;}
