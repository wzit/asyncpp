#include "asyncommon.hpp"
#include <cassert>
#include <ctime>
#ifdef __GNUC__
#include <unistd.h>
#include <dirent.h>
#elif defined _WIN32
#include <direct.h>
#include <share.h>
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO    0   /* Standard input.  */
#define STDOUT_FILENO   1   /* Standard output.  */
#define STDERR_FILENO   2   /* Standard error output.  */

#define mkdir(path, mode) _mkdir(path)
#define write(fd, buf,len) _write(fd, buf, len)
#endif
#else
#error os not support
#endif

namespace asyncpp
{

volatile time_t g_unix_timestamp = time(nullptr);

static Logger _logger = { STDOUT_FILENO, LOGGER_ALL };

Logger* logger = &_logger;

time_t last_logger_time = 0;
char logger_time_string_buffer[24];

void logger_update_time_string()
{
	struct tm _lg_tm;
	time_t _lg_t = g_unix_timestamp;
	if (_lg_t != last_logger_time)
	{
		last_logger_time = _lg_t;
		localtime_r(&_lg_t, &_lg_tm);
		sprintf(logger_time_string_buffer, "[%04u-%02u-%02u %02u:%02u:%02u]", _lg_tm.tm_year + 1900, _lg_tm.tm_mon + 1, _lg_tm.tm_mday, _lg_tm.tm_hour, _lg_tm.tm_min, _lg_tm.tm_sec);
	}
}

int32_t Logger::logger_impl(const char* buf, uint32_t l)
{
	return write(m_fd, buf, l);
}

} //end of namespace asyncpp

