/**
# -*- coding:UTF-8 -*-
*/

#include "http_utility.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#pragma warning(disable:4996)
#endif

/*************************** http query string parser implement ****************************/

void HttpQueryStringParser::insert_param(uint32_t k, char* val, uint32_t val_len)
{
	m_params.insert(std::make_pair(k, std::make_pair(val, val_len)));
}

uint32_t HttpQueryStringParser::get_path_hash_value()
{
	return m_path_hash_value;
}

const char* HttpQueryStringParser::get_path()
{
	return m_path;
}

HttpQueryStringParser::value_t
HttpQueryStringParser::get_param_by_hash_value(const uint32_t k)
{
	const auto& it = m_params.find(k);
	if (it != m_params.end()) return (*it).second;
	else return{ nullptr, 0 };
}

HttpQueryStringParser::value_t
HttpQueryStringParser::get_param_by_name(const char* name)
{
	return get_param_by_hash_value(time33_hash(name));
}

int32_t HttpQueryStringParser::parse_url_inplace(char* url, uint32_t len)
{
	m_params.clear();

	//cmd
	if (strnicmp(url, "http://", strlen("http://")) == 0)
	{
		m_path = url + strlen("http://");
		while (*m_path != '/')
		{
			if (*m_path == 0) return EPROTO;
			++m_path;
		}
		++m_path;
	}
	else m_path = url + 1;

	url[len] = '?';
	char* p_cur = (char*)m_path;
	while(*p_cur!='?') ++p_cur;
	*p_cur = 0;
	m_path_hash_value = time33_hash(m_path);

	if (p_cur - url == static_cast<int32_t>(len)) return 0;

	//param
	url[len] = '&';
	url[len+1] = 0;
	char* p_val;
	char* p_param = ++p_cur;
	while(*p_cur){
		while(*p_cur!='='){
			if(*p_cur=='&')
				goto LABLE_NEXT_PARAM;
			++p_cur;
		}
		*p_cur = 0;
		p_val = ++p_cur;
		while(*p_cur!='&')++p_cur;
		*p_cur = 0;
		insert_param(time33_hash(p_param), p_val, p_cur-p_val);
LABLE_NEXT_PARAM:
		p_param = ++p_cur;
	}

	url[len] = 0;
	return 0;
}

int32_t http_package_parse_inplace(char* package, uint32_t package_len,
									char** p_head, uint32_t* p_head_len,
									char** p_body, uint32_t* p_body_len)
{
	char* p = package + 32; //按照规范，http头一般应大于32个字节
	char* end = package + package_len - 4;
	for( ; p<=end; ++p)
	{
		if(p[0]=='\r' && p[1]=='\n' &&
			p[2]=='\r' && p[3]=='\n')
		{
			if (p_head != NULL)
			{
				*p_head = package;
				*p_head_len = p-package;
			}

			if (p_body != NULL)
			{
				*p_body = p + 4;
				*p_body_len = package_len - (p - package) - 4;
			}

			//*p = 0;
			return 0;
		}
	}
	return EPROTO;
}

int32_t http_get_header_len(char* package, uint32_t package_len)
{
	char* p = package + 32; //按照规范，http头一般应大于32个字节
	char* end = package + package_len - 4;
	for (; p <= end; ++p)
	{
		if (p[0] == '\r' && p[1] == '\n' &&
			p[2] == '\r' && p[3] == '\n')
		{
			return p - package + 4;
		}
	}
	return 0;
}

static char* http_move_to_next_line(char* cur_line, char* package, uint32_t package_len)
{
	char* p = cur_line;
	char* end = package + package_len;// - 1;
	for( ; p<end; ++p)
	{
		if(/*p[0]=='\r' && p[1]=='\n'*/ *p == '\n')
		{
			++p; //p += 2;
			return p < end ? p : NULL;
		}
	}
	return NULL;
}

int32_t http_get_request_header(char* header, uint32_t header_len,
								const char* name, uint32_t name_len,
								char** p_value, uint32_t* p_value_len)
{
	char* line = header;
	char* end = header + header_len;
	do
	{
		if(memcmp(name, line, name_len) == 0)
		{
			char* p = line + 1;
			while(*p != ':')
			{
				if(*p=='\r' || p>=end)
					return EPROTO;
				++p;
			}
			++p;
			skip_space(p);
			*p_value = p;
			while(*p!='\r' && p<end) ++p;
			*p_value_len = p - *p_value;
			return 0;
		}
		line = http_move_to_next_line(line, header, header_len);
	}while(line);

	return ENOENT;
}

static char* http_cookie_move_to_next_item(char* cur_item, char* cookie, uint32_t cookie_len)
{
	char* p = cur_item + 3; //按照规范，cookie项至少大于3字节
	char* end = cookie + cookie_len;
	for( ; p<end; ++p)
	{
		if(*p == ';')
		{
			++p;
			skip_space(p);
			return p;
		}
	}
	return NULL;
}

int32_t http_get_cookie_item(char* cookie, uint32_t cookie_len,
							 const char* name, uint32_t name_len,
							 char** p_value, uint32_t* p_value_len)
{
	char* item = cookie;
	char* end = cookie + cookie_len;
	do
	{
		if(item[name_len]=='=' && memcmp(name, item, name_len)==0)
		{
			char* p = item + name_len + 1;
			*p_value = p;
			while (p < end)
			{
				if (*p == ';')
				{
					break;
				}
				++p;
			}
			*p_value_len = p - *p_value;
			return 0;
		}
		item = http_cookie_move_to_next_item(item, cookie, cookie_len);
	}while(item);

	return ENOENT;
}

