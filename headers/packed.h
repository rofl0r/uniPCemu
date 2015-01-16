#ifdef _WIN32
//Visual C++ Assumed
#define PACKED
#pragma pack(push,1)
#else
//GCC assumed
#define PACKED __attribute__ ((__packed__))
#endif