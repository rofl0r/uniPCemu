#ifdef PACKED
#undef PACKED
#endif
#ifdef _WIN32
#ifndef __GNUC__
#ifndef __MINGW32__
//Visual C++ Assumed
#define PACKED
#pragma pack(push,1)
//Our VisualC flag for end detection!
#ifndef VISUALC
#define VISUALC
#endif
#endif
#endif
#endif

#ifndef VISUALC
//GCC assumed
#define PACKED __attribute__ ((__packed__))
#endif