static int32_t http_get_body_item_multipart(char* body, uint32_t body_len,
											char* boundary, uint32_t boundary_len,
											char* name, uint32_t name_len,
											char** p_value, uint32_t* p_value_len)
{
	char* p_sub_datagram = body;
	uint32_t start_boundary_len = 2;
	char start_boundary[128] = "--";
	uint32_t end_boundary_len;
	char end_boundary[128];

	memcpy(start_boundary+start_boundary_len, boundary, boundary_len);
	start_boundary_len += boundary_len;

	memcpy(end_boundary, start_boundary, start_boundary_len);
	memcpy(end_boundary+start_boundary_len, "--", 2);
	end_boundary_len = start_boundary_len+2;

	do
	{
		if(p_sub_datagram[start_boundary_len] == '\r'
			&& memcmp(p_sub_datagram, start_boundary, start_boundary_len) == 0)
		{
			char* p_name;
			p_sub_datagram = http_move_to_next_line(p_sub_datagram, body, body_len);
			if(p_sub_datagram == NULL) return EPROTO;
			p_name = p_sub_datagram;
			while(*p_name!='\r')
			{
				if(memcmp("name=\"", p_name, strlen("name=\"")) == 0)
				{
					char* p = p_name + strlen("name=\"");
					if(p[name_len]=='"' && memcmp(p, name, name_len)==0)
					{
						do
						{
							p_sub_datagram = http_move_to_next_line(p_sub_datagram, body, body_len);
							if (p_sub_datagram == NULL) return EPROTO;
						}while(*p_sub_datagram != '\r'); //发现空行
						p_sub_datagram = http_move_to_next_line(p_sub_datagram, body, body_len);
						if (p_sub_datagram == NULL) return EPROTO;
						*p_value = p_sub_datagram;

						do
						{
							p_sub_datagram = http_move_to_next_line(p_sub_datagram, body, body_len);
							if (p_sub_datagram == NULL) return EPROTO;
						}while(memcmp(p_sub_datagram, end_boundary, end_boundary_len) != 0);

						*p_value_len = p_sub_datagram - *p_value - strlen("\r\n");
						return 0;
					}
					else goto LABEL_NEXT;
				}
				++p_name;
			}
			return EPROTO;
		}
LABEL_NEXT:
		p_sub_datagram = http_move_to_next_line(p_sub_datagram, body, body_len);
	}while(p_sub_datagram);

	return ENOENT;
}

static char* http_move_to_next_item(char* cur_item, char* query_string, uint32_t query_string_len)
{
	char* p = cur_item + 2; //按照规范，query_string项至少大于2字节
	char* end = query_string + query_string_len;
	for( ; p<end; ++p)
	{
		if(*p == '&')
		{
			do
			{
				++p;
				skip_space(p);
			}while(*p == '&');
			return p;
		}
	}
	return NULL;
}

static int32_t http_get_body_item_urlencoded(char* body, uint32_t body_len,
											 char* name, uint32_t name_len,
											 char** p_value, uint32_t* p_value_len)
{
	char* item = body;
	char* end = body + body_len;
	do
	{
		char* p = item + name_len;
		if (p >= end) break;
		if(item[name_len]=='=' && memcmp(name, item, name_len)==0)
		{
			*p_value = ++p;
			while(p<end && *p!='&') ++p;
			*p_value_len = p - *p_value;
			return 0;
		}
		item = http_move_to_next_item(item, body, body_len);
	}while(item);

	return ENOENT;
}

int32_t http_get_body_item(char* header, uint32_t header_len,
						   char* body, uint32_t body_len,
						   char* name, uint32_t name_len,
						   char** p_value, uint32_t* p_value_len)
{
	char* content_type;
	uint32_t content_type_len;
	int32_t ret;
	ret = http_get_request_header(header, header_len, "Content-Type", strlen("Content-Type"), &content_type, &content_type_len);
	if (ret != 0 || content_type_len == 0) return EPROTO;
	if(memcmp("multipart/form-data", content_type, strlen("multipart/form-data")) == 0)
	{
		char* p_boundary = content_type + strlen("multipart/form-data") + 1;
		skip_space(p_boundary);
		if(memcmp("boundary=", p_boundary, strlen("boundary=")) == 0)
		{
			p_boundary += strlen("boundary=");
			return http_get_body_item_multipart(body, body_len, p_boundary, content_type_len - (p_boundary-content_type), name, name_len, p_value, p_value_len);
		}
		else return EPROTO;
	}
	else if(memcmp("application/x-www-form-urlencoded", content_type, strlen("application/x-www-form-urlencoded")) == 0)
	{
		return http_get_body_item_urlencoded(body, body_len, name, name_len, p_value, p_value_len);
	}
	else return EPROTO;
}

#if 0
int main()
{
	HttpQueryStringParser parser;
	char url[1024] = "http://yuancheng.xunlei.com/test?p1=v1%202+2&xxx&&p2=123+321&p3=liu";
	uint32_t ret = parser.parse_url_inplace(url, strlen(url));
	printf("ret:%d\n", ret);

	printf("p2:%d\n", parser.get_int32_param("p2"));

	printf("p3:%s\n", parser.get_cstr_param("p3"));

	printf("p1:%s\n", parser.get_cstr_param("p1"));
	printf("p1:%s\n", parser.get_decoded_string_param("p1").c_str());
	printf("p1:%s\n", parser.get_cstr_param("p1"));
	printf("p1:%s\n", parser.get_string_param("p1").c_str());

	return 0;
}
#endif
