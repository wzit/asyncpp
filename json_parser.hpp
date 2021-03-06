﻿/**
# -*- coding:UTF-8 -*-
*/

#ifndef _JSON_PARSER_HPP_
#define _JSON_PARSER_HPP_

#include "string_utility.h"
#include <unordered_map>
#include <vector>

enum class jo_type_t
{
	null,
	boolean,
	number,
	object,
	array,
	string
};

class JO
{
public:
	union u_jo_data
	{
		char* value;
		void* object_members;
		std::vector<JO>* array_elements;
	}m_jo_data;
#define m_object_members reinterpret_cast<jo_map_t*>(m_jo_data.object_members)
#define m_array_elements m_jo_data.array_elements
#define m_jo_value m_jo_data.value
	jo_type_t m_type;

public:
	JO();
	JO(jo_type_t type);
	~JO();
	JO(const JO&) = delete;
	JO& operator=(const JO&) = delete;
	JO(JO&& val);
	JO& operator=(JO&& val);

public:
	int32_t i32() const;
	int64_t i64() const;
	char* str() const; //非string类型返回NULL
	char* str2() const; //非string类型转为string，null类型返回空字符串
	double dbl() const;
	bool b() const;
	jo_type_t type() const;

public:
	bool parse_inplace(char** json, uint32_t json_len);

	bool empty() const;
	uint32_t size() const;

	const JO& operator[](const char* key) const;
	const JO& operator[](int32_t idx) const;

private:
	int32_t parse_string(char** s, char** p_cursor, uint32_t remain_len, char quote);

	const char* parse_key(char** p_cursor, uint32_t remain_len);
	int32_t parse_value_string(char** p_cursor, uint32_t remain_len, char quote);
	int32_t parse_value_number(char** p_cursor, uint32_t remain_len);
	int32_t parse_value_bool(char** p_cursor, uint32_t remain_len);
	int32_t parse_value_object(char** p_cursor, uint32_t remain_len);
	int32_t parse_value_array(char** p_cursor, uint32_t remain_len);

private:
	void set_type(jo_type_t type);
};

struct CstrHashFunc
{
	size_t operator()(const char* s) const
	{
		return time33_hash(s);
	}
	size_t operator()(const char* s1, const char* s2) const
	{
		return strcmp(s1, s2) == 0;
	}
};

typedef std::unordered_multimap<const char*, JO, CstrHashFunc, CstrHashFunc> jo_map_t;
//typedef std::unordered_multimap<std::string, JO> jo_map_t;

jo_map_t::const_iterator jo_begin(const JO& jo);
jo_map_t::const_iterator jo_end(const JO& jo);
jo_map_t::const_iterator jo_find(const JO& jo, const char* key);

#endif
