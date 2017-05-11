
# Uncomment this if you're using STL in your project
# See CPLUSPLUS-SUPPORT.html in the NDK documentation for more information
APP_STL := stlport_static 

ifeq (,$(profile))
#Make us equal for easy double support!
profile = $(line-profile)
endif

ifneq (,$(profile))
#Support specific platforms only for this profiler!
APP_ABI := armeabi armeabi-v7a
else
#Support all platforms
APP_ABI := all
endif
