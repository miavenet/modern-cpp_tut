// export_plugin.cpp - two C-linkage functions; built three ways by export_check.sh (default, all hidden, hidden + one exported).
// Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/export_check.sh
#ifdef EXPORT_API
#define API __attribute__((visibility("default")))
#else
#define API
#endif
extern "C" API int public_value(){return 42;}
extern "C" int internal_value(){return 6;}
