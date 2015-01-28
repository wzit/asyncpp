#include "cfg.hpp"
#include "string_utility.h"
#include <assert.h>
#include <errno.h>
#ifdef _WIN32
#include <codecvt>
#else
#include <locale>
#endif

using namespace std;

static std::string _cfg_empty_string;

CFG::section_t& CFG::operator[](const char* section_name)
{
	auto it = sections.find(section_name);
	if (it != sections.end()) return it->second;
	else
	{
		auto r = sections.insert(std::make_pair(section_name, section_t()));
		assert(r.second);
		return r.first->second;
	}
}

const std::string& CFG::get(const char* section, const char* key) const
{
	auto it = sections.find(section);
	if (it != sections.end())
	{
		auto v = it->second.find(key);
		if (v != it->second.end())
			return v->second;
	}
	return _cfg_empty_string;
}

void CFG::set(const char* section, const char* key, const char* val)
{
	operator[](section)[key] = val;
}

int32_t CFG::getInt32(const char* section, const char* key) const
{
	return atoi32(get(section, key).c_str());
}

void CFG::setInt32(const char* section, const char* key, int32_t v)
{
	char buf[16];
	i32toa(v, buf);
	set(section, key, buf);
}

int64_t CFG::getInt64(const char* section, const char* key) const
{
	return atoi64(get(section, key).c_str());
}
void CFG::setInt64(const char* section, const char* key, int64_t v)
{
	char buf[32];
	i64toa(v, buf);
	set(section, key, buf);
}

int32_t CFG::flush()
{
#ifdef _WIN32
	FILE* f = _wfopen(cfg_path.c_str(), L"w");
#else
	FILE* f = fopen(cfg_path.c_str(), "w");
#endif
	if (f == nullptr)
	{
		printf("flush to file open %s fail%d[%s]\n", cfg_path.c_str(), errno, strerror(errno));
		return errno;
	}

	for (auto section : sections)
	{
		fprintf(f, "[%s]\n", section.first.c_str());
		for (auto item : section.second)
		{
			fprintf(f, "%s=%s\n", item.first.c_str(), item.second.c_str());
		}
	}
	fclose(f);
	return 0;
}

int32_t CFG::flush(const char* cfg_file)
{
#ifdef _WIN32
	wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> utf16conv;
	cfg_path = utf16conv.from_bytes(cfg_file);
#else
	cfg_path = cfg_file;
#endif
	return flush();
}

int32_t CFG::read(const char* cfg_file)
{
	int32_t ret = 0;
	char line[4100];
#ifdef _WIN32
	wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> utf16conv;
	cfg_path = utf16conv.from_bytes(cfg_file);
	FILE* f = _wfopen(cfg_path.c_str(), L"r");
#else
	cfg_path = cfg_file;
	FILE* f = fopen(cfg_path.c_str(), "r");
#endif
	if (f == nullptr)
	{
		printf("load cfg, open %s fail:%d[%s]\n", cfg_file, errno, strerror(errno));
		return errno;
	}

	char* pline = fgets(line, 4096, f);
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
	do
	{
		skip_space(pline);
		int n = strlen(pline);
		if (n == 0) continue;
		while (isspace(pline[--n]));
		pline[n + 1] = 0;

		if (pline[0] == ';' || pline[0] == '#') continue;
		if (pline[0] != '[')
		{
			printf("expect '[', ignore invalid line:%s", line);
			continue;
		}
		char* pe = strchr(pline + 1, ']');
		if (pe == nullptr)
		{
			printf("expect ']', ignore invalid line:%s", line);
			continue;
		}
		std::string section_name(pline + 1, pe);
		sections.insert(std::make_pair(section_name, section_t()));
		ret = read_next_section(sections[section_name], line, f);
	} while ((pline = fgets(line, 4096, f)) != nullptr);

	fclose(f);
	return ret;
}

int32_t CFG::read_next_section(section_t& section, char* line, FILE* f)
{
	for (;;)
	{
		char* pline = fgets(line, 4096, f);
		if (pline == nullptr)
		{
			if (feof(f)) return 0;
			else return EINVAL;
		}
		skip_space(pline);
		int n = strlen(pline);
		if (n == 0) continue;
		while (isspace(pline[--n]));
		pline[n + 1] = 0;

		if (pline[0] == ';' || pline[0] == '#') continue;
		if (pline[0] == '[')
		{
			char* pe = strchr(pline + 1, ']');
			if (pe == nullptr)
			{
				printf("expect ']', ignore invalid line:%s", line);
				continue;
			}
			std::string section_name(pline + 1, pe);
			sections.insert(std::make_pair(section_name, section_t()));
			return read_next_section(sections[section_name], line, f);
		}
		else
		{
			char* pv = strchr(pline, '=');
			if (pv == nullptr)
			{
				printf("expect '=', ignore invalid line:%s", line);
				continue;
			}
			char* pe = pv - 1;
			while (isspace(*pe)) --pe;
			while (isspace(*++pv));
			auto r = section.insert(std::make_pair(std::string(pline, pe + 1), pv));
			if (!r.second)
			{
				printf("ignore duplicate key:%s", line);
			}
		}
	}
}
