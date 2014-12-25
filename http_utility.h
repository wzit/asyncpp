/**
# -*- coding:UTF-8 -*-
*/

#ifndef _HTTP_UTILITY_H_
#define _HTTP_UTILITY_H_

#include "string_utility.h"

#include <string>
#include <stdlib.h>
#include <unordered_map>
#include <utility>

class HttpQueryStringParser
{
private:
	typedef std::pair<char*, uint32_t> value_t;
	std::unordered_map<uint32_t, value_t> m_params;
	const char* m_path;
	uint32_t m_path_hash_value;
public:
	HttpQueryStringParser()
		: m_params()
		, m_path(nullptr)
		, m_path_hash_value(0)
	{
	}
	~HttpQueryStringParser() = default;
	HttpQueryStringParser(const HttpQueryStringParser&) = delete;
	HttpQueryStringParser& operator=(const HttpQueryStringParser&) = delete;

private:
	void insert_param(uint32_t k, char* val, uint32_t val_len);

public:
	/**
	 获取URL中的path(http://host:port/path?param_list)
	 hash_value = time33_hash(path)
	 仅在parse_url调用成功后有效
	 */
	uint32_t get_path_hash_value();
	const char* get_path();

	/**
	 获取URL参数，其中哈希值 k = time33_hash(name)
	 @return 如果该参数不存在，返回NULL。返回结果是C风格字符串。
	*仅在parse_url调用成功后有效
	 */
	char* get_param_by_hash_value(uint32_t k);
	char* get_param_by_name(const char* name);
	value_t get_param_len_by_hash_value(uint32_t k);
	value_t get_param_len_by_name(const char* name);

	/**
	 原地解析URL中的参数列表，传入的URL将会被改变，需保证URL缓冲区长度至少为 url_len+1
	 URL的内存将用于随后的get_param调用中，调用者需保证其生命周期足够长
	 @return 0表示成功
	 */
	int32_t parse_url_inplace(char* url, uint32_t url_len);

public:
	/*以下接口当参数不存在时，返回 空字符串、0、false*/
	char* get_cstr_param(const char* name)
	{
		return get_cstr_param(time33_hash(name));
	}
	int32_t get_int32_param(const char* name)
	{
		return get_int32_param(time33_hash(name));
	}
	int64_t get_int64_param(const char* name)
	{
		return get_int64_param(time33_hash(name));
	}
	std::string get_string_param(const char* name)
	{
		return get_string_param(time33_hash(name));
	}
	std::string get_decoded_string_param(const char* name)
	{
		return get_decoded_string_param(time33_hash(name));
	}
	bool get_bool_param(const char* name)
	{
		return get_bool_param(time33_hash(name));
	}

	char* get_cstr_param(uint32_t k)
	{
		return get_param_len_by_hash_value(k).first;
	}
	int32_t get_int32_param(uint32_t k)
	{
		const auto& v = get_param_len_by_hash_value(k);
		if (v.first != nullptr)
		{
			return atoi32(v.first);
		}
		else return 0;
	}
	int64_t get_int64_param(uint32_t k)
	{
		const auto& v = get_param_len_by_hash_value(k);
		if (v.first != nullptr)
		{
			return atoi64(v.first);
		}
		else return 0;
	}
	std::string get_string_param(uint32_t k)
	{
		const auto& v = get_param_len_by_hash_value(k);
		if (v.first != nullptr)
		{
			return std::string(v.first, v.second);
		}
		else return std::string();
	}

	/**
	 获取字符串参数，并对其进行URL解码
	 (对原始数据执行URL解码，然后返回)
	*/
	std::string get_decoded_string_param(uint32_t k)
	{
		const auto& v = get_param_len_by_hash_value(k);
		if (v.first != nullptr)
		{
			return std::string(v.first, decode_url_inplace(v.first, v.second));
		}
		else return std::string();
	}

	/**
	 当参数的第一个字符是't'或'1'时，val=1，否则val=0
	*/
	bool get_bool_param(uint32_t k)
	{
		const auto& v = get_param_len_by_hash_value(k);
		if (v.first != nullptr)
		{
			return v.first[0] == '1' || v.first[0] == 't';
		}
		else return false;
	}
};

/**
 *以下http相关接口均不会申请、释放内存，也不会改变传入内存
 */
int32_t http_package_parse_inplace(char* package, uint32_t package_len,
									char** p_head, uint32_t* p_head_len,
									char** p_body, uint32_t* p_body_len);

int32_t http_get_header_len(char* package, uint32_t package_len);

int32_t http_get_request_header(char* header, uint32_t header_len,
								const char* name, uint32_t name_len,
								char** p_value, uint32_t* p_value_len);

#define http_get_request_cookie(header, header_len, cookie, cookie_len) \
	http_get_request_header(header, header_len, "Cookie", strlen("Cookie"), &(cookie), &(cookie_len))

int32_t http_get_cookie_item(char* cookie, uint32_t cookie_len,
							 const char* name, uint32_t name_len,
							 char** p_value, uint32_t* p_value_len);

int32_t http_get_body_item(char* header, uint32_t header_len,
						   char* body, uint32_t body_len,
						   char* name, uint32_t name_len,
						   char** p_value, uint32_t* p_value_len);


#endif
