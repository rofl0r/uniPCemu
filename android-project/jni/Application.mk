
# Uncomment this if you're using STL in your project
# See CPLUSPLUS-SUPPORT.html in the NDK documentation for more information
# APP_STL := stlport_static 

ifeq (,$(NDK_PROFILE))
#Make us equal for easy double support!
NDK_PROFILE = $(NDK_LINEPROFILE)
endif

ifneq (,$(NDK_PROFILE))
#Support specific platforms only for this profiler!
APP_ABI := armeabi armeabi-v7a
else
#Support all platforms
ifeq (2,$(ANDROIDSTUDIO))
APP_ABI := armeabi armeabi-v7a arm64-v8a x86 x86_64 mips
else
APP_ABI := all
endif
endif

# Min runtime API level
APP_PLATFORM=android-14