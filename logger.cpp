#include "logger.hpp"
#include "asyncommon.hpp"
#include "string_utility.h"
#include <cassert>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#ifdef __GNUC__
#include <locale>
#define O_BINARY 0
#elif defined _WIN32
#include <codecvt>
#include <direct.h>
#include <share.h>
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO    0   /* Standard input.  */
#define STDOUT_FILENO   1   /* Standard output.  */
#define STDERR_FILENO   2   /* Standard error output.  */
#endif
#define mkdir(path, mode) _mkdir(path)
#define write(fd, buf,len) _write(fd, buf, len)
#pragma warning(disable:4996)
#else
#error os not support
#endif

using namespace std;

static Logger _logger = { STDOUT_FILENO, LOGGER_ALL };

Logger* logger = &_logger;

time_t last_logger_time = 0;
char logger_time_string_buffer[24];

void logger_update_time_string()
{
	struct tm _lg_tm;
	time_t _lg_t = asyncpp::g_unix_timestamp;
	if (_lg_t != last_logger_time)
	{
		last_logger_time = _lg_t;
		localtime_r(&_lg_t, &_lg_tm);
		sprintf(logger_time_string_buffer, "[%04u-%02u-%02u %02u:%02u:%02u]", _lg_tm.tm_year + 1900, _lg_tm.tm_mon + 1, _lg_tm.tm_mday, _lg_tm.tm_hour, _lg_tm.tm_min, _lg_tm.tm_sec);
	}
}

int32_t Logger::log(const char* buf, uint32_t l)
{
	asyncpp::ScopeSpinLock _lock(m_lock);

	if (m_fd >= 3)
	{
		int32_t ret;
		if (m_filesize < m_filesizelimit) m_filesize += l; //write size may < l
		else
		{
			char numstr[16];
			i32toa(m_filenumlimit - 1, numstr);
			string newpath = m_filepath + '.' + numstr;
#ifdef _WIN32
			unlink(newpath.c_str());
#endif
			for (int32_t i = m_filenumlimit - 2; i > 0; --i)
			{
				i32toa(i, numstr);
				string oldpath = m_filepath + '.' + numstr;
				ret = rename(oldpath.c_str(), newpath.c_str());
				newpath = oldpath;
			}
			ret = close(m_fd);
			assert(ret == 0);
			ret = rename(m_filepath.c_str(), newpath.c_str());
			assert(ret == 0);
			m_fd = open(m_filepath.c_str(), O_RDWR | O_BINARY | O_CREAT, 0644);
			assert(m_fd >= 3);
			m_filesize = 0;
		}
	}

	return write(m_fd, buf, l);
}

int32_t Logger::load_cfg(const char* cfg_path)
{
	int32_t ret = 0;
	char line[1100];
#ifdef _WIN32
	wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> utf16conv;
	FILE* f = _wfopen(utf16conv.from_bytes(cfg_path).c_str(), L"r");
#else
	FILE* f = fopen(cfg_path, "r");
#endif
	if (f == nullptr)
	{
		//printf("load cfg, open %s fail:%d[%s]\n", cfg_path, errno, strerror(errno));
		return errno;
	}

	char* pline = fgets(line, 1024, f);
	if (pline == nullptr)
	{
		fclose(f);
		return EINVAL;
	}

	if ((uint8_t)line[0] & 0x80)
	{
		if (!((uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF))
		{ // utf-8 bom
			printf("support utf8 and ascii file\n");
			fclose(f);
			return EINVAL;
		}
		pline += 3;
	}

	asyncpp::ScopeSpinLock _lock(m_lock);
	do
	{
		skip_space(pline);
		int n = static_cast<int>(strlen(pline));
		if (n == 0) continue;
		while (isspace(pline[--n]));
		pline[n + 1] = 0;

		if (pline[0] == ';' || pline[0] == '#') continue;

		const char* pval = strchr(pline, '=');
		if (pval != nullptr)
		{
			const char* p = pval - 1;
			while (isspace(*p)) --p;
			++pval;
			skip_space(pval);

			if (strnicmp(pline, "file", p - pline) == 0)
			{
				m_filepath = pval;
				if (m_fd >= 3) close(m_fd);
				m_fd = open(pval, O_APPEND | O_RDWR | O_BINARY, 0644);
				if (m_fd >= 3)
				{
					struct stat fst;
					ret = fstat(m_fd, &fst);
					assert(ret == 0);
					m_filesize = static_cast<int32_t>(fst.st_size);
				}
				else
				{
					if (errno == ENOENT)
					{
						m_fd = open(pval, O_CREAT | O_RDWR | O_BINARY, 0644);
						assert(m_fd >= 3);
						m_filesize = 0;
					}
					else
					{
						assert(0);
					}
				}
			}
			else if (strnicmp(pline, "level", p - pline) == 0)
			{
				if (stricmp(pval, "ALL") == 0)
					m_level = LOGGER_ALL;
				else if (stricmp(pval, "TRACE") == 0)
					m_level = LOGGER_TRACE;
				else if (stricmp(pval, "DEBUG") == 0)
					m_level = LOGGER_DEBUG;
				else if (stricmp(pval, "INFO") == 0)
					m_level = LOGGER_INFO;
				else if (stricmp(pval, "WARN") == 0)
					m_level = LOGGER_WARN;
				else if (stricmp(pval, "ERROR") == 0)
					m_level = LOGGER_ERROR;
				else if (stricmp(pval, "FATAL") == 0)
					m_level = LOGGER_FATAL;
				else if (stricmp(pval, "NONE") == 0)
					m_level = LOGGER_NONE;
				else printf("error line:%s\n", line);
			}
			else if (strnicmp(pline, "filesizelimit", p - pline) == 0)
			{
				const char* punit;
				uint64_t filesizelimit = strtou32(pval, &punit, 10);
				if (*punit == 'M' || *punit == 'm')
				{ //MB
					filesizelimit *= 1024 * 1024;
				}
				else if (*punit == 'K' || *punit == 'k')
				{ //KB
					filesizelimit *= 1024;
				}
				else if (*punit == 'G' || *punit == 'g')
				{ //GB
					filesizelimit *= 1024 * 1024 * 1024;
				}

				if (filesizelimit > 3 * 1024 * 1024 * 1024ull)
				{
					printf("max limit 3GB\n");
					m_filesizelimit = 3 * 1024 * 1024 * 1024ull;
				}
				else m_filesizelimit = (uint32_t)filesizelimit;
			}
			else if (strnicmp(pline, "filenumlimit", p - pline) == 0)
			{
				m_filenumlimit = atou32(pval);
			}
			else
			{
				printf("error line:%s\n", line);
			}
		}
		else
		{
			printf("error line:%s\n", line);
		}
	} while ((pline = fgets(line, 4096, f)) != nullptr);

	fclose(f);
	return ret;
}
