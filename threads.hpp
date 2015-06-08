#ifndef _THREADS_HPP_
#define _THREADS_HPP_

#include "asyncommon.hpp"
#include "pqueue.hpp"
#include "syncqueue.hpp"
#include "selector.hpp"
#include "byteorder.h"
#include <stdio.h>
#include <vector>
#include <queue>
#include <tuple>
#include <utility>
#include <thread>

#ifndef _ASYNCPP_THREAD_QUEUE_SIZE
#define _ASYNCPP_THREAD_QUEUE_SIZE 64
#endif

#ifndef _ASYNCPP_THREAD_MSG_CACHE_SIZE
#define _ASYNCPP_THREAD_MSG_CACHE_SIZE 16
#endif

#ifndef _ASYNCPP_THREAD_POOL_QUEUE_SIZE
#define _ASYNCPP_THREAD_POOL_QUEUE_SIZE 1024
#endif

#ifndef _ASYNCPP_DNS_TIMEOUT
#define _ASYNCPP_DNS_TIMEOUT 3600 //s
#endif

#ifndef _ASYNCPP_CONNECT_TIMEOUT
#define _ASYNCPP_CONNECT_TIMEOUT 3600 //s
#endif

#ifndef _ASYNCPP_IDLE_TIMEOUT
#define _ASYNCPP_IDLE_TIMEOUT 3600 //s
#endif

#ifdef _WIN32
#pragma warning(disable:4100)
#endif

namespace asyncpp
{

enum class ThreadState : uint8_t
{
	INIT,
	WORKING,
	END,
};

class AsyncFrame;
class ThreadPool;
/************** Thread Base ****************/
class BaseThread
{
protected:
	FixedSizeCircleQueue<ThreadMsg, _ASYNCPP_THREAD_QUEUE_SIZE> m_msg_queue;
	pqueue<TimerMsg> m_timer;
	std::unordered_map<uint64_t, MsgContext*> m_ctxs;
	ThreadMsg m_msg_cache[_ASYNCPP_THREAD_MSG_CACHE_SIZE];
	ThreadPool* m_master;
	std::thread* m_thr;
	thread_id_t m_id;
	ThreadState m_state;
public:
	BaseThread()
		: m_msg_queue()
		, m_timer()
		, m_ctxs()
		, m_msg_cache()
		, m_master(nullptr)
		, m_thr(nullptr)
		, m_id(0)
		, m_state(ThreadState::INIT)
	{
	}
	BaseThread(ThreadPool* threadpool)
		: m_msg_queue()
		, m_timer()
		, m_ctxs()
		, m_msg_cache()
		, m_master(threadpool)
		, m_thr(nullptr)
		, m_id(0)
		, m_state(ThreadState::INIT)
	{
	}
	virtual ~BaseThread()
	{
		for (auto& it : m_ctxs)delete it.second;
	}
	BaseThread(const BaseThread&) = delete;
	BaseThread& operator=(const BaseThread&) = delete;
public:
	void set_state(ThreadState state) { m_state = state; }
	ThreadState get_state() { return m_state; }
	void attach_thread(std::thread* thr){m_thr = thr;}
	void waitstop()
	{
		m_thr->join();
		delete m_thr;
		m_thr = nullptr;
	}
	
	uint32_t check_timer_and_thread_msg();

	virtual void on_start(){}

	/*
	 重写这个函数以改变线程行为
	*/
	virtual void run();

	/*
	 重写这个函数以处理线程消息
	*/
	virtual void process_msg(ThreadMsg& msg){}

	void set_id(thread_id_t id){ m_id = id; }
	thread_id_t get_id() const { return m_id; }

	void set_thread_pool(ThreadPool* thread_pool){ m_master = thread_pool; }
	ThreadPool* get_thread_pool() const { return m_master; }
	thread_id_t get_thread_pool_id() const;

	AsyncFrame* get_asynframe() const;

	/*
	 向自己发送一个线程消息
	*/
	bool push_msg(ThreadMsg&& msg)
	{
		return m_msg_queue.push(std::move(msg));
	}
	bool full() const { return m_msg_queue.full(); }
	uint32_t get_queued_msg_number()
	{
		return m_msg_queue.size();
	}

public:
	/*
	 重写这个函数以处理定时器消息
	*/
	virtual void on_timer(uint32_t timerid, uint32_t type, uint64_t ctx){}

	/*
	 添加一个一次性定时器
	 @param wait_time， 多少秒后定时器触发，可以为0(稍后触发)
	 @return timerid: [0, INT32_MAX], always success except memory out
	*/
	int32_t add_timer(uint32_t wait_time, uint32_t type, uint64_t ctx)
	{
		uint32_t timerid = m_timer.push(TimerMsg(g_us_tick + wait_time * 1000000ull, ctx, type));
		_INFOLOG(logger, "timerid:%u, wait_time:%us, g_us_tick:%" PRIu64
			", type:%u, ctx:%" PRIu64, timerid, wait_time, g_us_tick, type, ctx);
		return (int32_t)timerid;
	}

	/*
	 添加一个一次性定时器
	 @param wait_time_us， 多少微妙后定时器触发，可以为0(稍后触发)，目前最高精度为10ms级别
	 @return timerid: [0, INT32_MAX], always success except memory out
	*/
	int32_t add_timer_us(uint32_t wait_time_us, uint32_t type, uint64_t ctx)
	{
		uint32_t timerid = m_timer.push(TimerMsg(g_us_tick + wait_time_us, ctx, type));
		_INFOLOG(logger, "timerid:%u, wait_time:%uus, g_us_tick:%" PRIu64
			", type:%u, ctx:%" PRIu64, timerid, wait_time_us, g_us_tick, type, ctx);
		return (int32_t)timerid;
	}

