/**
# -*- coding:UTF-8 -*-
*/

#include "json_writer.hpp"
#include "string_utility.h"

#ifdef __GNUC__
#include <alloca.h>
#elif defined(_WIN32)
#include <malloc.h>
#include <Windows.h>
#else
#error os not support
#endif

//目前只考虑下面几个控制字符
static const char json_writer_escape_table[256] =
{
	0,0,0,0,0,0,0,0,'b','t','n',0,'f','r',0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,'"',0,0,0,0,0,0,0,0,0,0,0,0,'/',
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,'\\',0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static uint32_t translate_utf8_char_to_ucs4(const char* s, const char** endptr)
{
	uint32_t ucs4 = 0;
	const uint8_t* p = (const uint8_t*)s;
	if (*p < 0xC0)
	{
		++p;
		goto L_END;
	}
	else if (*p < 0xE0)
	{
		ucs4 = *p & 0x1f;
		goto L_2_BYTES;
	}
	else if (*p < 0xF0)
	{
		ucs4 = *p & 0xf;
		goto L_3_BYTES;
	}
	else if (*p < 0xF8)
	{
		ucs4 = *p & 0x7;
		goto L_4_BYTES;
	}
	else if (*p < 0xFC)
	{
		ucs4 = *p & 0x3;
		goto L_5_BYTES;
	}
	else if (*p < 0xFE)
	{
		ucs4 = *p & 0x1;
		goto L_6_BYTES;
	}
	else
	{
		++p;
		goto L_END;
	}

L_6_BYTES:
	if ((*++p & 0xC0) != 0x80) goto L_END;
	ucs4 = (ucs4 << 6) | (*p & 0x3f);
L_5_BYTES:
	if ((*++p & 0xC0) != 0x80) goto L_END;
	ucs4 = (ucs4 << 6) | (*p & 0x3f);
L_4_BYTES:
	if ((*++p & 0xC0) != 0x80) goto L_END;
	ucs4 = (ucs4 << 6) | (*p & 0x3f);
L_3_BYTES:
	if ((*++p & 0xC0) != 0x80) goto L_END;
	ucs4 = (ucs4 << 6) | (*p & 0x3f);
L_2_BYTES:
	if ((*++p & 0xC0) != 0x80) goto L_END;
	ucs4 = (ucs4 << 6) | (*p & 0x3f);

	++p;

L_END:
	if (endptr)
	{
		*endptr = (const char*)p;
	}

	return ucs4;
}

static void escape_utf8(char** p_dst, const char** p_s)
{
	uint32_t ucs4 = translate_utf8_char_to_ucs4(*p_s, p_s);
	if (ucs4 != 0)
	{
		if (ucs4 & 0xffff0000) //ignore
		{
		}
		else
		{
			*(*p_dst)++ = '\\';
			*(*p_dst)++ = 'u';
			*(*p_dst)++ = int2hex_lower(ucs4 >> 12);
			*(*p_dst)++ = int2hex_lower((ucs4 >> 8) & 0xf);
			*(*p_dst)++ = int2hex_lower((ucs4 >> 4) & 0xf);
			*(*p_dst)++ = int2hex_lower(ucs4 & 0xf);
		}
	}
	else
	{
		*(*p_dst)++ = *(*p_s)++;
	}
}

static int32_t jo_escape_utf8_string(char* dst, const char* s, uint32_t len)
{
	const char* in = s;
	const char* end = in + len;
	char* out = dst;
	while (in < end)
	{
		if (*in & 0x80)
		{
			escape_utf8(&out, &in);
		}
		else
		{
			if (*in != 0)
			{
				if (json_writer_escape_table[CHAR_TO_INT(*in)])
				{
					*out++ = '\\';
					*out++ = json_writer_escape_table[CHAR_TO_INT(*in)];
				}
				else *out++ = *in;
			}
			++in;
		}
	}
	return out - dst;
}

static int32_t jo_escape_utf16_string(char* dst, const wchar_t* s, uint32_t len)
{
	char* out = dst;
	for (uint32_t i = 0; i < len; ++i)
	{
		if (s[i] & 0xff00)
		{
			*out++ = '\\';
			*out++ = 'u';
			*out++ = int2hex_lower((uint16_t)s[i] >> 12);
			*out++ = int2hex_lower(((uint16_t)s[i] >> 8) & 0xf);
			*out++ = int2hex_lower(((uint16_t)s[i] >> 4) & 0xf);
			*out++ = int2hex_lower((uint16_t)s[i] & 0xf);
		}
		else
		{
			if (json_writer_escape_table[CHAR_TO_INT(s[i + 1])])
			{
				*out++ = '\\';
				*out++ = json_writer_escape_table[CHAR_TO_INT(s[i + 1])];
			}
			else *out++ = CHAR_TO_INT(s[i + 1]);
		}
	}
	return out - dst;
}

void JOWriter::add_value_string(const char* s, uint32_t len)
{
	char* buf = (char*)alloca(len * 3 + 3);
	int32_t esc_len = 0;
	buf[esc_len++] = '"';
	if (m_input_codepage == CodePage::ASCII ||
		m_input_codepage == CodePage::UTF8)
	{
		esc_len += jo_escape_utf8_string(buf + esc_len, s, len);
	}
	else
	{
#ifdef _WIN32
		int32_t ucs2len = len;
		wchar_t* ucs2 = (wchar_t*)alloca(ucs2len * 2 + 1);
		if (m_input_codepage == CodePage::GBK)
		{
			ucs2len = MultiByteToWideChar(936, 0, s, len, ucs2, ucs2len);
		}
		else if (m_input_codepage == CodePage::UTF16)
		{
		}
		else if (m_input_codepage == CodePage::BIG5)
		{
			ucs2len = MultiByteToWideChar(950, 0, s, len, ucs2, ucs2len);
		}
		else
		{ // impossible
			assert(0);
		}
		assert(ucs2len != 0);
		esc_len += jo_escape_utf16_string(buf + esc_len, ucs2, ucs2len);
#endif
	}
	buf[esc_len++] = '"';
	append(buf, esc_len);
}

void JOWriter::add_value_int32(int32_t n)
{
	char buf[16];
	int32_t len = 0;
	len += i32toa(n, buf+len);
	append(buf, len);
}

void JOWriter::add_value_uint32(uint32_t n)
{
	char buf[16];
	int32_t len = 0;
	len += u32toa(n, buf+len);
	append(buf, len);
}

void JOWriter::add_value_int64(int64_t n)
{
	char buf[32];
	int32_t len = 0;
	buf[len++] = '"';
	len += i64toa(n, buf+len);
	buf[len++] = '"';
	append(buf, len);
}

void JOWriter::add_value_uint64(uint64_t n)
{
	char buf[32];
	int32_t len = 0;
	buf[len++] = '"';
	len += u64toa(n, buf+len);
	buf[len++] = '"';
	append(buf, len);
}

void JOWriter::add_value_bool(bool n)
{
	if (n)
	{
		append("true", strlen("true"));
	}
	else
	{
		append("false", strlen("false"));
	}
}

void JOWriter::object_add_name(const char* s, uint32_t len)
{
	char* buf = (char*)alloca(len * 3 + 3);
	int32_t esc_len = 0;
	char* json_string = get_string();
	uint32_t json_len = get_length();
	if (json_len != 0 && json_string[json_len - 1] != '{')
	{
		buf[esc_len++] = ',';
	}
	buf[esc_len++] = '"';
	esc_len += jo_escape_utf8_string(buf + esc_len, s, len);
	buf[esc_len++] = '"';
	buf[esc_len++] = ':';
	append(buf, esc_len);
}

void JOWriter::object_begin()
{
	char* json_string = get_string();
	uint32_t json_len = get_length();
	if (json_len != 0 && json_string[json_len - 1] != '{'
		&& json_string[json_len - 1] != '[' && json_string[json_len - 1] != ':')
	{
		push_back(',');
	}
	push_back('{');
}

void JOWriter::object_begin(const char* name)
{
	object_add_name(name, strlen(name));
	object_begin();
}

void JOWriter::object_add_string(const char* name, const char* s, uint32_t len)
{
	object_add_name(name, strlen(name));
	add_value_string(s, len);
}

void JOWriter::object_add_int32(const char* name, int32_t n)
{
	object_add_name(name, strlen(name));
	add_value_int32(n);
}

void JOWriter::object_add_uint32(const char* name, uint32_t n)
{
	object_add_name(name, strlen(name));
	add_value_uint32(n);
}

void JOWriter::object_add_int64(const char* name, int64_t n)
{
	object_add_name(name, strlen(name));
	add_value_int64(n);
}

void JOWriter::object_add_uint64(const char* name, uint64_t n)
{
	object_add_name(name, strlen(name));
	add_value_uint64(n);
}

void JOWriter::object_add_bool(const char* name, bool n)
{
	object_add_name(name, strlen(name));
	add_value_bool(n);
}

void JOWriter::object_end()
{
	push_back('}');
}


void JOWriter::array_add_comma()
{
	char* json_string = get_string();
	uint32_t json_len = get_length();
	if (json_len != 0 && json_string[json_len - 1] != '[')
	{
		push_back(',');
	}
}

void JOWriter::array_begin()
{
	char* json_string = get_string();
	uint32_t json_len = get_length();
	if (json_len != 0 && json_string[json_len - 1] != '{'
		&& json_string[json_len - 1] != '[' && json_string[json_len - 1] != ':')
	{
		push_back(',');
	}
	push_back('[');
}

void JOWriter::array_begin(const char* name)
{
	object_add_name(name, strlen(name));
	array_begin();
}

void JOWriter::array_add_string(const char* s, uint32_t len)
{
	array_add_comma();
	add_value_string(s, len);
}

void JOWriter::array_add_int32(int32_t n)
{
	array_add_comma();
	add_value_int32(n);
}

void JOWriter::array_add_uint32(uint32_t n)
{
	array_add_comma();
	add_value_uint32(n);
}

void JOWriter::array_add_int64(int64_t n)
{
	array_add_comma();
	add_value_int64(n);
}

void JOWriter::array_add_uint64(uint64_t n)
{
	array_add_comma();
	add_value_uint64(n);
}

void JOWriter::array_add_bool(bool n)
{
	array_add_comma();
	add_value_bool(n);
}

void JOWriter::array_end()
{
	push_back(']');
}

#if 0
/*********** example ************/
const char* dest_json_string = 
"{"
"	\"task_number\" : 2,"
"	\"task_list\" :"
"	["
"		{"
"			\"name\": \"taks1\","
"			\"size\": 12345,"
"		},"
"		{"
"			\"name\": \"taks2\","
"			\"size\": 12346,"
"		}"
"	]"
"}";

struct task_info
{
	char* name;
	uint64_t size;
}task_list[2];

void ti_to_json_string(JOWriter& jo, const struct task_info* task)
{
	jo.object_begin();
	jo.object_add_string("name", task->name, strlen(task->name));
	jo.object_add_uint64("name", task->size);
	jo.object_end();
}

void test_json_writer()
{
	JOWriter jo;
	task_list[0].name = "task1";
	task_list[0].size = 12345;
	task_list[1].name = "task2";
	task_list[1].size = 12346;
	jo.object_begin();
	jo.object_add_uint32("task_number", 2);
	jo.array_begin();
	for(int i = 0 ; i<2; ++i)
	{
		JO_ARRAY_ADD_OBJECT(jo, &task_list[i], ti_to_json_string);
	}
	jo.array_end();
	jo.object_end();
	printf("%.*s\n", jo.get_length(), jo.get_string());
}

static void test_add_array(JOWriter& jw, int32_t a1, int32_t a2)
{
	jw.array_begin();
	jw.array_add_bool(!!a1);
	jw.array_begin();
	jw.array_add_int32(a2);
	jw.array_end();
	jw.array_add_string("a b	cd'\"'ef", 11);
	const char* p = "中华人民共和国";
	jw.array_add_string(p, strlen(p));
	jw.array_end();
}

void test_jo_writer()
{
	JOWriter jw;
	jw.set_input_codepage(CodePage::GBK);
	jw.object_begin();
	jw.object_add_int32("n1", 1);

	JO_OBJECT_ADD_ARRAY(jw, "a\\r\t\fr", 1, test_add_array, 2);
	//same as below
	//object_add_name("a\\r\t\fr", strlen("a\\r\t\fr"));
	//test_add_array(1, 2);

	jw.object_add_int32("n2\x0d\x0a", 2);
	jw.object_end();
	jw.push_back('\x00');
	printf("%s\n", jw.get_string());
}

#if 0
int main()
{
	test_json_writer();
	test_jo_writer();
	return 0;
}
#endif

#endif
