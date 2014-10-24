#ifndef _ASYNCPP_HPP_
#define _ASYNCPP_HPP_

#include "pqueue.hpp"
#include "syncqueue.hpp"
#include <thread>

namespace asyncpp
{

typedef int16_t thread_id_t;
typedef int16_t thread_pool_id_t;

enum MsgContextType : uint8_t
{
	MSG_CONTEXT_TYPE_I64, //销毁时无需执行任何操作
	MSG_CONTEXT_TYPE_POD_STATIC, //销毁时无需执行任何操作
	MSG_CONTEXT_TYPE_POD_MALLOC, //销毁时使用free
	MSG_CONTEXT_TYPE_POD_NEW, //销毁时使用delete[]（不会调用析构函数）
	MSG_CONTEXT_TYPE_OBJECT, //销毁时使用delete（会调用析构函数）
};

enum MsgBufferType : uint8_t
{
	MSG_BUFFER_TYPE_STATIC, //销毁时无需执行任何操作
	MSG_BUFFER_TYPE_MALLOC, //销毁时使用free
	MSG_BUFFER_TYPE_NEW, //销毁时使用delete[]
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
	case MsgContextType::MSG_CONTEXT_TYPE_I64: break;
	case MsgContextType::MSG_CONTEXT_TYPE_POD_STATIC: break;
	case MsgContextType::MSG_CONTEXT_TYPE_POD_MALLOC: free(ctx.pod); ctx.pod = nullptr; break;
	case MsgContextType::MSG_CONTEXT_TYPE_POD_NEW: delete[] static_cast<char*>(ctx.pod); ctx.pod = nullptr;  break;
	case MsgContextType::MSG_CONTEXT_TYPE_OBJECT: delete ctx.obj; ctx.obj = nullptr; break;
	default: break;
	}
}

class MsgContext
{
public:
	virtual ~MsgContext() = 0;
};

struct ThreadMsg
{
	MsgCtx m_ctx;
	char* m_buf;
	uint32_t m_buf_len;
	MsgBufferType m_buf_type;
	MsgContextType m_ctx_type;
	thread_id_t m_src_thread_id;
	thread_id_t m_dst_thread_id;
	thread_pool_id_t m_src_thread_pool_id;
	thread_pool_id_t m_dst_thread_pool_id;

	ThreadMsg()
		: m_ctx()
		, m_buf(nullptr)
		, m_buf_len(0)
		, m_buf_type(MsgBufferType::MSG_BUFFER_TYPE_STATIC)
		, m_ctx_type(MsgContextType::MSG_CONTEXT_TYPE_I64)
		, m_src_thread_id(-1)
		, m_dst_thread_id(-1)
		, m_src_thread_pool_id(-1)
		, m_dst_thread_pool_id(-1)
	{
	}

	ThreadMsg(const ThreadMsg& val) = delete;
	ThreadMsg& operator=(const ThreadMsg& val) = delete;

	void free_buffer()
	{
		switch (m_buf_type)
		{
		case MsgBufferType::MSG_BUFFER_TYPE_STATIC: break;
		case MsgBufferType::MSG_BUFFER_TYPE_MALLOC: free(m_buf); m_buf = nullptr;  break;
		case MsgBufferType::MSG_BUFFER_TYPE_NEW: delete[] m_buf; m_buf = nullptr; break;
		default: break;
		}
	}

	ThreadMsg(ThreadMsg&& val)
	{
		*this = std::move(val);
	}
	ThreadMsg& operator=(ThreadMsg&& val)
	{
		if (&val != this)
		{
			free_buffer();
			free_context(m_ctx, m_ctx_type);
			m_ctx.i64 = val.m_ctx.i64;
			m_buf = val.m_buf;
			m_buf_len = val.m_buf_len;
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
		free_buffer();
		free_context(m_ctx, m_ctx_type);
	}
};


typedef void(*timer_func_t)(uint32_t timer_id, MsgCtx& ctx);

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
};

class BaseThread
{
private:
	FixedSizeCircleQueue<ThreadMsg, 256> m_msg_queue;
	pqueue<TimerMsg> m_timer;
public:
	void run()
	{}

	bool push_msg()
	{}
	int32_t get_queued_msg_number()
	{}

	uint32_t add_timer(time_t expire_time, timer_func_t callback, uint64_t ctx)
	{
		return m_timer.push(TimerMsg(callback, expire_time, ctx));
	}
	void del_timer()
	{}
	void change_timer()
	{}
};

class NetThread : public BaseThread
{};

class NetConnect
{
private:
	char ip[40];
	char peer_ip[40];
	uint16_t port;
	uint16_t peer_port;
	queue<int> send_list;
public:
	int32_t frame(const char* msg, int32_t msg_len);
	int32_t on_read(const char* msg, int32_t msg_len);
	int32_t on_write(const char* msg, int32_t msg_len);
	int32_t on_error(const char* msg, int32_t msg_len);
};

class ThreadPool
{
private:
	vector<BaseThread*> threads;
public:
	ThreadPool() = default;
	~ThreadPool()
	{
		for (auto t : threads) delete t;
	}
	ThreadPool(const ThreadPool& val) = delete;
	ThreadPool& operator=(const ThreadPool& val) = delete;
	ThreadPool& operator=(ThreadPool&& val)
	{
		if (&val != this)
		{
			threads = std::move(val.threads);
		}
		return *this;
	}
	ThreadPool(ThreadPool&& val){*this = std::move(val);}
};

class ActiveConnect : public NetConnect
{};

class PassiveConnect : public NetConnect
{};

class NetIOConnect : public NetConnect
{};

class AsyncFrame
{
private:
	std::vector<ThreadPool> thread_pools;
public:
	AsyncFrame() = default;
	~AsyncFrame() = default;
	AsyncFrame(const AsyncFrame&) = delete;
	AsyncFrame& operator=(const AsyncFrame&) = delete;

	//return thread_pool_id
	int16_t add_thread_pool();
	//return thread_id
	void add_thread(int16_t thread_pool_id);
};

} //end of namespace asyncpp

#endif