	/*
	 删除一个定时器
	*/
	void del_timer(uint32_t timerid)
	{
		if (m_timer.is_index_valid(timerid))
		{
			_INFOLOG(logger, "timerid:%u", timerid);
			m_timer.remove(timerid);
		}
		else
		{
			_WARNLOG(logger, "invalid timerid:%u", timerid);
		}
	}

	/*
	 修改定时器时间
	*/
	void change_timer(uint32_t timerid, uint32_t wait_time)
	{
		if (m_timer.is_index_valid(timerid))
		{
			m_timer[timerid].m_expire_time = g_us_tick + wait_time * 1000000ull;
			m_timer.change_priority(timerid);
			_INFOLOG(logger, "timerid:%u, wait_time:%us, g_us_tick:%" PRIu64, timerid, wait_time, g_us_tick);
		}
		else
		{
			_WARNLOG(logger, "invalid timerid:%u", timerid);
		}
	}
	void change_timer_us(uint32_t timerid, uint32_t wait_time_us)
	{
		if (m_timer.is_index_valid(timerid))
		{
			m_timer[timerid].m_expire_time = g_us_tick + wait_time_us;
			m_timer.change_priority(timerid);
			_INFOLOG(logger, "timerid:%u, wait_time:%uus, g_us_tick:%" PRIu64, timerid, wait_time_us, g_us_tick);
		}
		else
		{
			_WARNLOG(logger, "invalid timerid:%u", timerid);
		}
	}

	const TimerMsg* get_timer(int32_t timerid) const
	{
		if (m_timer.is_index_valid(timerid))
		{
			return &m_timer[timerid];
		}
		else
		{
			return nullptr;
		}
	}

