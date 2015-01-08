#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <stdio.h>
#include <stdint.h>
#include <thread>

#ifdef _WIN32

#ifndef localtime_r
#define localtime_r(unix_time, tms) localtime_s(tms, unix_time)
#endif
#ifndef snprintf
#define snprintf(buffer, buffer_size, fmt, ...) _snprintf(buffer, buffer_size, fmt, ##__VA_ARGS__)
#endif

#define DIR_SEP '\\'
#define THREADID static_cast<uint32_t>(GetCurrentThreadId())

#else
#define DIR_SEP '/'
#define THREADID static_cast<uint32_t>(pthread_self())
#endif

#ifndef __FUNCSIG__
#ifdef  __PRETTY_FUNCTION__
#define __FUNCSIG__ __PRETTY_FUNCTION__
#elif defined __FUNCTION__
#define __FUNCSIG__ __FUNCTION__
#else
#define __FUNCSIG__ __func__
#endif
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
	LOGGER_NULL = 8,
};

class Logger
{
public:
	int m_fd;
	int32_t m_level;

public:
	int32_t log(const char* buf, uint32_t l);
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
}\

#define logger_trace(logger, fmt, ...) _logger_wrapper(logger, TRACE, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_debug(logger, fmt, ...) _logger_wrapper(logger, DEBUG, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_info(logger, fmt, ...) _logger_wrapper(logger, INFO, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_warn(logger, fmt, ...) _logger_wrapper(logger, WARN, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_error(logger, fmt, ...) _logger_wrapper(logger, ERROR, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_fatal(logger, fmt, ...) _logger_wrapper(logger, FATAL, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)

#endif
