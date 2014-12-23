#ifndef _ASYNCOMMON_HPP_
#define _ASYNCOMMON_HPP_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cstdlib>
#include <utility>

#ifdef _WIN32
#pragma warning(disable:4351) //for visual studio
#pragma warning(disable:4819) //for visual studio
#endif

namespace asyncpp
{

const int32_t MAX_IPV4 = 16;
const int32_t MAX_IPV6 = 40;
const int32_t MAX_IP = MAX_IPV6;

extern volatile time_t g_unix_timestamp;

#ifdef _WIN32
#ifndef localtime_r
#define localtime_r(unix_time, tms) localtime_s(tms, unix_time)
#endif
#ifndef snprintf
#define snprintf(buffer, buffer_size, fmt, ...) _snprintf(buffer, buffer_size, fmt, ##__VA_ARGS__)
#endif
#endif

#ifndef __FUNCSIG__
#ifdef  __PRETTY_FUNCTION__
#define __FUNCSIG__ __PRETTY_FUNCTION__
#elif defined __FUNCTION__
#define __FUNCSIG__ __FUNCTION__
#else
#define __FUNCSIG__ __func__
#endif
#endif

const int LOGGER_LINE_SIZE = 4096;
enum e_logger_level_t
{
	LOGGER_ALL = 0,
	LOGGER_TRACE = 2,
	LOGGER_DEBUG = 3,
	LOGGER_INFO = 4,
	LOGGER_WARN = 5,
	LOGGER_ERROR = 6,
	LOGGER_FATAL = 7,
	LOGGER_NULL = 8,
};

class Logger
{
public:
	int m_fd;
	int32_t m_level;

public:
	int32_t log(const char* buf, uint32_t l);
};

extern Logger* logger;

extern char logger_time_string_buffer[24];
void logger_update_time_string();

#define _logger_wrapper(logger, logger_level, file, line, func, fmt, ...) \
if (LOGGER_##logger_level >= logger->m_level)\
{\
	char _lg_buf[LOGGER_LINE_SIZE]; \
	int32_t _lg_l; \
	logger_update_time_string(); \
	_lg_l = snprintf(_lg_buf, LOGGER_LINE_SIZE, "[" #logger_level "] %s [%s:%u: %s] " fmt "\n", logger_time_string_buffer, file, line, func, ##__VA_ARGS__); \
	if(_lg_l>0) logger->log(_lg_buf,_lg_l<LOGGER_LINE_SIZE?_lg_l:LOGGER_LINE_SIZE-1); \
}\

#define logger_trace(logger, fmt, ...) _logger_wrapper(logger, TRACE, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_debug(logger, fmt, ...) _logger_wrapper(logger, DEBUG, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_info(logger, fmt, ...) _logger_wrapper(logger, INFO, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_warn(logger, fmt, ...) _logger_wrapper(logger, WARN, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_error(logger, fmt, ...) _logger_wrapper(logger, ERROR, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)
#define logger_fatal(logger, fmt, ...) _logger_wrapper(logger, FATAL, __FILE__, __LINE__, __FUNCSIG__, fmt, ##__VA_ARGS__)

typedef uint16_t thread_id_t;
typedef uint16_t thread_pool_id_t;

const thread_id_t INVALID_THREAD_ID = static_cast<thread_id_t>(-1);
const thread_pool_id_t INVALID_THREAD_POOL_ID = static_cast<thread_pool_id_t>(-1);

extern thread_pool_id_t dns_thread_pool_id;
extern thread_id_t dns_thread_id;

enum ERROR_CODE : int32_t
{
	SUCCESS,
	ERR_NOT_FOUND = 20001,
	ERR_TOO_MANY,
	ERR_PROTOCOL,
	ERR_NO_MEMORY,
	ERR_NOT_ENOUGH_BUFFER,

	ERR_UNKNOW = 21000,
};

enum PACKAGE_SIZE_LIMIT : int32_t
{
	MIN_PACKAGE_SIZE = 4,
	MAX_PACKAGE_SIZE = 128 * 1024 * 1024, //128MB
};


/************** Thread Message ****************/
enum class MsgContextType : uint8_t
{
	STATIC, //销毁时无需执行任何操作
	POD_MALLOC, //销毁时使用free
	POD_NEW, //销毁时使用delete[]（不会调用析构函数）
	OBJECT, //销毁时使用delete（会调用析构函数）
};

enum class MsgBufferType : uint8_t
{
	STATIC, //销毁时无需执行任何操作
	MALLOC, //销毁时使用free
	NEW, //销毁时使用delete[]
};

class MsgContext
{
public:
	virtual ~MsgContext(){}
};

union MsgCtx
{
	uint64_t i64;
	void* pod;
	MsgContext* obj;
};

inline void free_context(MsgCtx& ctx, MsgContextType ctx_type)
{
	switch (ctx_type)
	{
	case MsgContextType::STATIC: break;
	case MsgContextType::POD_MALLOC: free(ctx.pod); ctx.pod = nullptr; break;
	case MsgContextType::POD_NEW: delete[] static_cast<char*>(ctx.pod); ctx.pod = nullptr;  break;
	case MsgContextType::OBJECT: delete ctx.obj; ctx.obj = nullptr; break;
	default: break;
	}
}

inline void free_buffer(char*& buf, MsgBufferType buf_type)
{
	switch (buf_type)
	{
	case MsgBufferType::STATIC: break;
	case MsgBufferType::MALLOC: free(buf); buf = nullptr;  break;
	case MsgBufferType::NEW: delete[] buf; buf = nullptr; break;
	default: break;
	}
}

struct ThreadMsg
{
	MsgCtx m_ctx;
	uint32_t m_type;
	uint32_t m_buf_len;
	char* m_buf;
	MsgBufferType m_buf_type;
	MsgContextType m_ctx_type;
	thread_id_t m_src_thread_id;
	thread_id_t m_dst_thread_id;
	thread_pool_id_t m_src_thread_pool_id;
	thread_pool_id_t m_dst_thread_pool_id;

	ThreadMsg()
		: m_ctx({0})
		, m_type(0)
		, m_buf_len(0)
		, m_buf(nullptr)
		, m_buf_type(MsgBufferType::STATIC)
		, m_ctx_type(MsgContextType::STATIC)
		, m_src_thread_id(INVALID_THREAD_ID)
		, m_dst_thread_id(INVALID_THREAD_POOL_ID)
		, m_src_thread_pool_id(INVALID_THREAD_ID)
		, m_dst_thread_pool_id(INVALID_THREAD_POOL_ID)
	{
	}

	ThreadMsg(const MsgCtx& ctx, int32_t type, uint32_t buf_len, char* buf,
		MsgBufferType buf_type, MsgContextType ctx_type,
		thread_id_t src_thread_id, thread_id_t dst_thread_id,
		thread_pool_id_t src_thread_pool_id, thread_pool_id_t dst_thread_pool_id)
		: m_ctx(ctx)
		, m_type(type)
		, m_buf_len(buf_len)
		, m_buf(buf)
		, m_buf_type(buf_type)
		, m_ctx_type(ctx_type)
		, m_src_thread_id(src_thread_id)
		, m_dst_thread_id(dst_thread_id)
		, m_src_thread_pool_id(src_thread_pool_id)
		, m_dst_thread_pool_id(dst_thread_pool_id)
	{
	}

	ThreadMsg(const ThreadMsg& val) = delete;
	ThreadMsg& operator=(const ThreadMsg& val) = delete;

	ThreadMsg(ThreadMsg&& val)
	{
		*this = std::move(val);
	}
	ThreadMsg& operator=(ThreadMsg&& val)
	{
		if (&val != this)
		{
			free_buffer(m_buf, m_buf_type);
			free_context(m_ctx, m_ctx_type);
			m_ctx.i64 = val.m_ctx.i64;
			m_type = val.m_type;
			m_buf_len = val.m_buf_len;
			m_buf = val.m_buf;
			m_buf_type = val.m_buf_type;
			m_ctx_type = val.m_ctx_type;
			m_src_thread_id = val.m_src_thread_id;
			m_dst_thread_id = val.m_dst_thread_id;
			m_src_thread_pool_id = val.m_src_thread_pool_id;
			m_dst_thread_pool_id = val.m_dst_thread_pool_id;

			val.m_ctx.i64 = 0;
			val.m_buf = nullptr;
			val.m_buf_len = 0;
		}
		return *this;
	}

	~ThreadMsg()
	{
		free_buffer(m_buf, m_buf_type);
		free_context(m_ctx, m_ctx_type);
	}
};

/************** Timer Message ****************/
typedef void(*timer_func_t)(uint32_t timer_id, uint64_t ctx);

struct TimerMsg
{
	timer_func_t m_callback;
	time_t m_expire_time;
	uint64_t m_ctx;

	TimerMsg() = default;
	TimerMsg(const TimerMsg& val) = default;
	TimerMsg& operator=(const TimerMsg& val) = default;
	~TimerMsg() = default;

	TimerMsg(timer_func_t callback, time_t expire_time, uint64_t ctx)
		: m_callback(callback), m_expire_time(expire_time), m_ctx(ctx)
	{
	}

	bool operator<(const TimerMsg& val){ return m_expire_time < val.m_expire_time; }
};

/************** Speed Sample ****************/
template<size_t MAX_RANGE = 64> //MAX_RANGE=2^n有助于提高性能(改善模运算速度)
class SpeedSample
{
private:
	time_t m_last_second;
	uint64_t m_total_r_bytes;
	uint64_t m_total_w_bytes;
	uint32_t m_r_sample[MAX_RANGE];
	uint32_t m_w_sample[MAX_RANGE];
	int32_t m_pos;
public:
	SpeedSample()
		: m_last_second(0)
		, m_total_r_bytes(0)
		, m_total_w_bytes(0)
		, m_r_sample()
		, m_w_sample()
		, m_pos(0)
	{}
	~SpeedSample() = default;
	SpeedSample(const SpeedSample&) = default;
	SpeedSample& operator=(const SpeedSample&) = default;

	void sample(uint32_t r_bytes, uint32_t w_bytes)
	{
		if (m_last_second != g_unix_timestamp)
		{
			uint32_t seconds = static_cast<uint32_t>(
				g_unix_timestamp - m_last_second - 1);
			if (seconds > MAX_RANGE) seconds = MAX_RANGE;
			while (seconds != 0)
			{
				m_pos = (m_pos + 1) % MAX_RANGE;
				m_r_sample[m_pos] = 0;
				m_w_sample[m_pos] = 0;
				--seconds;
			}
			m_last_second = g_unix_timestamp;
			m_pos = (m_pos + 1) % MAX_RANGE;
			m_r_sample[m_pos] = r_bytes;
			m_w_sample[m_pos] = w_bytes;
		}
		else
		{
			m_r_sample[m_pos] += r_bytes;
			m_w_sample[m_pos] += w_bytes;
		}

		m_total_r_bytes += r_bytes;
		m_total_w_bytes += w_bytes;
	}
	std::pair<uint32_t, uint32_t> get_avg_speed(uint32_t seconds = 5)
	{
		int32_t pos = m_pos;
		uint64_t r = m_r_sample[pos];
		uint64_t w = m_w_sample[pos];
		
		if(seconds > MAX_RANGE) seconds = MAX_RANGE;
		pos += MAX_RANGE;
		for (uint32_t sec = 1; sec < seconds; ++sec)
		{
			pos = (pos - 1) % MAX_RANGE;
			r += m_r_sample[pos];
			w += m_w_sample[pos];
		}

		return {static_cast<uint32_t>(r/seconds),static_cast<uint32_t>(w/seconds)};
	}

	uint32_t get_cur_r_speed(){ return m_r_sample[m_pos]; }
	uint32_t get_cur_w_speed(){ return m_w_sample[m_pos]; }
	std::pair<uint32_t, uint32_t> get_cur_speed()
	{
		return{ m_r_sample[m_pos], m_w_sample[m_pos] };
	}

	uint32_t get_r_speed(){ return m_r_sample[(m_pos + MAX_RANGE - 1) % MAX_RANGE]; }
	uint32_t get_w_speed(){ return m_w_sample[(m_pos + MAX_RANGE - 1) % MAX_RANGE]; }
	std::pair<uint32_t, uint32_t> get_speed()
	{
		int32_t pos = (m_pos + MAX_RANGE - 1) % MAX_RANGE;
		return{ m_r_sample[pos], m_w_sample[pos] };
	}
};

} //end of namespace asyncpp

#endif