	/*
	 检查定时器是否到时间
	 一般情况下请不要调用此接口
	*/
	uint32_t timer_check()
	{
		uint32_t cnt = 0;
		uint64_t cur = g_us_tick;
		while (!m_timer.empty())
		{
			const TimerMsg& front = m_timer.front();
			if (front.m_expire_time <= cur)
			{
				uint32_t type = front.m_type;
				uint64_t ctx = front.m_ctx;
				int32_t timerid = m_timer.front_index();
				m_timer.pop();

				++cnt;
				_DEBUGLOG(logger, "run timerid:%d, type:%u, ctx:%" PRIu64, timerid, type, ctx);
				on_timer(timerid, type, ctx);
			}
			else break;
		}
		return cnt;
	}

public:
	void add_thread_ctx(uint64_t seq, MsgContext* ctx)
	{
		m_ctxs.insert(std::make_pair(seq, ctx));
	}
	void del_thread_ctx(uint64_t seq)
	{
		m_ctxs.erase(seq);
	}
	MsgContext* get_thread_ctx(uint64_t seq)
	{
		const auto& it = m_ctxs.find(seq);
		if (it != m_ctxs.end()) return it->second;
		else return nullptr;
	}
	void del_first_thread_ctx(MsgContext* val, bool(*equ)(MsgContext* val, MsgContext* r))
	{
		for (auto it = m_ctxs.begin(); it != m_ctxs.end(); ++it)
		{
			if (equ(val, it->second))
			{
				m_ctxs.erase(it);
				return;
			}
		}
	}
	std::pair<uint64_t, MsgContext*> get_first_thread_ctx(MsgContext* val, bool(*equ)(MsgContext* val, MsgContext* r))
	{
		for (auto it : m_ctxs)
		{
			if (equ(val, it.second)) return it;
		}
		return {0,nullptr};
	}
};

int32_t dns_query(const char* host, char* ip);

class DnsThread : public BaseThread
{
private:
	typedef std::pair<std::string, std::string> CacheDataT;
	std::unordered_map<uint64_t, CacheDataT> m_cache;
public:
	virtual void process_msg(ThreadMsg& msg) override;
	virtual void on_timer(uint32_t timerid, uint32_t type, uint64_t ctx) override;
};

/************** Net Connection Info ****************/
enum NetTimerType
{
	NetTimeoutTimer = 10000,
	NetBusyTimer,
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

struct SendMsgType
{
	char* data;
	uint32_t data_len;
	uint32_t bytes_sent;
	MsgBufferType buf_type;
};

struct NetConnect
{
	std::queue<SendMsgType> m_send_list;
	uint64_t m_ctx;
	char* m_recv_buf;
	int32_t m_recv_len;
	int32_t m_recv_buf_len;
	int32_t m_header_len;
	int32_t m_body_len;
	SOCKET_HANDLE m_fd;
	int32_t m_timerid;
	//int32_t m_busytimerid;
	//bool m_busy;
	thread_pool_id_t m_client_thread_pool; //for listen socket only
	thread_id_t m_client_thread; //for listen socket only
	NetConnectState m_state;
	NetMsgType m_net_msg_type;
	uint16_t m_send_queue_limit;

public:
	NetConnect()
		: m_send_list()
		, m_ctx(0)
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_fd(INVALID_SOCKET)
		, m_timerid(-1)
		, m_client_thread_pool(INVALID_THREAD_POOL_ID)
		, m_client_thread(INVALID_THREAD_ID)
		, m_state(NetConnectState::NET_CONN_CLOSED)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_send_queue_limit(64)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		NetConnectState state = NetConnectState::NET_CONN_CONNECTED)
		: m_send_list()
		, m_ctx(0)
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_fd(fd)
		, m_timerid(-1)
		, m_client_thread_pool(INVALID_THREAD_POOL_ID)
		, m_client_thread(INVALID_THREAD_ID)
		, m_state(state)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_send_queue_limit(64)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		thread_pool_id_t client_thread_pool, thread_id_t client_thread)
		: m_send_list()
		, m_ctx(0)
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_fd(fd)
		, m_timerid(-1)
		, m_client_thread_pool(client_thread_pool)
		, m_client_thread(client_thread)
		, m_state(NetConnectState::NET_CONN_LISTENING)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_send_queue_limit(64)
	{
	}
	~NetConnect()
	{
		destruct();
		if (m_recv_buf != nullptr) free(m_recv_buf);
	}
	void destruct()
	{
		m_recv_len = 0; //m_recv_buf继续使用
		if (m_fd != INVALID_SOCKET)
		{
			_INFOLOG(logger, "close sockfd:%d, state:%d", (int)m_fd, (int)m_state);
			assert(m_state == NetConnectState::NET_CONN_CLOSED);
#ifdef _ASYNCPP_DEBUG
			assert(m_ctx == 0x010203041234dbfellu);
#endif
			::closesocket(m_fd);
			m_fd = INVALID_SOCKET;
		}
		while (!m_send_list.empty())
		{
			auto& it = m_send_list.front();
			free_buffer(it.data, it.buf_type);
			m_send_list.pop();
		}
	}
	NetConnect(const NetConnect&) = delete;
	NetConnect& operator=(const NetConnect&) = delete;
	NetConnect(NetConnect&& val)
	{
		if (&val != this)
		{
			copy(std::move(val));
		}
	}
	NetConnect& operator=(NetConnect&& val)
	{
		if (&val != this)
		{
			free(m_recv_buf);
			copy(std::move(val));
		}
		return *this;
	}

private:
	void copy(NetConnect&& val)
	{
		m_send_list = std::move(val.m_send_list);
		m_ctx = val.m_ctx;
		m_recv_buf = val.m_recv_buf; val.m_recv_buf = nullptr;
		m_recv_len = val.m_recv_len;
		m_recv_buf_len = val.m_recv_buf_len;
		m_header_len = val.m_header_len;
		m_body_len = val.m_body_len;
		m_fd = val.m_fd; val.m_fd = INVALID_SOCKET; //do NOT close fd
		m_timerid = val.m_timerid; val.m_timerid = -1;
		m_client_thread_pool = val.m_client_thread_pool;
		m_client_thread = val.m_client_thread;
		m_state = val.m_state;
		m_net_msg_type = val.m_net_msg_type;
		m_send_queue_limit = val.m_send_queue_limit;
	}

public:
	void set_ctx(uint64_t ctx){m_ctx=ctx;}
	uint64_t get_ctx()const{return m_ctx;}
	void set_send_queue_limit(uint16_t limit){m_send_queue_limit=limit;}
	uint16_t get_send_queue_limit()const{return m_send_queue_limit;}
	uint32_t send_queue_size(){return static_cast<uint32_t>(m_send_list.size());}
	uint32_t send_queue_empty(){return m_send_list.empty();}
	bool send_queue_full(){return m_send_list.size() >= m_send_queue_limit;}
	void enlarge_recv_buffer(int32_t n)
	{
		if (n > m_recv_buf_len)
		{
			m_recv_buf_len = n;
			m_recv_buf = static_cast<char*>(realloc(m_recv_buf, n + 32));
			assert(m_recv_buf != nullptr);
#ifdef _ASYNCPP_DEBUG
			//memory barrier
			memcpy(m_recv_buf + n + 16, "ASYNCPPMEMORYBAR", 16);
#endif
		}
	}
	void enlarge_recv_buffer()
	{
		enlarge_recv_buffer(m_recv_buf_len * 2 + 512);
	}
	void detach_recv_buffer()
	{
		m_recv_buf = nullptr;
		m_recv_len = 0;
	}
	uint32_t id(){ return static_cast<uint32_t>(m_fd); }
	std::pair<std::string, uint16_t> get_addr() const
	{
		char ip[MAX_IP] = {};
		struct sockaddr_in addr;
		socklen_t len = sizeof addr;
		int ret = getsockname(m_fd,
			reinterpret_cast<struct sockaddr*>(&addr), &len);
		if (ret != 0) return {std::string(), 0};
		asyncpp_inet_ntop(addr.sin_family,
			reinterpret_cast<void*>(&addr.sin_addr), ip, MAX_IP);
		return {std::string(ip), be16toh(addr.sin_port)};
	}
	std::pair<std::string, uint16_t> get_peer_addr() const
	{
		char ip[MAX_IP] = {};
		struct sockaddr_in addr;
		socklen_t len = sizeof addr;
		int ret = getpeername(m_fd,
			reinterpret_cast<struct sockaddr*>(&addr), &len);
		if (ret != 0) return {std::string(), 0};
		asyncpp_inet_ntop(addr.sin_family,
			reinterpret_cast<void*>(&addr.sin_addr), ip, MAX_IP);
		return {std::string(ip), be16toh(addr.sin_port)};
	}
	int32_t setopt(int level, int optname, const char* optval, int optlen)
	{
		return setsockopt(m_fd, level, optname, optval, optlen);
	}

	int32_t set_nodelay(int32_t bEnable = 1)
	{
		return setopt(IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<const char*>(&bEnable), sizeof bEnable);
	}

public: //private
	/*请勿使用*/
	int32_t send(char* msg, uint32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		if (m_send_list.size() < m_send_queue_limit)
		{
			m_send_list.push({ msg, msg_len, 0, buf_type });
			return 0;
		}
		else
		{
			_WARNLOG(logger, "conn %d send list full", (int)m_fd);
			return EAGAIN;
		}
	}
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

class QueryDnsCtx : public MsgContext
{
public:
	int32_t m_ret;
	int32_t m_seq;
	thread_pool_id_t m_src_thread_pool_id;
	thread_id_t m_src_thread_id;
	uint16_t m_port;
	char m_ip[MAX_IP];

