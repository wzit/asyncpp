/**
# -*- coding:UTF-8 -*-
*/

#ifndef _JSON_WRITER_H_
#define _JSON_WRITER_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

class JOWriter
{
private:
	char* m_buf;
	uint32_t m_buf_len;
	uint32_t m_json_len;
private:
	void enlarge_buffer(uint32_t new_size)
	{
		m_buf = (char*)realloc((void*)m_buf, new_size);
		assert(m_buf != nullptr);
		m_buf_len = new_size;
	}
public:
	JOWriter()
		: m_buf(nullptr)
		, m_buf_len(0)
		, m_json_len(0)
	{
	}
	JOWriter(uint32_t init_buf_len)
	{
		m_buf = (char*)malloc(init_buf_len);
		assert(m_buf != nullptr);
		m_buf_len = init_buf_len;
		m_json_len = 0;
	}
	~JOWriter(){ free(m_buf); }
	JOWriter(const JOWriter&) = delete;
	JOWriter& operator=(const JOWriter&) = delete;
public:
	char* get_string(){ return m_buf; }
	uint32_t get_length(){ return m_json_len; }
	void reserve(uint32_t n)
	{
		if (n > m_buf_len)
		{
			enlarge_buffer(n);
		}
	}
	void attach_buffer(char* buffer, uint32_t buffer_size)
	{
		m_buf = buffer;
		m_buf_len = buffer_size;
		m_json_len = 0;
	}
	char* detach_buffer()
	{
		char* p = m_buf;
		m_buf = nullptr;
		m_buf_len = 0;
		m_json_len = 0;
		return p;
	}
	void append(const char* p, uint32_t len)
	{
		uint32_t new_len = len + m_json_len;
		if (new_len > m_buf_len)
		{
			enlarge_buffer(new_len* 4);
		}
		memcpy(m_buf + m_json_len, p, len);
		m_json_len = new_len;
	}
	void push_back(char c)
	{
		uint32_t pos = m_json_len++;
		if (m_json_len > m_buf_len)
		{
			enlarge_buffer(m_json_len * 4);
		}
		m_buf[pos] = c;
	}

	void object_begin();
	void object_begin(const char* name);
	void object_add_string(const char* name, const char* s, uint32_t len);
	void object_add_int32(const char* name, int32_t n);
	void object_add_uint32(const char* name, uint32_t n);
	void object_add_int64(const char* name, int64_t n);
	void object_add_uint64(const char* name, uint64_t n);
	void object_add_bool(const char* name, bool n);
	void object_end();

	void array_begin();
	void array_begin(const char* name);
	void array_add_string(const char* s, uint32_t len);
	void array_add_int32(int32_t n);
	void array_add_uint32(uint32_t n);
	void array_add_int64(int64_t n);
	void array_add_uint64(uint64_t n);
	void array_add_bool(bool n);
	void array_end();

	void object_add_name(const char* s, uint32_t len);

private:
	void add_value_string(const char* s, uint32_t len);
	void add_value_int32(int32_t n);
	void add_value_uint32(uint32_t n);
	void add_value_int64(int64_t n);
	void add_value_uint64(uint64_t n);
	void add_value_bool(bool n);

	void array_add_comma();
};

#define JO_OBJECT_ADD_OBJECT(jo, name, json_obj, to_json_string_func, ...) \
{\
	jo.object_add_name(name, strlen(name)); \
	to_json_string_func(jo, json_obj, ##__VA_ARGS__); \
}

#define JO_OBJECT_ADD_ARRAY(jo, name, json_obj, to_json_string_func, ...) JO_OBJECT_ADD_OBJECT(jo, name, json_obj, to_json_string_func, ##__VA_ARGS__)

#define JO_ARRAY_ADD_OBJECT(jo, json_obj, to_json_string_func, ...)  to_json_string_func(jo, json_obj, ##__VA_ARGS__)
#define JO_ARRAY_ADD_ARRAY(jo, name, json_obj, to_json_string_func, ...) JO_ARRAY_ADD_OBJECT(jo, name, json_obj, to_json_string_func, ##__VA_ARGS__)

#endif
