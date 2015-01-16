#ifndef _CFG_HPP_
#define _CFG_HPP_

#include <unordered_map>
#include <string>
#include <stdint.h>
#include <stdio.h>

class CFG
{
private:
	typedef std::unordered_map<std::string, std::string> section_t;
	std::unordered_map<std::string, section_t> sections;
	std::string cfg_path;

public:
	CFG() = default;
	CFG(const CFG&) = default;
	CFG& operator=(const CFG&) = default;
	~CFG() = default;

public:
	int32_t read(const char* cfg_file);
	section_t& operator[](const char*); //read&write cfg item
	const std::string& get(const char* section, const char* key) const; //read cfg item
	void set(const char* section, const char* key, const char* val); //write cfg item
	int32_t getInt32(const char* section, const char* key) const; //read int cfg item
	void setInt32(const char* section, const char* key, int32_t v); //write int cfg item
	int64_t getInt64(const char* section, const char* key) const; //read int cfg item
	void setInt64(const char* section, const char* key, int64_t v); //write int cfg item
	int32_t flush();
	int32_t flush(const char* cfg_file);

private:
	int32_t read_next_section(section_t& section, char* line, FILE* f);
};

#endif