	QueryDnsCtx() = default;
	virtual ~QueryDnsCtx() = default;

	QueryDnsCtx(const QueryDnsCtx&) = default;
	QueryDnsCtx& operator=(const QueryDnsCtx&) = default;

};

class AddConnectorCtx : public QueryDnsCtx
{
public:
	uint32_t m_connid;

	AddConnectorCtx() = default;
	~AddConnectorCtx() = default;

	AddConnectorCtx(const AddConnectorCtx&) = default;
	AddConnectorCtx& operator=(const AddConnectorCtx&) = default;
};

class AddListenerCtx : public MsgContext
{
public:
	int32_t m_ret;
	int32_t m_seq;
	uint32_t m_connid;
	thread_pool_id_t m_client_thread_pool_id;
	thread_id_t m_client_thread_id;
	uint16_t m_port;

	AddListenerCtx() = default;
	virtual ~AddListenerCtx() = default;

	AddListenerCtx(const AddListenerCtx&) = default;
	AddListenerCtx& operator=(const AddListenerCtx&) = default;

};

enum ReservedThreadMsgType
{
	MSG_TYPE_UNKNOWN,
	STOP_THREAD,

	NET_CONNECT_HOST_REQ,   //msg.m_buf = host
							//msg.m_ctx.obj = AddConnectorCtx*
	NET_CONNECT_HOST_RESP,	//msg.m_buf = host
							//msg.m_ctx.obj = AddConnectorCtx*
	
	NET_LISTEN_ADDR_REQ,	//msg.m_buf = ip
							//msg.m_ctx.obj = AddListenerCtx*
	NET_LISTEN_ADDR_RESP,	//msg.m_buf = ip
							//msg.m_ctx.obj = AddListenerCtx*
	
	NET_ACCEPT_CLIENT_REQ,  //msg.m_ctx.i64 = fd
	NET_ACCEPT_CLIENT_RESP,	//response ONLY on ERROR, msg.m_ctx.i64 = ret<<32 | fd
	
	NET_QUERY_DNS_REQ,      //msg.m_buf = host
							//msg.m_ctx.obj = QueryDnsCtx*
	NET_QUERY_DNS_RESP,		//msg.m_buf = host
							//msg.m_ctx.obj = QueryDnsCtx*

	NET_CLOSE_CONN_REQ,		//msg.m_ctx.i64 = force_close << 32 | conn_id
	NET_CLOSE_CONN_RESP,	//no response

	NET_MSG_TYPE_NUMBER
};

const uint32_t SPEEDUNLIMITED = 3 * 1024 * 1024 * 1024u; //3GB/s

/************** Net Recv & Send Thread ****************/
class NetBaseThread : public BaseThread
{
public:
	SpeedSample<16> m_ss;
	uint32_t m_dns_timeout; //s
	uint32_t m_connect_timeout; //s
	uint32_t m_idle_timeout; //s
	uint32_t m_sendspeedlimit; //B/s
	uint32_t m_recvspeedlimit; //B/s
public:
	NetBaseThread()
		: m_ss()
		, m_dns_timeout(_ASYNCPP_DNS_TIMEOUT)
		, m_connect_timeout(_ASYNCPP_CONNECT_TIMEOUT)
		, m_idle_timeout(_ASYNCPP_IDLE_TIMEOUT)
		, m_sendspeedlimit(SPEEDUNLIMITED)
		, m_recvspeedlimit(SPEEDUNLIMITED)
	{
	}
	~NetBaseThread() = default;
	NetBaseThread(NetBaseThread&) = delete;
	NetBaseThread& operator=(const NetBaseThread&) = delete;
public:
	void set_speedlimit(uint32_t sendlimit = SPEEDUNLIMITED,
		uint32_t recvlimit = SPEEDUNLIMITED)
	{
		m_sendspeedlimit = sendlimit;
		m_recvspeedlimit = recvlimit;
	}
	//connect在t秒产生错误ETIMEDOUT
	void set_connect_timeout(uint32_t t){m_connect_timeout=t;}
	//连接上t秒收不到数据后产生错误ETIMEDOUT
	void set_idle_timeout(uint32_t t){m_idle_timeout=t;}
public:
	virtual void run() override;
public:
	/*一般情况下，请勿调用这些函数*/
	uint32_t on_read_event(NetConnect* conn);
	uint32_t on_write_event(NetConnect* conn);
	void on_error_event(NetConnect* conn)
	{
		int32_t ret = GET_SOCK_ERR();
		int32_t sockerr = 0;
		socklen_t len = sizeof(sockerr);
		getsockopt(conn->m_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&sockerr), &len);
		
		_WARNLOG(logger, "sockfd:%d, sockerr:%d, errno:%d[%s], state:%d", (int)conn->m_fd, sockerr, ret, strerror(errno), (int)conn->m_state);
		if (conn->m_state == NetConnectState::NET_CONN_CLOSING)
		{
			remove_conn(conn);
		}
		else if(conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			if (sockerr == 0) sockerr = ret;
			assert(sockerr != 0);
			ret = on_error(conn, sockerr);
			if (ret == 0) remove_conn(conn);
		}
	}
protected:
	/*一般情况下，请勿调用这些函数*/
	uint32_t do_accept(NetConnect* conn);
	uint32_t do_connect(NetConnect* conn);
	uint32_t do_send(NetConnect* conn);
	uint32_t do_recv(NetConnect* conn);
	int32_t set_sock_nonblock(SOCKET_HANDLE fd);
	int32_t set_sock_cloexec(SOCKET_HANDLE fd);
	SOCKET_HANDLE create_tcp_socket(bool nonblock = true);

protected:
	/*
	 创建一个监听
	 @return <result, fd>, on success result=0
	*/
	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_listen_socket(const char* ip, uint16_t port,
			thread_pool_id_t client_thread_pool, thread_id_t client_thread,
			bool nonblock = true);

