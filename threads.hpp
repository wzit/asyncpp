#ifndef _THREADS_HPP_
#define _THREADS_HPP_

#include "asyncommon.hpp"
#include "pqueue.hpp"
#include "syncqueue.hpp"
#include "selector.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <utility>
#include <thread>

namespace asyncpp
{
	
class ThreadPool;
/************** Thread Base ****************/
class BaseThread
{
private:
	FixedSizeCircleQueue<ThreadMsg, 128> m_msg_queue;
	pqueue<TimerMsg> m_timer;
	enum {MSG_CACHE_SIZE = 16};
	ThreadMsg m_msg_cache[MSG_CACHE_SIZE];
	ThreadPool* m_master;
	thread_id_t m_id;
public:
	BaseThread()
		: m_msg_queue()
		, m_timer()
		, m_msg_cache()
		, m_master(nullptr)
		, m_id(0)
	{
	}
	BaseThread(ThreadPool* threadpool)
		: m_msg_queue()
		, m_timer()
		, m_msg_cache()
		, m_master(threadpool)
		, m_id(0)
	{
	}
	virtual ~BaseThread(){}
	BaseThread(const BaseThread&) = delete;
	BaseThread& operator=(const BaseThread&) = delete;
public:
	virtual void run();
	virtual void process_msg(ThreadMsg& msg){}

	void set_id(thread_id_t id){ m_id = id; }
	thread_id_t get_id() const { return m_id; }

	void set_thread_pool(ThreadPool* thread_pool){ m_master = thread_pool; }
	ThreadPool* get_thread_pool() const { return m_master; }

	bool push_msg(ThreadMsg&& msg)
	{
		return m_msg_queue.push(std::move(msg));
	}
	bool full() const { return m_msg_queue.full(); }
	uint32_t get_queued_msg_number()
	{
		return m_msg_queue.size();
	}

	uint32_t add_timer(time_t wait_time, timer_func_t callback, uint64_t ctx)
	{
		return m_timer.push(TimerMsg(callback, time(nullptr) + wait_time, ctx));
	}
	uint32_t add_timer_abs(time_t expire_time, timer_func_t callback, uint64_t ctx)
	{
		return m_timer.push(TimerMsg(callback, expire_time, ctx));
	}
	void del_timer(uint32_t timer_id)
	{
		if (m_timer.is_index_valid(timer_id))
		{
			m_timer.remove(timer_id);
		}
	}
	void change_timer(uint32_t timer_id, time_t expire_time)
	{
		if (m_timer.is_index_valid(timer_id))
		{
			m_timer[timer_id].m_expire_time = expire_time;
			m_timer.change_priority(timer_id);
		}
	}
	uint32_t timer_check()
	{
		uint32_t cnt = 0;
		time_t cur = time(nullptr);
		while (!m_timer.empty())
		{
			const TimerMsg& front = m_timer.front();
			if (front.m_expire_time >= cur)
			{
				timer_func_t callback = front.m_callback;
				uint64_t ctx = front.m_ctx;
				int32_t timer_id = m_timer.front_index();
				m_timer.pop();

				++cnt;
				callback(timer_id, ctx);
			}
			else break;
		}
		return cnt;
	}
};

/************** Net Connection Info ****************/
enum class NetConnectState : uint8_t
{
	NET_CONN_CONNECTED,
	NET_CONN_LISTENING,
	NET_CONN_CONNECTING,
	NET_CONN_CLOSING,
	NET_CONN_ERROR,
};

enum class NetMsgType : uint8_t
{
	CUSTOM_BIN,
	HTTP_GET,
	HTTP_POST,
	HTTP_POST_CHUNKED,
	HTTP_RESP,
	HTTP_RESP_CHUNKED,
};

struct NetConnect
{
	typedef std::tuple<char*, uint32_t, MsgBufferType> SendMsgType;
	std::queue<SendMsgType> m_send_list;
	SOCKET_HANDLE m_fd;
	thread_pool_id_t m_client_thread_pool; //for listen socket only
	thread_id_t m_client_thread; //for listen socket only
	NetConnectState m_state;

	NetConnect()
		: m_send_list()
		, m_fd(INVALID_SOCKET)
		, m_client_thread_pool(-1)
		, m_client_thread(-1)
		, m_state(NetConnectState::NET_CONN_CONNECTING)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		NetConnectState state = NetConnectState::NET_CONN_CONNECTED)
		: m_send_list()
		, m_fd(fd)
		, m_client_thread_pool(-1)
		, m_client_thread(-1)
		, m_state(state)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		thread_pool_id_t client_thread_pool, thread_id_t client_thread)
		: m_send_list()
		, m_fd(fd)
		, m_client_thread_pool(client_thread_pool)
		, m_client_thread(client_thread)
		, m_state(NetConnectState::NET_CONN_LISTENING)
	{
	}
	~NetConnect() = default;
	NetConnect(const NetConnect&) = default;
	NetConnect& operator=(const NetConnect&) = default;
	NetConnect(NetConnect&& val)
	{
		*this = std::move(val);
	}
	NetConnect& operator=(NetConnect&& val)
	{
		if (&val != this)
		{
			m_send_list = std::move(val.m_send_list);
			m_fd = val.m_fd;
			m_state = val.m_state;
		}
		return *this;
	}
	uint32_t id(){ return static_cast<uint32_t>(m_fd); }
	void send(char* msg, uint32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		m_send_list.push(SendMsgType(msg, msg_len, buf_type));
	}
	int32_t get_addr(){}
	int32_t get_peer_addr(){}
};

