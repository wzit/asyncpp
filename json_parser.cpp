/**
# -*- coding:UTF-8 -*-
*/

#include "json_parser.hpp"
#include "logger.hpp"
#include <assert.h>

JO::JO()
	: m_jo_data({nullptr})
	, m_type(jo_type_t::null)
{
}

JO::JO(jo_type_t type)
	: JO()
{
	set_type(type);
}

JO::~JO()
{
	if (m_type == jo_type_t::object)
		delete m_object_members;
	else if (m_type == jo_type_t::array)
		delete m_array_elements;
}

JO::JO(JO&& val)
{
	*this = std::move(val);
}

JO& JO::operator=(JO&& val)
{
	if (this != &val)
	{
		m_jo_data = val.m_jo_data; val.m_jo_data = {nullptr};
		m_type = val.m_type;
	}
	return *this;
}

//目前只考虑下面几个控制字符
static const char json_parser_escape_table[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, '"', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '/', //不转义/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0,
	0, 0, 0x08, 0, 0, 0, 0x0c, 0, 0, 0, 0, 0, 0, 0, 0x0a, 0,
	0, 0, 0x0d, 0, 0x09, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define MAX_JSON_LEN (1*1024*1024*1024)
static int32_t expect_token(char** p_cursor, uint32_t remain_len, char token)
{
	if (remain_len > 0)
	{
		if (**p_cursor == token)
		{
			++*p_cursor;
			return 0;
		}
		else return -1;
	}
	else return -1;
}

static void transfer_char(char** s, char c)
{
	if (c != 0) //暂不过滤其他字符
	{
		**s = c;
		++*s;
	}
}

static int32_t transfer_ucs2_to_utf8(char** s, char** p_cursor, uint32_t remain_len, char quote)
{
	(void)quote;
	if (remain_len < 5 || remain_len > MAX_JSON_LEN) return -1;
	if (isxdigit((*p_cursor)[0]) && isxdigit((*p_cursor)[1])
		&& isxdigit((*p_cursor)[2]) && isxdigit((*p_cursor)[3]))
	{
		uint8_t hi = (get_hexvalue((*p_cursor)[0]) << 4) | get_hexvalue((*p_cursor)[1]);
		uint8_t lo = (get_hexvalue((*p_cursor)[2]) << 4) | get_hexvalue((*p_cursor)[3]);
		
		if (hi&0xf8) //3B
		{
			*(*s)++ = 0xE0 | (hi >> 4);
			*(*s)++ = 0x80 | ((hi & 0xf) << 2) | (lo >> 6);
			*(*s)++ = 0x80 | (lo & 0x3f);
		}
		else if (hi) //2B
		{
			*(*s)++ = 0xC0 | ((hi & 0x07) << 2) | (lo >> 6);
			*(*s)++ = 0x80 | (lo & 0x3f);
		}
		else  //1B
		{
			transfer_char(s, lo);
		}
	}
	else return -1;
	*p_cursor += 4;
	return 0;
}

int32_t JO::parse_string(char** s, char** p_cursor, uint32_t remain_len, char quote)
{
	int32_t ret = 0;
	char* cursor = *p_cursor;
	char* end = cursor + remain_len;

	//if (remain_len > MAX_JSON_LEN) return -1;

	while (*cursor != quote && cursor < end)
	{
		if (*cursor != '\\')
		{
			transfer_char(s, *cursor);
			++cursor;
		}
		else
		{
			++cursor;
			if (*cursor == 'u')
			{
				++cursor;
				ret = transfer_ucs2_to_utf8(s, &cursor, static_cast<uint32_t>(end - cursor), quote);
				if (ret != 0)
				{
					*p_cursor = cursor;
					return ret;
				}
			}
			else if (json_parser_escape_table[CHAR_TO_INT(*cursor)])
			{
				*(*s)++ = json_parser_escape_table[CHAR_TO_INT(*cursor++)];
			}
			else if (*cursor == 'x')
			{
				++cursor;
				if (isxdigit(cursor[0]) && isxdigit(cursor[1]))
				{
					transfer_char(s, (get_hexvalue(cursor[0]) << 4) | get_hexvalue(cursor[1]));
					cursor += 2;
				}
				else
				{
					*p_cursor = cursor;
					return -1;
				}
			}
			else
			{
				*(*s)++ = cursor[-1];
				*(*s)++ = cursor[0];
				++cursor;
			}
		}
	}

	if (cursor < end)
	{
		**s = 0;
		++cursor;
		*p_cursor = cursor;
		return 0;
	}
	else return -1;
}

const char* JO::parse_key(char** p_cursor, uint32_t remain_len)
{
	char* key;
	char* s;
	char quote;

	//if (remain_len > MAX_JSON_LEN) return nullptr;

	skip_space(*p_cursor);
	if (**p_cursor != '"' && **p_cursor != '\'')
	{
		return nullptr;
	}

	quote = **p_cursor;
	s = ++*p_cursor;
	key = s;

	if(parse_string(&s, p_cursor, remain_len, quote) == 0) return key;
	else return nullptr;
}

int32_t JO::parse_value_string(char** p_cursor, uint32_t remain_len, char quote)
{
	char* s = *p_cursor;

	if (remain_len > MAX_JSON_LEN) return -1;

	set_type(jo_type_t::string);
	m_jo_value = s;

	return parse_string(&s, p_cursor, remain_len, quote);
}

int32_t JO::parse_value_bool(char** p_cursor, uint32_t remain_len)
{
	(void)remain_len;
	//skip_space(*p_cursor);

	set_type(jo_type_t::boolean);
	m_jo_value = *p_cursor;
	if (memcmp(*p_cursor, "true", 4) == 0)
	{
		*p_cursor += 4;
	}
	else if (memcmp(*p_cursor, "false", 5) == 0)
	{
		*p_cursor += 5;
	}
	else return -1;
	return 0;
}

int32_t JO::parse_value_number(char** p_cursor, uint32_t remain_len)
{
	(void)remain_len;
	//if (remain_len > MAX_JSON_LEN) return -1;

	set_type(jo_type_t::number);
	m_jo_value = *p_cursor;
	strtod(*p_cursor, p_cursor); //暂不将数字结尾置0
	return 0;
}

void JO::set_type(jo_type_t type)
{
	switch (type)
	{
	case jo_type_t::null:
		m_type = type;
		break;
	case jo_type_t::boolean:
		m_type = type;
		break;
	case jo_type_t::number:
		m_type = type;
		break;
	case jo_type_t::object:
		m_type = type;
		m_jo_data.object_members = reinterpret_cast<void*>(new jo_map_t);
		break;
	case jo_type_t::array:
		m_type = type;
		m_array_elements = new std::vector<JO>;
		break;
	case jo_type_t::string:
		m_type = type;
		break;
	}
}

int32_t JO::parse_value_array(char** p_cursor, uint32_t remain_len)
{
	int32_t ret = -1;
	char* cursor = *p_cursor;
	char* end = cursor + remain_len;

	//if (remain_len > MAX_JSON_LEN) return -1;

	set_type(jo_type_t::array);

	while (cursor <= end)
	{

		skip_space(cursor);
		if (*cursor == ']')
		{
			++cursor;
			ret = 0;
			break;
		}


		if (*cursor == '"' || *cursor == '\'') //string
		{
			JO jo;
			char quote = *cursor;
			++cursor;
			ret = jo.parse_value_string(&cursor, static_cast<uint32_t>(end - cursor), quote);
			if (ret != 0) break;
			else m_array_elements->push_back(std::move(jo));
		}
		else if (*cursor == '{') //object
		{
			JO jo;
			++cursor;
			ret = jo.parse_value_object(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_array_elements->push_back(std::move(jo));
		}
		else if (*cursor == '[') //array
		{
			JO jo;
			++cursor;
			ret = jo.parse_value_array(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_array_elements->push_back(std::move(jo));
		}
		else if (isdigit(*cursor) || *cursor == '-') //number
		{
			JO jo;
			ret = jo.parse_value_number(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_array_elements->push_back(std::move(jo));
		}
		else if (*cursor == 't' || *cursor == 'f') //true,false
		{
			JO jo;
			ret = jo.parse_value_bool(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_array_elements->push_back(std::move(jo));
		}
		else if (*cursor == 'n') //null
		{
			if (cursor[1] == 'u' && cursor[2] == 'l' && cursor[3] == 'l')
			{
				JO jo;
				cursor += 4;
				ret = 0;
				jo.set_type(jo_type_t::null);
				m_array_elements->push_back(std::move(jo));
			}
			else
			{
				ret = -1;
				break;
			}
		}
		else
		{
			ret = -1;
			break;
		}

		skip_space(cursor);
		if (*cursor != ']')
		{
			ret = expect_token(&cursor, static_cast<uint32_t>(end - cursor), ',');
			if (ret != 0) break;
		}
	}
	*p_cursor = cursor;
	return ret;
}

int32_t JO::parse_value_object(char** p_cursor, uint32_t remain_len)
{
	int32_t ret = -1;
	char* cursor = *p_cursor;
	char* end = cursor + remain_len;

	//if (remain_len > MAX_JSON_LEN) return -1;

	set_type(jo_type_t::object);

	while (cursor <= end)
	{
		skip_space(cursor);
		if (*cursor == '}')
		{ //object end
			++cursor;
			ret = 0;
			break;
		}

		JO jo;
		const char* key = jo.parse_key(&cursor, static_cast<uint32_t>(end - cursor));
		if (key == nullptr) break;

		skip_space(cursor);
		ret = expect_token(&cursor, static_cast<uint32_t>(end - cursor), ':');
		if (ret != 0) break;

		skip_space(cursor);
		if (*cursor == '"' || *cursor == '\'') //string
		{
			char quote = *cursor;
			++cursor;
			ret = jo.parse_value_string(&cursor, static_cast<uint32_t>(end - cursor), quote);
			if (ret != 0) break;
			else m_object_members->insert(std::make_pair(key, std::move(jo)));
		}
		else if (*cursor == '{') //object
		{
			++cursor;
			ret = jo.parse_value_object(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_object_members->insert(std::make_pair(key, std::move(jo)));
		}
		else if (*cursor == '[') //array
		{
			++cursor;
			ret = jo.parse_value_array(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_object_members->insert(std::make_pair(key, std::move(jo)));
		}
		else if (isdigit(*cursor) || *cursor == '-') //number
		{
			ret = jo.parse_value_number(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_object_members->insert(std::make_pair(key, std::move(jo)));
		}
		else if (*cursor == 't' || *cursor == 'f') //true,false
		{
			ret = jo.parse_value_bool(&cursor, static_cast<uint32_t>(end - cursor));
			if (ret != 0) break;
			else m_object_members->insert(std::make_pair(key, std::move(jo)));
		}
		else if (*cursor == 'n') //null
		{
			if (cursor[1] == 'u' && cursor[2] == 'l' && cursor[3] == 'l')
			{
				cursor += 4;
				ret = 0;
				jo.set_type(jo_type_t::null);
				m_object_members->insert(std::make_pair(key, std::move(jo)));
			}
			else
			{
				ret = -1;
				break;
			}
		}
		else
		{
			ret = -1;
			break;
		}

		skip_space(cursor);
		if (*cursor != '}')
		{
			ret = expect_token(&cursor, static_cast<uint32_t>(end - cursor), ',');
			if (ret != 0) break;
		}
	}
	*p_cursor = cursor;
	return ret;
}

bool JO::parse_inplace(char** json, uint32_t remain_len)
{
	int32_t ret = -1;
	char* cursor = *json;
	char* end = cursor + remain_len;

	if (remain_len > MAX_JSON_LEN) return false;

	skip_space(cursor);
	if (*cursor == '{')
	{
		++cursor;
		ret = parse_value_object(&cursor, static_cast<uint32_t>(end - cursor));
	}
	else if (*cursor == '[')
	{
		++cursor;
		ret = parse_value_array(&cursor, static_cast<uint32_t>(end - cursor));
	}
	else
	{
		ret = -1;
	}
	*json = cursor;
	if (ret != 0) return false;
	else return true;
}


jo_type_t JO::type() const
{
	return m_type;
}

int32_t JO::i32() const
{
	if (m_type == jo_type_t::number || m_type == jo_type_t::string)
	{
		return atoi(m_jo_value);
	}
	else if (m_type == jo_type_t::boolean)
	{
		return *m_jo_value == 't' ? 1 : 0;
	}
	else return 0;
}

int64_t JO::i64() const
{
	if (m_type == jo_type_t::number || m_type == jo_type_t::string)
	{
		return atoll(m_jo_value);
	}
	else if (m_type == jo_type_t::boolean)
	{
		return *m_jo_value == 't' ? 1 : 0;
	}
	else return 0;
}

double JO::dbl() const
{
	if (m_type == jo_type_t::number || m_type == jo_type_t::string)
	{
		return atof(m_jo_value);
	}
	else if (m_type == jo_type_t::boolean)
	{
		return *m_jo_value == 't' ? 1 : 0;
	}
	else return 0;
}

char* JO::str() const
{
	if (m_type == jo_type_t::string )
	{
		return m_jo_value;
	}
	else return NULL;
}

static char jo_str2_empty_string[4];
char* JO::str2() const
{
	if (m_type != jo_type_t::null)
	{
		return m_jo_value;
	}
	//else return "";
	else return jo_str2_empty_string;
}

bool 
JO::b() const
{
	if (m_type == jo_type_t::boolean) {
		return *m_jo_value == 't' ? true : false;
	} else {
		return false;
	}
}

const JO& JO::operator[](const char* key) const
{
	assert(m_type == jo_type_t::object);
	return m_object_members->find(key)->second;
}

jo_map_t::const_iterator jo_find(const JO& jo, const char* key)
{
	return reinterpret_cast<const jo_map_t*>(jo.m_jo_data.object_members)->find(key);
}
jo_map_t::const_iterator jo_begin(const JO& jo)
{
	return reinterpret_cast<const jo_map_t*>(jo.m_jo_data.object_members)->begin();
}
jo_map_t::const_iterator jo_end(const JO& jo)
{
	return reinterpret_cast<const jo_map_t*>(jo.m_jo_data.object_members)->end();
}

uint32_t JO::size() const
{
	if (m_type == jo_type_t::object)
	{
		return static_cast<uint32_t>(m_object_members->size());
	}
	else if (m_type == jo_type_t::array)
	{
		return static_cast<uint32_t>(m_array_elements->size());
	}
	else
	{
		return 1;
	}
}

bool JO::empty() const
{
	if (m_type == jo_type_t::object)
	{
		return m_object_members->empty();
	}
	else if (m_type == jo_type_t::array)
	{
		return m_array_elements->empty();
	}
	else
	{
		return false;
	}
}

const JO& JO::operator[](int32_t idx) const
{
	assert(m_type == jo_type_t::array);
	return (*m_array_elements)[idx];
}

#if 0
int main()
{
	char s[100] = "{\"a\\x20b\":\"123\\u6211abc\\/\x0d\x0avwx\",'b':[1,2,3,{'ba':'aa','ba':'bb'}],'c':123.12}";
	char* json = s;
	JO jo;
	if(!jo.parse_inplace(&json, strlen(json)))
	{
		printf("json parse fail at: %s\n", json);
		return -1;
	}
	char* s1 = jo["a b"].str();
	/*FILE* f;
	fopen_s(&f, "1", "w");
	fprintf(f, "%s\n", s1);
	fclose(f);*/
	printf("a b : %s\n", s1);
	const JO& b = jo["b"];
	const JO& b3 = b[3];
	printf("b0 : %d\n", b[0].i32());
	printf("b1 : %d\n", b[1].i32());
	printf("b2 : %d\n", b[2].i32());
	printf("ba : %s\n", b3["ba"].str());
	printf("c : %f\n", jo["c"].dbl());

	auto it = jo_find(b3, "ba");
	printf("ba : %s\n", it->second.str());
	it++;
	printf("ba : %s\n", it->second.str());
}
#endif