	/*
	 创建一个连接
	 @return <result, fd>, on success result=0
	*/
	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_connect_socket(const char* ip, uint16_t port,
		bool nonblock = true, uint32_t seq = 0);
protected:
	/*
	 线程内部接口，获取一个消息的长度
	 当框架能够满足此消息长度后回调process_net_msg(conn)
	 @return >0  预期的消息长度
	         <=0 出错，连接将被关闭
	*/
	virtual int32_t frame(NetConnect* conn);

	/*
	 accept一个客户端后回调
	 @return 0 pass
	         1 reject and close client conn
			 2 reject but do NOT close client conn
	 */
	virtual int32_t on_accept(SOCKET_HANDLE fd){return 0;}

	/*
	 连接关闭前回调(连接关闭无法被阻止)
	*/
	virtual void on_close(NetConnect* conn){}

	/*
	 连接成功后回调
	*/
	virtual void on_connect(NetConnect* conn){}

	/*
	 连接出错后回调
	 @return 0 close conn
	         1 do NOT close conn
	*/
	virtual int32_t on_error(NetConnect* conn, int32_t errcode){return 0;}

public:
	/*
	 关闭连接
	 连接将延迟到数据发送或出错后关闭，期间不再接收消息
	*/
	virtual void close(NetConnect* conn)
	{
		if (conn->m_state != NetConnectState::NET_CONN_CLOSING
			&& conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			_DEBUGLOG(logger, "sockfd:%d", (int)conn->m_fd);
			set_write_event(conn);
			conn->m_state = NetConnectState::NET_CONN_CLOSING;
		}
	}

	/*
	 强制关闭连接
	*/
	virtual void force_close(NetConnect* conn){remove_conn(conn);}

	/*
	 强制关闭连接，对端将会收到RST
	*/
	virtual void reset(NetConnect* conn)
	{
		struct linger so_linger = { 1, 0 }; //set linger on and wait 0s (i.e. do NOT wait)
		conn->setopt(SOL_SOCKET, SO_LINGER,
			reinterpret_cast<const char*>(&so_linger), sizeof so_linger);
		remove_conn(conn);
	}

	/*
	 获取连接
	*/
	virtual NetConnect* get_conn(uint32_t conn_id) = 0;

	/*
	 发送数据
	 这些数据将会被排队发送，如果队列满，返回EAGAIN
	 @return result
	*/
	int32_t send(NetConnect* conn, char* msg, int32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		if (conn->m_state == NetConnectState::NET_CONN_CONNECTED
			|| conn->m_state == NetConnectState::NET_CONN_CONNECTING)
		{
			_DEBUGLOG(logger, "conn %d send %uB", (int)conn->m_fd, msg_len);
			if (conn->m_send_list.empty())
			{
				set_rdwr_event(conn);
			}
			return conn->send(msg, msg_len, buf_type);
		}
		else
		{
			_WARNLOG(logger, "conn %d error state:%d", (int)conn->m_fd, (int)conn->m_state);
			assert(0);
			return EBUSY;
		}
	}

	/*
	 发送数据
	 这些数据将会被排队发送，如果队列满，返回EAGAIN
	 @return result
	*/
	virtual int32_t send(uint32_t conn_id, char* msg, uint32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		NetConnect* conn = get_conn(conn_id);
		if (conn != nullptr)
		{
			return send(conn, msg, msg_len, buf_type);
		}
		else
		{
			_WARNLOG(logger, "conn %d not found", conn_id);
			return EINVAL;
		}
	}
public:
	/*
	 重写这个函数以处理线程消息
	*/
	virtual void process_net_msg(NetConnect* conn) = 0;

	/*
	 重写这个函数以改变网络事件处理逻辑
	*/
	virtual int32_t poll() = 0;

	/*
	 重写这个函数时，必须在default分支内调用基类on_timer
	*/
	virtual void on_timer(uint32_t timerid, uint32_t type, uint64_t ctx) override
	{
		switch (type)
		{
		case NetTimeoutTimer:
		{
			NetConnect* conn = get_conn((uint32_t)ctx);
			assert(conn != nullptr);
			if (conn != nullptr)
			{
				conn->m_timerid = -1;

				if (conn->m_state != NetConnectState::NET_CONN_CLOSING
					&& conn->m_state != NetConnectState::NET_CONN_CLOSED
					&& on_error(conn, ETIMEDOUT) == 0)
				{
					close(conn);
				}
			}
		}
			break;
		case NetBusyTimer:
			///TODO: process_net_msg返回busy表示业务繁忙，框架会暂停收包，稍后回调
			break;
		default:
			_WARNLOG(logger, "recv error timer type:%d, timerid:%u, ctx:%" PRIu64, type, timerid, ctx);
			break;
		}
	}

public:
	/*一般情况下，请勿调用这些函数*/
	virtual void add_conn(NetConnect* conn) = 0;
	virtual void remove_conn(NetConnect* conn) = 0;
	virtual void set_read_event(NetConnect* conn) = 0;
	virtual void set_write_event(NetConnect* conn) = 0;
	virtual void set_rdwr_event(NetConnect* conn) = 0;
};