struct net_conn_hash
{
	size_t operator()(const NetConnect& val){ return static_cast<size_t>(val.m_fd); }
};
struct net_conn_eq
{
	bool operator()(const NetConnect& val1, const NetConnect& val2)
	{ return val1.m_fd == val2.m_fd; }
};

enum ReservedThreadMsgType
{
	MSG_TYPE_UNKNOWN,
	STOP_THREAD,

	NET_CONNECT_HOST_REQ,   //msg.m_buf = host
							//msg.m_ctx.i64 = port
	NET_CONNECT_HOST_RESP,
	
	NET_LISTEN_ADDR_REQ,	//msg.m_buf = ip
							//msg.m_ctx.i64 = port<<32 | client_thread_pool_id<<16 | client_thread_id
	NET_LISTEN_ADDR_RESP,
	
	NET_ACCEPT_CLIENT_REQ,  //msg.m_ctx.i64 = fd
	NET_ACCEPT_CLIENT_RESP,
	
	NET_DNS_QUERY_REQ,      //msg.m_buf = host
	NET_DNS_QUERY_RESP,

	NET_MSG_TYPE_NUMBER
};

/************** Net Recv & Send Thread ****************/
class MultiWaitNetThread : public BaseThread
{
private:
	std::unordered_map<uint32_t, NetConnect> m_conns;
protected:
	SpeedSample<16> m_ss;
	int32_t m_recv_len;
	int32_t m_recv_buf_len;
	char* m_recv_buf;
	int32_t m_header_len;
	int32_t m_body_len;
	NetMsgType m_net_msg_type;
public:
	MultiWaitNetThread()
		: m_conns()
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_recv_buf(nullptr)
		, m_header_len(0)
		, m_body_len(0)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
	{
	}
	MultiWaitNetThread(int32_t init_buf_size)
		: m_conns()
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_recv_buf(nullptr)
		, m_header_len(0)
		, m_body_len(0)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
	{
		m_recv_buf_len = init_buf_size;
		m_recv_buf = static_cast<char*>(malloc(init_buf_size));
		assert(m_recv_buf != nullptr);
	}
public:
	void on_read_event(NetConnect* conn);
	void on_write_event(NetConnect* conn);
	void on_error_event(NetConnect* conn){ on_error(conn, errno); }
protected:
	virtual int32_t frame();
	virtual void on_write(NetConnect* conn);
	void on_read(NetConnect* conn);
	virtual void on_accept(NetConnect* conn);
	virtual void on_close(NetConnect* conn);
	virtual void on_error(NetConnect* conn, int32_t errcode);
public:
	NetConnect* get_conn(uint32_t conn_id){ return &m_conns[conn_id]; }
	virtual void process_msg(NetConnect* conn);
	virtual void close(NetConnect* conn){ conn->m_state = NetConnectState::NET_CONN_CLOSING; }
	virtual void force_close(NetConnect* conn);
	virtual void reset(NetConnect* conn);
	void send(NetConnect* conn, char* msg, int32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		conn->send(msg, msg_len, buf_type);
	}
public:
	void detach_recv_buffer()
	{
		m_recv_buf = nullptr;
		m_recv_len = 0;
	}
public:
	int32_t add_conn(){}
};

/************** Thread Poll ****************/
class AsyncFrame;
class ThreadPool
{
private:
	friend class BaseThread;
	FixedSizeCircleQueue<ThreadMsg, 256> m_msg_queue;
	std::vector<BaseThread*> m_threads;
	AsyncFrame* m_master;
	thread_pool_id_t m_id;
public:
	ThreadPool(AsyncFrame* asynframe, thread_pool_id_t id)
		: m_msg_queue()
		, m_threads()
		, m_master(asynframe)
		, m_id(id)
	{
	}
	~ThreadPool(){ for (auto t : m_threads) delete t; }
	ThreadPool(const ThreadPool& val) = delete;
	ThreadPool& operator=(const ThreadPool& val) = delete;

	void set_id(thread_pool_id_t id){ m_id = id; }
	thread_pool_id_t get_id(){ return m_id; }

	bool push_msg(ThreadMsg&& msg, bool force_receiver_thread = false)
	{
		if (msg.m_dst_thread_id >= 0)
		{
			if (m_threads[msg.m_dst_thread_id]->push_msg(std::move(msg)))
			{
				return true;
			}
			else
			{
				if (!force_receiver_thread)
				{
					return m_msg_queue.push(std::move(msg));
				}
				else return false;
			}
		}
		else
		{
			assert(!force_receiver_thread);
			return m_msg_queue.push(std::move(msg));
		}
	}
	bool full() const { return m_msg_queue.full(); }
	bool full(thread_id_t thread_id) const
	{
		return m_threads[thread_id]->full();
	}
	uint32_t get_queued_msg_number() const
	{
		return m_msg_queue.size();
	}
	uint32_t get_queued_msg_number(thread_id_t thread_id) const
	{
		return m_threads[thread_id]->get_queued_msg_number();
	}

	BaseThread* operator[](thread_id_t thread_id) const
	{
		return m_threads[thread_id];
	}

	thread_id_t add_thread(BaseThread* new_thread)
	{
		new_thread->set_id(m_threads.size());
		new_thread->set_thread_pool(this);
		m_threads.push_back(new_thread);
		return new_thread->get_id();
	}
};

} //end of namespace asyncpp

#endif
