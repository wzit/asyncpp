#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <string>
#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <errno.h>

#ifdef __cplusplus
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "spinlock.hpp"

#ifdef _WIN32

#ifndef localtime_r
#define localtime_r(unix_time, tms) localtime_s(tms, unix_time)
#endif
#ifndef snprintf
#define snprintf(buffer, buffer_size, fmt, ...) _snprintf(buffer, buffer_size, fmt, ##__VA_ARGS__)
#endif

#define DIR_SEP '\\'
#define THREADID (uint32_t)(GetCurrentThreadId())

#else
#define DIR_SEP '/'
#ifdef __i386__
#define THREADID (uint32_t)(pthread_self())
#elif defined __x86_64__
#define THREADID (uint32_t)(uint64_t)(pthread_self())
#elif defined _AIX
#define THREADID 0
#else
#error architecture not supported
#endif
#include <unistd.h>
#endif

#ifdef _WIN32

#ifdef _DEBUG
#define __LOGFUNCNAME__ __FUNCSIG__
#else
#define __LOGFUNCNAME__ __FUNCTION__
#endif

#else

#define __LOGFUNCNAME__ __func__

#endif


const int LOGGER_LINE_SIZE = 4096;
enum e_logger_level_t
{
	LOGGER_ALL = 0,
	LOGGER_TRACE = 2,
	LOGGER_DEBUG = 3,
	LOGGER_INFO = 4,
	LOGGER_WARN = 5,
	LOGGER_ERROR = 6,
	LOGGER_FATAL = 7,
	LOGGER_NONE = 8,
};

class Logger
{
public:
	volatile int m_fd;
	int32_t m_level;
	uint32_t m_filesize;
	uint32_t m_filesizelimit;
	int32_t m_filenumlimit;
	asyncpp::SpinLock m_lock;
	std::string m_filepath;
public:
	int32_t log(const char* buf, uint32_t l);
	int32_t load_cfg(const char* cfg_path);
};

extern Logger* logger;

extern char logger_time_string_buffer[24];
void logger_update_time_string();

#define _logger_wrapper(logger, logger_level, file, line, func, fmt, ...) \
if (LOGGER_##logger_level >= logger->m_level)\
{\
	char _lg_buf[LOGGER_LINE_SIZE]; \
	int32_t _lg_l; \
	const char* _lg_pfile = strrchr(file, DIR_SEP);\
	_lg_pfile = _lg_pfile?_lg_pfile+1:file;\
	logger_update_time_string(); \
	_lg_l = snprintf(_lg_buf, LOGGER_LINE_SIZE, "[" #logger_level "] %s [%s:%u: %s %u] " fmt "\n", logger_time_string_buffer, _lg_pfile, line, func, THREADID, ##__VA_ARGS__); \
	if(_lg_l>0) logger->log(_lg_buf,_lg_l<LOGGER_LINE_SIZE?_lg_l:LOGGER_LINE_SIZE-1); \
}

#define logger_trace(logger, fmt, ...) _logger_wrapper(logger, TRACE, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)
#define logger_debug(logger, fmt, ...) _logger_wrapper(logger, DEBUG, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)
#define logger_info(logger, fmt, ...) _logger_wrapper(logger, INFO, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)
#define logger_warn(logger, fmt, ...) _logger_wrapper(logger, WARN, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)
#define logger_error(logger, fmt, ...) _logger_wrapper(logger, ERROR, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)
#define logger_fatal(logger, fmt, ...) _logger_wrapper(logger, FATAL, __FILE__, __LINE__, __LOGFUNCNAME__, fmt, ##__VA_ARGS__)

#ifndef _RELEASE
#define logger_assert(expr) \
if (!(expr))\
{\
	int32_t _lg_errcode = errno;\
	logger_fatal(logger, "Assertion '" #expr "' failed. errno:%d[%s]", _lg_errcode, strerror(_lg_errcode)); \
	fprintf(stderr, "%s: %u: %s: Assertion '" #expr "' failed.\n", __FILE__, __LINE__, __LOGFUNCNAME__); \
	abort();\
}

#define logger_assert_val(int_var, expected_val) \
if (int_var != expected_val)\
{\
	logger_fatal(logger, #int_var " = %d.", int_var); \
	fprintf(stderr, "%s: %u: %s: " #int_var " = %d\n", __FILE__, __LINE__, __LOGFUNCNAME__, int_var); \
	abort(); \
}

#define logger_assert_ret(ret) logger_assert_val(ret, 0)

#define logger_assert_false() \
{\
	logger_fatal(logger, "Assertion fatal error."); \
	fprintf(stderr, "%s: %u: %s: Assertion fatal error.\n", __FILE__, __LINE__, __LOGFUNCNAME__); \
	abort(); \
}
#else
#define logger_assert(expr)
#define logger_assert_ret_val(ret)
#define logger_assert_false()
#endif

#endif