int32_t is_str_ipv4(const char* ipv4str);

class NonblockNetThread : public NetBaseThread
{
protected:
	NetConnect m_conn;
public:
	NonblockNetThread() = default;
	~NonblockNetThread()
	{
#ifndef NDEBUG //to avoid assert() in conn->destruct()
#ifdef _ASYNCPP_DEBUG
		m_conn.m_ctx = 0x010203041234dbfellu;
#endif
		m_conn.m_state = NetConnectState::NET_CONN_CLOSED;
#endif
	}
	NonblockNetThread(const NonblockNetThread&) = delete;
	NonblockNetThread& operator=(const NonblockNetThread&) = delete;

public:
	virtual void process_msg(ThreadMsg& msg) override;
	virtual void process_net_msg(NetConnect* conn) override{}
	virtual int32_t poll() override
	{
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			uint32_t bytes_recv = 0;
			uint32_t bytes_sent = 0;

			const auto& s = m_ss.get_cur_speed();
			if (s.first < m_recvspeedlimit)
			{
				bytes_recv = on_read_event(&m_conn);
			}
			if (s.second < m_sendspeedlimit)
			{
				bytes_sent = on_write_event(&m_conn);
			}

			m_ss.sample(bytes_recv, bytes_sent);
			if (m_conn.m_state == NetConnectState::NET_CONN_CLOSED)
			{
#ifdef _ASYNCPP_DEBUG
				m_conn.m_ctx = 0x010203041234dbfellu;
#endif
				m_conn.destruct();
			}

			return bytes_sent + bytes_recv;

		}
		else return 0;
	}

	virtual void add_conn(NetConnect* conn) override
	{
		assert(conn->m_fd != INVALID_SOCKET);
		assert(m_conn.m_fd == INVALID_SOCKET);
		assert(conn->m_fd != m_conn.m_fd);
		if (conn->m_state == NetConnectState::NET_CONN_CONNECTED)
		{
			conn->m_timerid = add_timer(m_idle_timeout, NetTimeoutTimer, conn->id());
		}
		else if (conn->m_state == NetConnectState::NET_CONN_CONNECTING)
		{
			conn->m_timerid = add_timer(m_connect_timeout, NetTimeoutTimer, conn->id());
		}
		_DEBUGLOG(logger, "sockfd:%d, state:%d", (int)conn->m_fd, (int)conn->m_state);
		m_conn = std::move(*conn);
	}
	virtual void set_read_event(NetConnect* conn) override{}
	virtual void set_write_event(NetConnect* conn) override{}
	virtual void set_rdwr_event(NetConnect* conn) override{}
	virtual NetConnect* get_conn(uint32_t conn_id) override
	{
		assert(conn_id == m_conn.id());
		if (conn_id == m_conn.id()) return &m_conn;
		else return nullptr;
	}
	virtual void remove_conn(NetConnect* conn) override
	{
		assert(conn->m_fd != INVALID_SOCKET);
		if (conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			_DEBUGLOG(logger, "sockfd:%d, timerid:%d", conn->m_fd, conn->m_timerid);
			conn->m_state = NetConnectState::NET_CONN_CLOSED;
			if (conn->m_timerid >= 0)
			{
				del_timer(conn->m_timerid);
				conn->m_timerid = -1;
			}
			on_close(conn);
		}
	}

protected:
	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_listen_socket(const char* ip, uint16_t port,
		thread_pool_id_t client_thread_pool, thread_id_t client_thread,
		bool nonblock = true) override
	{
		return {EINVAL, INVALID_SOCKET};
	}

	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_connect_socket(const char* ip, uint16_t port,
		bool nonblock = true, uint32_t seq = 0) override
	{
		return {EINVAL, INVALID_SOCKET};
	}
};

class NonblockConnectThread : public NonblockNetThread
{ //自行DNS解析
public:
	NonblockConnectThread() = default;
	~NonblockConnectThread() = default;
	NonblockConnectThread(const NonblockConnectThread&) = delete;
	NonblockConnectThread& operator=(const NonblockConnectThread&) = delete;

public:
	virtual void process_msg(ThreadMsg& msg) override;

protected:
	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_connect_socket(const char* ip, uint16_t port,
		bool nonblock = true, uint32_t seq = 0) override
	{
		if (m_conn.m_fd != INVALID_SOCKET)
			return {EINPROGRESS,INVALID_SOCKET};
		const auto& r = NetBaseThread::create_connect_socket(ip, port, false, seq);
		if (r.first == 0 && nonblock)
		{
			set_sock_nonblock(r.second);
		}
		return r;
	}
};

class NonblockListenThread : public NonblockNetThread
{ //自行listen
public:
	NonblockListenThread() = default;
	~NonblockListenThread() = default;
	NonblockListenThread(const NonblockListenThread&) = delete;
	NonblockListenThread& operator=(const NonblockListenThread&) = delete;

public:
	virtual void process_msg(ThreadMsg& msg) override;

protected:
	virtual std::pair<int32_t, SOCKET_HANDLE>
		create_listen_socket(const char* ip, uint16_t port,
		thread_pool_id_t client_thread_pool, thread_id_t client_thread,
		bool nonblock = true) override
	{
		if (m_conn.m_fd != INVALID_SOCKET)
			return {EINPROGRESS,INVALID_SOCKET};
		return NetBaseThread::create_listen_socket(ip, port,
			client_thread_pool, client_thread, nonblock);
	}
};

