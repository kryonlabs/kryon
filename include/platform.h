#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define ANDROID_BUILD 1
#else
#define ANDROID_BUILD 0
#endif

#endif
