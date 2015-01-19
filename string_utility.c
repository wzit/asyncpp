/**
# -*- coding:UTF-8 -*-
*/

#include "string_utility.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" 
{
#endif

const char* const tbl_hexvalue = 
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\xff\xff\xff\xff\xff\xff"
"\xff\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18"
"\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\xff\xff\xff\xff\xff"
"\xff\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18"
"\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

const char* const tbl_int2hex_lower = "0123456789abcdefghijklmnopqrstuvwxyz";
const char* const tbl_int2hex_upper = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

#ifdef __GNUC__

char* strtolower(char* s)
{
	char* p = s;
	while(*p)
	{
		*p = tolower(CHAR_TO_INT(*p));
		++p;
	}
	return s;
}

char* strtoupper(char* s)
{
	char* p = s;
	while(*p)
	{
		*p = toupper(CHAR_TO_INT(*p));
		++p;
	}
	return s;
}
#endif

int64_t atoi64(const char *nptr)
{
	skip_space(nptr);
	if(*nptr=='-')
	{
		++nptr;
		if(!isdigit(CHAR_TO_INT(*nptr))) return 0;
		else return -(int64_t)atou64(nptr);
	}
	else
	{
		return atou64(nptr);
	}
}

uint64_t atou64(const char *nptr)
{
	uint64_t r = 0;
	skip_space(nptr);
	if(*nptr=='+') ++nptr;
	while(isdigit(CHAR_TO_INT(*nptr)))
	{
		r = r*10 + get_hexvalue(*nptr++);
	}
	return r;
}

int32_t atoi32(const char *nptr)
{
	skip_space(nptr);
	if(*nptr=='-')
	{
		++nptr;
		if(!isdigit(CHAR_TO_INT(*nptr))) return 0;
		else return -(int32_t)atou64(nptr);
	}
	else
	{
		return atou32(nptr);
	}
}

uint32_t atou32(const char *nptr)
{
	uint32_t r = 0;
	skip_space(nptr);
	if(*nptr=='+') ++nptr;
	while(isdigit(CHAR_TO_INT(*nptr)))
	{
		r = r*10 + get_hexvalue(*nptr++);
	}
	return r;
}

int64_t strtoi64(const char *nptr, const char **endptr, int32_t base)
{
	skip_space(nptr);
	if(*nptr=='-')
	{
		++nptr;
		if(!isalnum(CHAR_TO_INT(*nptr)))
		{
			if(endptr) *endptr = nptr;
			return 0;
		}
		else
		{
			return -(int64_t)strtou64(nptr, endptr, base);
		}
	}
	else
	{
		return strtou64(nptr, endptr, base);
	}
}

uint64_t strtou64(const char *nptr, const char **endptr, int32_t base)
{
	uint64_t r = 0;
	if(2 <= base && base <= 36)
	{
		skip_space(nptr);
		if(*nptr=='+') ++nptr;
		while(isalnum(CHAR_TO_INT(*nptr)))
		{
			int32_t v = get_hexvalue(*nptr);
			if (v < base)
			{
				nptr++;
				r = r*base + v;
			}
			else break;
		}
	}
	else
	{
		//error
	}
	if(endptr) *endptr = nptr;
	return r;
}

int32_t strtoi32(const char *nptr, const char **endptr, int32_t base)
{
	skip_space(nptr);
	if(*nptr=='-')
	{
		++nptr;
		if(!isalnum(CHAR_TO_INT(*nptr)))
		{
			if(endptr) *endptr = nptr;
			return 0;
		}
		else
		{
			return -(int32_t)strtou32(nptr, endptr, base);
		}
	}
	else
	{
		return strtou32(nptr, endptr, base);
	}
}

uint32_t strtou32(const char *nptr, const char **endptr, int32_t base)
{
	uint32_t r = 0;
	if(2 <= base && base <= 36)
	{
		skip_space(nptr);
		if(*nptr=='+') ++nptr;
		while(isalnum(CHAR_TO_INT(*nptr)))
		{
			int32_t v = get_hexvalue(*nptr);
			if (v < base)
			{
				nptr++;
				r = r*base + v;
			}
			else break;
		}
	}
	else
	{
		//error
	}
	if(endptr) *endptr = nptr;
	return r;
}

int32_t is_string_integer(const char* str)
{
	skip_space(str);
	return isdigit(CHAR_TO_INT(*str)) ||
		( (*str=='+' || *str=='-') && isdigit(CHAR_TO_INT(str[1])));
}

int32_t is_string_unsigned_integer(const char* str)
{
	skip_space(str);
	return isdigit(CHAR_TO_INT(*str)) ||
		(*str=='+' && isdigit(CHAR_TO_INT(str[1])));
}

uint32_t strchrcount(const char* str, uint32_t str_len, char chr)
{
	uint32_t c = 0;
	uint32_t i = 0;
	for (; i < str_len; ++i)
	{
		if (str[i] == chr)
			++c;
	}
	return c;
}

uint32_t i64toa(int64_t n, char* result)
{
	if(n>=0)
	{
		return u64toa(n, result);
	}
	else
	{
		*result = '-';
		return u64toa(-n, result+1) + 1;
	}
}

uint32_t u64toa(uint64_t n, char* result)
{
	char buf[32];
	int32_t i=31;
	int32_t len;
	do{
		buf[--i] = n%10 + '0';
		n /= 10;
	}while(n!=0);
	len = 31 - i;
	memcpy(result, buf+i, len);
	result[len] = 0;
	return len;
}

uint32_t i32toa(int32_t n, char* result)
{
	if(n>=0)
	{
		return u32toa(n, result);
	}
	else
	{
		*result = '-';
		return u32toa(-n, result+1) + 1;
	}
}

uint32_t u32toa(uint32_t n, char* result)
{
	char buf[16];
	int32_t i=15;
	int32_t len;
	do{
		buf[--i] = n%10 + '0';
		n /= 10;
	}while(n!=0);
	len = 15 - i;
	memcpy(result, buf+i, len);
	result[len] = 0;
	return len;
}

uint32_t i64tostr(int64_t n, char* result, int32_t base)
{
	if(2<=base && base<=36)
	{
		if(n>=0)
		{
			return u64tostr(n, result, base);
		}
		else
		{
			*result = '-';
			return u64tostr(-n, result+1, base) + 1;
		}
	}
	else
	{
		*result = 0;
		return 0;
	}
}

uint32_t u64tostr(uint64_t n, char* result, int32_t base)
{
	if(2<=base && base<=36)
	{
		char buf[66];
		int32_t i=65;
		int32_t len;
		do{
			buf[--i] = int2hex(n%base);
			n /= base;
		}while(n!=0);
		len = 65 - i;
		memcpy(result, buf+i, len);
		result[len] = 0;
		return len;
	}
	else
	{
		*result = 0;
		return 0;
	}
}

uint32_t i32tostr(int32_t n, char* result, int32_t base)
{
	if(n>=0)
	{
		return u32tostr(n, result, base);
	}
	else
	{
		*result = '-';
		return u32tostr(-n, result+1, base) + 1;
	}
}

uint32_t u32tostr(uint32_t n, char* result, int32_t base)
{
	if(2<=base && base<=36)
	{
		char buf[34];
		int32_t i=33;
		int32_t len;
		do{
			buf[--i] = int2hex(n%base);
			n /= base;
		}while(n!=0);
		len = 33 - i;
		memcpy(result, buf+i, len);
		result[len] = 0;
		return len;
	}
	else
	{
		*result = 0;
		return 0;
	}
}

uint32_t string_line_number(const char* str, uint32_t str_len)
{
	if(str_len != 0)
	{
		char line_break = '\n';
		uint32_t line_number = 1;
		const char* pend = str + str_len;
		for( ; str<pend; ++str)
		{
			if(*str=='\r' || *str=='\n')
			{
				line_break = *str++;
				break;
			}
		}
		if(str == pend) return line_number;

		for( ; str<pend; ++str)
		{
			if(*str == line_break)
			{
				++line_number;
			}
		}
		if(str[-1] != '\r' && str[-1] != '\n')
		{
			return line_number +1;
		}
		else return line_number;
	}
	else return 0;
}

uint32_t strltrim(char* s, uint32_t n)
{
	int32_t len;
	char *p, *pend;
	if(n==0 || !isspace(CHAR_TO_INT(*s))) return n;
	for(p = s+1, pend = s+n, len = 1; p<pend; ++p, ++len)
	{
		if(!isspace(CHAR_TO_INT(*p))) break;
	}
	len = n - len;
	memmove(s, p, len);
	s[len] = 0;
	return len;
}

uint32_t strrtrim(char* s, uint32_t n)
{
	char* p;
	if(n==0) return 0;
	p = s+n-1;
	while(isspace(CHAR_TO_INT(*p))) --p;
	*++p = 0;
	return p-s;
}

uint32_t strtrim(char* s, uint32_t n)
{
	n = strrtrim(s, n);
	return strltrim(s, n);
}

uint32_t string2hex(const char* in, uint32_t in_len, char* out)
{
	const uint8_t* inp = (const uint8_t*)in;
	const uint8_t* inp_end = inp + in_len;
	for(; inp < inp_end ; inp++)
	{
		*out++ = int2hex(*inp >> 4);
		*out++ = int2hex(*inp & 0x0F);
	}
	*out = 0;

	return in_len*2;
}

uint32_t hex2string(const char* in, uint32_t len, char* out)
{
	char* p = out;
	const char* end = in+len;
	while(in<end){
		char hi = get_hexvalue(*in++);
		char lo = get_hexvalue(*in++);
		*p++ = (hi<<4) | lo;
	}
	//*p = 0;
	return p-out;
}

uint32_t decode_url(const char* in, uint32_t in_len, char* out)
{
	const char* p = in;
	const char* pend = in + in_len;
	int32_t len = 0;
	for( ; p<pend; ++p,++len)
	{
		if(*p == '%')
		{
			if(isxdigit(CHAR_TO_INT(p[1])) &&
				isxdigit(CHAR_TO_INT(p[2])))
			{
				char hi = get_hexvalue(*++p);
				char lo = get_hexvalue(*++p);
				out[len] = (hi<<4) | lo;
			}
			else out[len] = *p;
		}
		else
		{
			out[len] = *p!='+' ? *p : ' ';
		}
	}
	out[len] = 0;
	return len;
}

uint32_t escape_uri(const char* in, uint32_t in_len, char* out)
{
	const char* p = in;
	const char* pend = in + in_len;
	char* po = out;

	for (; p < pend; ++p)
	{
		if (!(*p & 0x80)
			&& (isalnum(CHAR_TO_INT(*p))
			|| *p == '*'
			|| *p == '@'
			|| *p == '-'
			|| *p == '_'
			|| *p == '+'
			|| *p == '.'
			|| *p == '/'))
		{
			*po++ = *p;
		}
		else
		{
			*po++ = '%';
			*po++ = int2hex((int32_t)((uint8_t)(*p) >> 4));
			*po++ = int2hex((int32_t)(*p & 0xf));
		}
	}
	*po = 0;
	return po - out;
}

uint32_t encode_uri(const char* in, uint32_t in_len, char* out)
{
	const char* p = in;
	const char* pend = in + in_len;
	char* po = out;

	for (; p < pend; ++p)
	{
		if (!(*p & 0x80)
			&& (isalnum(CHAR_TO_INT(*p))
			|| *p == '-'
			|| *p == '_'
			|| *p == '.'
			|| *p == '!'
			|| *p == '~'
			|| *p == '*'
			|| *p == '\''
			|| *p == '('
			|| *p == ')'
			|| *p == ';'
			|| *p == '/'
			|| *p == '?'
			|| *p == ':'
			|| *p == '@'
			|| *p == '&'
			|| *p == '='
			|| *p == '+'
			|| *p == '$'
			|| *p == ','
			|| *p == '#'))
		{
			*po++ = *p;
		}
		else
		{
			*po++ = '%';
			*po++ = int2hex((int32_t)((uint8_t)(*p) >> 4));
			*po++ = int2hex((int32_t)(*p & 0xf));
		}
	}
	*po = 0;
	return po - out;
}

uint32_t encode_uri_component(const char* in, uint32_t in_len, char* out)
{
	const char* p = in;
	const char* pend = in + in_len;
	char* po = out;

	for (; p < pend; ++p)
	{
		if (!(*p & 0x80)
			&& (isalnum(CHAR_TO_INT(*p))
			|| *p == '-'
			|| *p == '_'
			|| *p == '.'
			|| *p == '!'
			|| *p == '~'
			|| *p == '*'
			|| *p == '\''
			|| *p == '('
			|| *p == ')'))
		{
			*po++ = *p;
		}
		else
		{
			*po++ = '%';
			*po++ = int2hex((int32_t)((uint8_t)(*p) >> 4));
			*po++ = int2hex((int32_t)(*p & 0xf));
		}
	}
	*po = 0;
	return po - out;
}

uint32_t time33_hash(const char* s)
{
	uint32_t hash = 0;
	for(; *s; ++s)
		hash = (hash<<5) + hash + *s;
	return hash;
}

uint32_t time31_hash(const char* s)
{
	uint32_t hash = 0;
	for(; *s; ++s)
		hash = (hash<<5) - hash + *s;
	return hash;
}

uint32_t time33_hash_bin(const void* data, uint32_t length)
{
	uint32_t hash = 0;
	const uint8_t* p = (const uint8_t*)data;
	const uint8_t* pend = p + length;
	for(; p < pend; ++p)
		hash = (hash<<5) + hash + *p;
	return hash;
}

uint32_t time31_hash_bin(const void* data, uint32_t length)
{
	uint32_t hash = 0;
	const uint8_t* p = (const uint8_t*)data;
	const uint8_t* pend = p + length;
	for(; p < pend; ++p)
		hash = (hash<<5) - hash + *p;
	return hash;
}

uint32_t one_at_a_time_hash_bin(const void* data, uint32_t length)
{
    uint32_t hash = 0;
	const uint8_t* p = (const uint8_t*)data;
	const uint8_t* pend = p + length;
    for(; p < pend; ++p)
    {
        hash += *p;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

uint32_t bob_hash_bin(const void* data, uint32_t length)
{
#define mix(a,b,c) \
a -= b; a -= c; a ^= (c >> 13);\
b -= c; b -= a; b ^= (a << 8);\
c -= a; c -= b; c ^= (b >> 13);\
a -= b; a -= c; a ^= (c >> 12);\
b -= c; b -= a; b ^= (a << 16);\
c -= a; c -= b; c ^= (b >> 5);\
a -= b; a -= c; a ^= (c >> 3);\
b -= c; b -= a; b ^= (a << 10);\
c -= a; c -= b; c ^= (b >> 15); 

	const uint8_t* k = (const uint8_t *)data;
	uint32_t a, b, c, len;

	/* Set up the internal state */
	len = length;
	a = b = c = 0x9e3779b9;  /* the golden ratio; an arbitrary value */

	/* Handle most of the key */
	while (len >= 12)
	{
		a += (k[0] +((uint32_t)k[1] << 8) +((uint32_t)k[2] << 16) +((uint32_t)k[3] << 24));
		b += (k[4] +((uint32_t)k[5] << 8) +((uint32_t)k[6] << 16) +((uint32_t)k[7] << 24));
		c += (k[8] +((uint32_t)k[9] << 8) +((uint32_t)k[10]<< 16)+((uint32_t)k[11] << 24));
		mix(a,b,c);
		k += 12; len -= 12;
	}

	/* Handle the last 11 bytes */
	c += length;
	switch(len)/* all the case statements fall through */
	{
	case 11: c+=((uint32_t)k[10] << 24);
	case 10: c+=((uint32_t)k[9]  << 16);
	case 9 : c+=((uint32_t)k[8]  << 8);
	/* the first byte of c is reserved for the length */
	case 8 : b+=((uint32_t)k[7] << 24);
	case 7 : b+=((uint32_t)k[6] << 16);
	case 6 : b+=((uint32_t)k[5] << 8);
	case 5 : b+=k[4];
	case 4 : a+=((uint32_t)k[3] << 24);
	case 3 : a+=((uint32_t)k[2] << 16);
	case 2 : a+=((uint32_t)k[1] << 8);
	case 1 : a+=k[0];
	}
	mix(a,b,c);

#undef mix

	return c;
}

/*
*   a simple 32 bit checksum that can be upadted from either end
*   (inspired by Mark Adler's Adler-32 checksum)
*/
#define CHAR_OFFSET 0
uint32_t adler32_checksum(char *buf, int len)
{
	int i;
	uint32_t s1, s2;

	s1 = s2 = 0;
	for (i = 0; i < (len - 4); i += 4) {
		s2 += 4 * (s1 + buf[i]) + 3 * buf[i + 1] + 2 * buf[i + 2] + buf[i + 3] +
			10 * CHAR_OFFSET;
		s1 += (buf[i + 0] + buf[i + 1] + buf[i + 2] + buf[i + 3] + 4 * CHAR_OFFSET);
	}
	for (; i < len; i++) {
		s1 += (buf[i] + CHAR_OFFSET);
		s2 += s1;
	}

	return (s1 & 0xffff) + (s2 << 16);
}
#undef CHAR_OFFSET

/*
* adler32_checksum(X0, ..., Xn), X0, Xn+1 ----> adler32_checksum(X1, ..., Xn+1)
* where csum is adler32_checksum(X0, ..., Xn), c1 is X0, c2 is Xn+1
*/
uint32_t adler32_rolling_checksum(uint32_t csum, int len, char c1, char c2)
{
	uint32_t s1, s2;

	s1 = csum & 0xffff;
	s2 = csum >> 16;
	s1 -= (c1 - c2);
	s2 -= (len * c1 - s1);

	return (s1 & 0xffff) + (s2 << 16);
}

uint64_t time31_bob_mixed_hash_bin(const void* data, uint32_t length)
{
	uint64_t time31_hash_val = time31_hash_bin(data, length);
	uint64_t bob_hash_value = bob_hash_bin(data, length);
	return (bob_hash_value<<32) | time31_hash_val;
}

int32_t is_file_name_valid(const char* name)
{
	while (*name)
	{
		switch (*name)
		{
		case '\\': return 0; break;
		case '/': return 0; break;
		case ':': return 0; break;
		case '*': return 0; break;
		case '?': return 0; break;
		case '"': return 0; break;
		case '<': return 0; break;
		case '>': return 0; break;
		case '|': return 0; break;
		default: break;
		}
		++name;
	}
	return 1;
}

int32_t is_file_path_valid(const char* path)
{
	while (*path)
	{
		switch (*path)
		{
		//case ':': return 0; break;
		case '*': return 0; break;
		case '?': return 0; break;
		case '"': return 0; break;
		case '<': return 0; break;
		case '>': return 0; break;
		case '|': return 0; break;
		default: break;
		}
		++path;
	}
	return 1;
}

#ifdef __cplusplus
}
#endif