template<typename Selector>
class MultiplexNetThread : public NetBaseThread
{
private:
	std::unordered_map<uint32_t, NetConnect> m_conns;
	std::vector<uint32_t> m_removed_conns;
	Selector m_selector;
public:
	MultiplexNetThread()
		: m_conns()
		, m_removed_conns()
		, m_selector()
	{
	}
	~MultiplexNetThread()
	{
#ifndef NDEBUG //to avoid assert() in conn->destruct()
		for (auto& it : m_conns)
		{
#ifdef _ASYNCPP_DEBUG
			it.second.m_ctx = 0x010203041234dbfellu;
#endif
			it.second.m_state = NetConnectState::NET_CONN_CLOSED;
		}
#endif
	}
public:
	virtual NetConnect* get_conn(uint32_t conn_id) override
	{
		const auto& it = m_conns.find(conn_id);
		if (it != m_conns.end()) return &it->second;
		else return nullptr;
		//return &m_conns[conn_id];
	}
	virtual void process_msg(ThreadMsg& msg) override
	{
		switch (msg.m_type)
		{
		case NET_ACCEPT_CLIENT_REQ:
		{
			NetConnect conn(static_cast<SOCKET_HANDLE>(msg.m_ctx.i64));
			add_conn(&conn);
		}
			break;
		case NET_CONNECT_HOST_REQ:
			if (is_str_ipv4(msg.m_buf))
			{
				auto ctx = (AddConnectorCtx*)msg.m_ctx.obj;
				const auto& r = create_connect_socket(msg.m_buf, ctx->m_port, true, ctx->m_seq);

				_DEBUGLOG(logger, "create_conn result:%d, fd:%d", (int)r.first, (int)r.second);

				ctx->m_ret = r.first;
				ctx->m_connid = static_cast<uint32_t>(r.second);
				get_asynframe()->send_resp_msg(
					NET_CONNECT_HOST_RESP,
					msg.m_buf, msg.m_buf_len, msg.m_buf_type,
					msg.m_ctx, msg.m_ctx_type, msg, this);
				msg.detach();
			}
			else
			{
				char* buf = nullptr;
				MsgCtx ctx = {0};
				auto dnsctx = (AddConnectorCtx*)msg.m_ctx.obj;

				/*复制msg.m_buf*/
				if (msg.m_buf_type == MsgBufferType::STATIC) buf = msg.m_buf;
				else
				{
					buf = (char*)malloc(msg.m_buf_len);
					memcpy(buf, msg.m_buf, msg.m_buf_len);
				}

				/*复制msg.m_ctx*/
				AddConnectorCtx* respctx = new AddConnectorCtx(*dnsctx);
				ctx.obj = respctx;

				dnsctx->m_src_thread_pool_id = msg.m_src_thread_pool_id;
				dnsctx->m_src_thread_id = msg.m_src_thread_id;
				bool bSuccess = get_asynframe()->send_thread_msg(NET_QUERY_DNS_REQ,
					msg.m_buf, msg.m_buf_len, msg.m_buf_type,
					msg.m_ctx, msg.m_ctx_type,
					dns_thread_pool_id, dns_thread_id, this);
				msg.detach();

				if (!bSuccess)
				{
					dnsctx->m_ret = EBUSY;
					bSuccess = get_asynframe()->send_resp_msg(
						NET_CONNECT_HOST_RESP,
						buf, msg.m_buf_len, MsgBufferType::MALLOC,
						ctx, MsgContextType::OBJECT, msg, this);
				}
				else
				{
					if (msg.m_buf_type != MsgBufferType::STATIC) free(buf);
					delete ctx.obj;
				}
				///TODO: set DNS timeout timer
			}
			break;
		case NET_LISTEN_ADDR_REQ:
		{
			auto ctx = (AddListenerCtx*)msg.m_ctx.obj;
			const auto& r = create_listen_socket(msg.m_buf, ctx->m_port,
				ctx->m_client_thread_pool_id, ctx->m_client_thread_id);
			ctx->m_ret = r.first;
			ctx->m_connid = static_cast<uint32_t>(r.second);

			_DEBUGLOG(logger, "create_conn result:%d, fd:%d", (int)r.first, (int)r.second);

			get_asynframe()->send_resp_msg(NET_LISTEN_ADDR_RESP,
				nullptr, 0, MsgBufferType::STATIC, msg.m_ctx, msg.m_ctx_type, msg, this);
			msg.detach();
		}
			break;
		case NET_QUERY_DNS_RESP:
		{
			auto ctx = (AddConnectorCtx*)msg.m_ctx.obj;
			if (ctx->m_ret == 0)
			{
				const auto& r = create_connect_socket(ctx->m_ip, ctx->m_port, true, ctx->m_seq);

				_DEBUGLOG(logger, "create_conn result:%d, fd:%d", (int)r.first, (int)r.second);

				ctx->m_ret = r.first;
				ctx->m_connid = static_cast<uint32_t>(r.second);
			}
			get_asynframe()->send_thread_msg(NET_CONNECT_HOST_RESP,
				msg.m_buf, msg.m_buf_len, msg.m_buf_type,
				msg.m_ctx, msg.m_ctx_type, ctx->m_src_thread_pool_id,
				ctx->m_src_thread_id, this);
			msg.detach();
		}
			break;
		default:
			break;
		}
	}

