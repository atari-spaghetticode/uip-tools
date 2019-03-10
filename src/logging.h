#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <stdio.h>

#if DEBUG==1
	#define INFO(...) (printf(__VA_ARGS__))
	#define LOG(...) (printf(__VA_ARGS__))
	#define LOG_WARN(...) (printf(__VA_ARGS__))
	#define LOG_TRACE(...) (printf(__VA_ARGS__))
#else
	#define INFO(...) (printf(__VA_ARGS__))
	#define LOG(...)
	#define LOG_WARN(...) (printf(__VA_ARGS__))
	#define LOG_TRACE(...)
#endif

#endif // __LOGGING_H__