	virtual int32_t poll() override
	{
		uint32_t mode = 0;
		const auto& s = m_ss.get_cur_speed();
		if (s.first < m_recvspeedlimit)
		{
			mode |= SELIN;
		}
		if (s.second < m_sendspeedlimit)
		{
			mode |= SELOUT;
		}

		int32_t n = 0;
		if (mode)
		{
			n = m_selector.poll(reinterpret_cast<void*>(this), mode, 0);
		}

		if (!m_removed_conns.empty())
		{ // close conn
			for (auto id : m_removed_conns)
			{
#ifdef _ASYNCPP_DEBUG
				NetConnect* conn = get_conn(id);
				assert(conn != nullptr);
				conn->m_ctx = 0x010203041234dbfellu;
#endif
				int32_t ret = static_cast<int32_t>(m_conns.erase(id));
				assert(ret == 1);
				if (ret != 1)
				{
					_WARNLOG(logger, "remove sockfd:%d error, cnt:%d", id, ret);
				}
			}
			m_removed_conns.clear();
		}

		return n;
	}
public:
	virtual void add_conn(NetConnect* conn) override
	{
		SOCKET_HANDLE fd = conn->m_fd;
		assert(fd != INVALID_SOCKET);
		assert(conn->m_state != NetConnectState::NET_CONN_CLOSED);
		assert(conn->m_state != NetConnectState::NET_CONN_CLOSING);
		auto it = m_conns.find(static_cast<uint32_t>(fd));
		if (it != m_conns.end())
		{
			assert(it->second.m_fd == INVALID_SOCKET);
			assert(it->second.m_state == NetConnectState::NET_CONN_CLOSED);
			assert(it->second.m_timerid == -1);
			if (it->second.m_fd == INVALID_SOCKET)
			{
				if (it->second.m_timerid != -1)
				{
					del_timer(it->second.m_timerid);
				}
				_WARNLOG(logger, "overwrite conn, key:%d, new sockfd:%d", (int)it->first, (int)conn->m_fd);
				it->second = std::move(*conn);
			}
			else
			{
				_ERRORLOG(logger, "duplicate sockfd:%d, state:%d, key:%d, new sockfd:%d", (int)it->second.m_fd, (int)it->second.m_state, (int)it->first, (int)conn->m_fd);
				///TODO: close new conn or close old conn
				return;
			}
		}
		else
		{
			it = m_conns.insert(std::make_pair(static_cast<uint32_t>(fd), std::move(*conn))).first;
		}

		set_sock_nonblock(fd);
		conn = &it->second;
		if (conn->m_state == NetConnectState::NET_CONN_CONNECTED)
		{
			conn->m_timerid = add_timer(m_idle_timeout, NetTimeoutTimer, fd);
		}
		else if (conn->m_state == NetConnectState::NET_CONN_CONNECTING)
		{
			conn->m_timerid = add_timer(m_connect_timeout, NetTimeoutTimer, fd);
		}
		int32_t ret = m_selector.add(fd, conn->m_state);
		if (ret == 0)
		{
			_DEBUGLOG(logger, "sockfd:%d, state:%d", (int)fd, (int)conn->m_state);
		}
		else
		{
			///TODO: close conn
			_ERRORLOG(logger, "sockfd:%d, state:%d, add selector ret:%d", (int)fd, (int)conn->m_state, ret);
		}
	}
	virtual void remove_conn(NetConnect* conn) override
	{
		assert(conn->m_fd != INVALID_SOCKET);
		if (conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			_DEBUGLOG(logger, "sockfd:%d, timerid:%d", (int)conn->m_fd, conn->m_timerid);
			conn->m_state = NetConnectState::NET_CONN_CLOSED;
			if (conn->m_timerid >= 0)
			{
				del_timer(conn->m_timerid);
				conn->m_timerid = -1;
			}
			m_selector.del(conn->m_fd);
			m_removed_conns.push_back(conn->id());
			on_close(conn);
		}
	}
	virtual void remove_conn(uint32_t conn_id)
	{
		NetConnect* conn = get_conn(conn_id);
		if (conn != nullptr)
		{
			remove_conn(conn);
		}
		else
		{
			_WARNLOG(logger, "conn %d not found", conn_id);
		}
	}
	virtual void set_read_event(NetConnect* conn) override
	{
		m_selector.set_read_event(conn->m_fd);
	}
	virtual void set_write_event(NetConnect* conn) override
	{
		m_selector.set_write_event(conn->m_fd);
	}
	virtual void set_rdwr_event(NetConnect* conn) override
	{
		m_selector.set_read_write_event(conn->m_fd);
	}
};

/************** Thread Poll ****************/
class AsyncFrame;
class ThreadPool
{
private:
	FixedSizeCircleQueue<ThreadMsg, _ASYNCPP_THREAD_POOL_QUEUE_SIZE> m_msg_queue;
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

public:
	void set_id(thread_pool_id_t id){ m_id = id; }
	thread_pool_id_t get_id(){ return m_id; }

	void set_asynframe(AsyncFrame* asynframe){ m_master = asynframe; }
	AsyncFrame* get_asynframe(){ return m_master; }
	
	decltype(m_msg_queue)& get_msg_queue() { return m_msg_queue; }

	bool push_msg(ThreadMsg&& msg, bool force_receiver_thread = false)
	{
		if (msg.m_dst_thread_id != INVALID_THREAD_ID)
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
			return !force_receiver_thread ?
				m_msg_queue.push(std::move(msg)) : false;
		}
	}
	uint32_t pop(ThreadMsg* msg, uint32_t n){return m_msg_queue.pop(msg, n);}
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

	decltype(m_threads)& get_threads(){ return m_threads; }

	thread_id_t add_thread(BaseThread* new_thread)
	{
		new_thread->set_id(static_cast<thread_id_t>(m_threads.size()));
		new_thread->set_thread_pool(this);
		m_threads.push_back(new_thread);
		return new_thread->get_id();
	}
};

} //end of namespace asyncpp

#endif
