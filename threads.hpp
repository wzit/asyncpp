#ifndef _THREADS_HPP_
#define _THREADS_HPP_

#include "asyncommon.hpp"
#include "pqueue.hpp"
#include "syncqueue.hpp"
#include "selector.hpp"
#include "byteorder.h"
#include <vector>
#include <queue>
#include <tuple>
#include <utility>
#include <thread>

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
	FixedSizeCircleQueue<ThreadMsg, 128> m_msg_queue;
	pqueue<TimerMsg> m_timer;
	std::unordered_map<uint64_t, MsgContext*> m_ctxs;
	enum {MSG_CACHE_SIZE = 16};
	ThreadMsg m_msg_cache[MSG_CACHE_SIZE];
	ThreadPool* m_master;
	thread_id_t m_id;
	ThreadState m_state;
public:
	BaseThread()
		: m_msg_queue()
		, m_timer()
		, m_ctxs()
		, m_msg_cache()
		, m_master(nullptr)
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
	
	uint32_t check_timer_and_thread_msg();

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
	 添加一个定时器
	 @param wait_time， 多少秒后定时器触发，可以为0(稍后触发)
	*/
	uint32_t add_timer(time_t wait_time, uint32_t type, uint64_t ctx)
	{
		return m_timer.push(TimerMsg(g_unix_timestamp + wait_time, ctx, type));
	}

	/*
	 使用绝对时间添加一个定时器
	 @param expire_time， unix_epoch，可以为早于当前时间(稍后触发)
	*/
	uint32_t add_timer_abs(time_t expire_time, uint32_t type, uint64_t ctx)
	{
		return m_timer.push(TimerMsg(expire_time, ctx, type));
	}

	/*
	 删除一个定时器
	*/
	void del_timer(uint32_t timer_id)
	{
		if (m_timer.is_index_valid(timer_id))
		{
			m_timer.remove(timer_id);
		}
	}

	/*
	 修改定时器时间
	*/
	void change_timer(uint32_t timer_id, time_t expire_time)
	{
		if (m_timer.is_index_valid(timer_id))
		{
			m_timer[timer_id].m_expire_time = expire_time;
			m_timer.change_priority(timer_id);
		}
	}

	/*
	 检查定时器是否到时间
	 一般情况下请不要调用此接口
	*/
	uint32_t timer_check()
	{
		uint32_t cnt = 0;
		time_t cur = g_unix_timestamp;
		while (!m_timer.empty())
		{
			const TimerMsg& front = m_timer.front();
			if (front.m_expire_time <= cur)
			{
				uint32_t type = front.m_type;
				uint64_t ctx = front.m_ctx;
				int32_t timer_id = m_timer.front_index();
				m_timer.pop();

				++cnt;
				_TRACELOG(logger, "run timerid:%d", timer_id);
				on_timer(timer_id, type, ctx);
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
};

int32_t dns_query(const char* host, char* ip);

class DnsThread : public BaseThread
{
public:
	virtual void process_msg(ThreadMsg& msg) override;
};

/************** Net Connection Info ****************/
enum class NetConnectState : uint8_t
{
	NET_CONN_CONNECTED,
	NET_CONN_LISTENING,
	NET_CONN_CONNECTING,
	NET_CONN_CLOSING,
	NET_CONN_CLOSED,
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
	char* m_recv_buf;
	int32_t m_recv_len;
	int32_t m_recv_buf_len;
	int32_t m_header_len;
	int32_t m_body_len;
	NetMsgType m_net_msg_type;
	SOCKET_HANDLE m_fd;
	thread_pool_id_t m_client_thread_pool; //for listen socket only
	thread_id_t m_client_thread; //for listen socket only
	NetConnectState m_state;

	NetConnect()
		: m_send_list()
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_fd(INVALID_SOCKET)
		, m_client_thread_pool(INVALID_THREAD_POOL_ID)
		, m_client_thread(INVALID_THREAD_ID)
		, m_state(NetConnectState::NET_CONN_CONNECTING)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		NetConnectState state = NetConnectState::NET_CONN_CONNECTED)
		: m_send_list()
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_fd(fd)
		, m_client_thread_pool(INVALID_THREAD_POOL_ID)
		, m_client_thread(INVALID_THREAD_ID)
		, m_state(state)
	{
	}
	NetConnect(SOCKET_HANDLE fd,
		thread_pool_id_t client_thread_pool, thread_id_t client_thread)
		: m_send_list()
		, m_recv_buf(nullptr)
		, m_recv_len(0)
		, m_recv_buf_len(0)
		, m_header_len(0)
		, m_body_len(0)
		, m_net_msg_type(NetMsgType::CUSTOM_BIN)
		, m_fd(fd)
		, m_client_thread_pool(client_thread_pool)
		, m_client_thread(client_thread)
		, m_state(NetConnectState::NET_CONN_LISTENING)
	{
	}
	~NetConnect()
	{
		if (m_recv_buf != nullptr) free(m_recv_buf);
		if (m_fd != INVALID_SOCKET) ::closesocket(m_fd);
	}
	void destruct()
	{
		m_recv_len = 0;
		if (m_fd != INVALID_SOCKET)
		{
			::closesocket(m_fd);
			m_fd = INVALID_SOCKET;
		}
	}
	NetConnect(const NetConnect&) = delete;
	NetConnect& operator=(const NetConnect&) = delete;
	NetConnect(NetConnect&& val)
	{
		*this = std::move(val);
	}
	NetConnect& operator=(NetConnect&& val)
	{
		if (&val != this)
		{
			m_send_list = std::move(val.m_send_list);
			m_recv_buf = val.m_recv_buf; val.m_recv_buf = nullptr;
			m_recv_len = val.m_recv_len;
			m_recv_buf_len = val.m_recv_buf_len;
			m_header_len = val.m_header_len;
			m_body_len = val.m_body_len;
			m_net_msg_type = val.m_net_msg_type;
			m_fd = val.m_fd; val.m_fd = INVALID_SOCKET; //do NOT close fd
			m_client_thread_pool = val.m_client_thread_pool;
			m_client_thread = val.m_client_thread;
			m_state = val.m_state;
		}
		return *this;
	}
	void enlarge_recv_buffer(int32_t n)
	{
		if (n > m_recv_buf_len)
		{
			m_recv_buf_len = n;
			m_recv_buf = static_cast<char*>(realloc(m_recv_buf, n));
			assert(m_recv_buf != nullptr);
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
	int32_t send(char* msg, uint32_t msg_len,
		MsgBufferType buf_type = MsgBufferType::STATIC)
	{
		if (m_send_list.size() < 64)
		{
			m_send_list.push({ msg, msg_len, 0, buf_type });
			return 0;
		}
		else return EAGAIN;
	}
	std::pair<std::string, uint16_t> get_addr()
	{
		char ip[MAX_IP] = {};
		struct sockaddr_in addr;
		socklen_t len = sizeof addr;
		int ret = getsockname(m_fd,
			reinterpret_cast<struct sockaddr*>(&addr), &len);
		if (ret != 0) return {std::string(), 0};
		inet_ntop(addr.sin_family,
			reinterpret_cast<void*>(&addr.sin_addr), ip, MAX_IP);
		return {std::string(ip), be16toh(addr.sin_port)};
	}
	std::pair<std::string, uint16_t> get_peer_addr()
	{
		char ip[MAX_IP] = {};
		struct sockaddr_in addr;
		socklen_t len = sizeof addr;
		int ret = getpeername(m_fd,
			reinterpret_cast<struct sockaddr*>(&addr), &len);
		if (ret != 0) return {std::string(), 0};
		inet_ntop(addr.sin_family,
			reinterpret_cast<void*>(&addr.sin_addr), ip, MAX_IP);
		return {std::string(ip), be16toh(addr.sin_port)};
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

class QueryDnsRespCtx : public MsgContext
{
public:
	int32_t m_ret;
	thread_pool_id_t m_src_thread_pool_id;
	thread_id_t m_src_thread_id;
	uint16_t m_port;
	char m_ip[MAX_IP];

	QueryDnsRespCtx() = default;
	virtual ~QueryDnsRespCtx() = default;

	QueryDnsRespCtx(const QueryDnsRespCtx&) = delete;
	QueryDnsRespCtx& operator=(const QueryDnsRespCtx&) = delete;

};

enum ReservedThreadMsgType
{
	MSG_TYPE_UNKNOWN,
	STOP_THREAD,

	NET_CONNECT_HOST_REQ,   //msg.m_buf = host
							//msg.m_ctx.i64 = port
	NET_CONNECT_HOST_RESP,	//msg.m_buf = host
							//msg.m_ctx.i64 = ret<<32 | fd
	
	NET_LISTEN_ADDR_REQ,	//msg.m_buf = ip
							//msg.m_ctx.i64 = port<<32 | client_thread_pool_id<<16 | client_thread_id
	NET_LISTEN_ADDR_RESP,	//msg.m_buf = ip
							//msg.m_ctx.i64 = ret<<32 | fd
	
	NET_ACCEPT_CLIENT_REQ,  //msg.m_ctx.i64 = fd
	NET_ACCEPT_CLIENT_RESP,	//response ONLY on ERROR, msg.m_ctx.i64 = ret<<32 | fd
	
	NET_QUERY_DNS_REQ,      //msg.m_buf = host
							//msg.m_ctx.obj = QueryDnsRespCtx*
	NET_QUERY_DNS_RESP,		//msg.m_buf = host
							//msg.m_ctx.obj = QueryDnsRespCtx*

	NET_CLOSE_CONN_REQ,		//msg.m_ctx.i64 = force_close << 32 | conn_id
	NET_CLOSE_CONN_RESP,	//no response

	NET_MSG_TYPE_NUMBER
};

/************** Net Recv & Send Thread ****************/
class NetBaseThread : public BaseThread
{
public:
	SpeedSample<16> m_ss;
public:
	NetBaseThread() = default;
	~NetBaseThread() = default;
	NetBaseThread(NetBaseThread&) = delete;
	NetBaseThread& operator=(const NetBaseThread&) = delete;
public:
	virtual void run() override;
public:
	/*一般情况下，请勿调用这些函数*/
	uint32_t on_read_event(NetConnect* conn);
	uint32_t on_write_event(NetConnect* conn);
	void on_error_event(NetConnect* conn)
	{
		on_error(conn, GET_SOCK_ERR());
	}
protected:
	/*一般情况下，请勿调用这些函数*/
	uint32_t do_accept(NetConnect* conn);
	uint32_t do_connect(NetConnect* conn);
	uint32_t do_send(NetConnect* conn);
	uint32_t do_recv(NetConnect* conn);
	int32_t set_sock_nonblock(SOCKET_HANDLE fd);
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
			bool nonblock = true);
protected:
	/*
	 线程内部接口，获取一个消息的长度
	 当框架能够满足此消息长度后回调process_net_msg(conn)
	 @return 预期的消息长度
	*/
	virtual int32_t frame(NetConnect* conn);

	/*
	 accept一个客户端后回调
	 @return 0 pass
	         1 reject and close client conn
			 2 reject but do NOT close client conn
	 */
	virtual int32_t on_accept(SOCKET_HANDLE fd){ return 0; }

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
	*/
	virtual void on_error(NetConnect* conn, int32_t errcode){}

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
			_TRACELOG(logger, "socket_fd:%d", conn->m_fd);
			set_write_event(conn);
			conn->m_state = NetConnectState::NET_CONN_CLOSING;
		}
	}

	/*
	 强制关闭连接
	*/
	virtual void force_close(NetConnect* conn){ remove_conn(conn); }

	/*
	 强制关闭连接，对端将会收到RST
	*/
	virtual void reset(NetConnect* conn)
	{
		struct linger so_linger = { 1, 0 }; //set linger on and wait 0s (i.e. do NOT wait)
		setsockopt(conn->m_fd, SOL_SOCKET, SO_LINGER,
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
			if (conn->m_send_list.empty())
			{
				set_rdwr_event(conn);
			}
			return conn->send(msg, msg_len, buf_type);
		}
		else
		{
			assert(0);
			return 0;
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
		return send(get_conn(conn_id), msg, msg_len, buf_type);
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

public:
	/*一般情况下，请勿调用这些函数*/
	virtual int32_t add_conn(NetConnect* conn) = 0;
	virtual int32_t remove_conn(NetConnect* conn) = 0;
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
	~NonblockNetThread() = default;
	NonblockNetThread(const NonblockNetThread&) = delete;
	NonblockNetThread& operator=(const NonblockNetThread&) = delete;

public:
	virtual void process_msg(ThreadMsg& msg) override;
	virtual void process_net_msg(NetConnect* conn) override{}
	virtual int32_t poll() override
	{
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			uint32_t bytes_recv = on_read_event(&m_conn);
			uint32_t bytes_sent = on_write_event(&m_conn);
			m_ss.sample(bytes_recv, bytes_sent);
			if (m_conn.m_state == NetConnectState::NET_CONN_CLOSED)
				m_conn.destruct();
			return bytes_sent + bytes_recv;
		}
		else return 0;
	}

	virtual int32_t add_conn(NetConnect* conn) override
	{ ///TODO: check m_conn.m_fd
		m_conn = std::move(*conn);
		return 0;
	}
	virtual void set_read_event(NetConnect* conn) override{}
	virtual void set_write_event(NetConnect* conn) override{}
	virtual void set_rdwr_event(NetConnect* conn) override{}
	virtual NetConnect* get_conn(uint32_t conn_id) override
	{
		return &m_conn;
	}
	virtual int32_t remove_conn(NetConnect* conn) override
	{
		assert(conn->m_fd != INVALID_SOCKET);
		if (conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			_TRACELOG(logger, "socket_fd:%d", conn->m_fd);
			conn->m_state = NetConnectState::NET_CONN_CLOSED;
			on_close(conn);
		}
		return 0;
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
		bool nonblock = true) override
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
		bool nonblock = true) override
	{
		const auto& r = NetBaseThread::create_connect_socket(ip, port, false);
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
public:
	virtual NetConnect* get_conn(uint32_t conn_id) override
	{
		return &m_conns[conn_id];
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
				const auto& r = create_connect_socket(msg.m_buf,
					static_cast<uint16_t>(msg.m_ctx.i64));

				get_asynframe()->send_resp_msg(NET_CONNECT_HOST_RESP,
					nullptr, 0, MsgBufferType::STATIC,
					{ static_cast<uint64_t>(r.first) << 32 |
					static_cast<uint32_t>(r.second) },
					MsgContextType::STATIC, msg, this);
			}
			else
			{
				MsgCtx ctx;
				QueryDnsRespCtx* dnsctx = new QueryDnsRespCtx;
				dnsctx->m_src_thread_pool_id = msg.m_src_thread_pool_id;
				dnsctx->m_src_thread_id = msg.m_src_thread_id;
				dnsctx->m_port = static_cast<uint16_t>(msg.m_ctx.i64);
				ctx.obj = dnsctx;
				get_asynframe()->send_thread_msg(NET_QUERY_DNS_REQ,
					msg.m_buf, msg.m_buf_len, msg.m_buf_type,
					ctx, MsgContextType::OBJECT,
					dns_thread_pool_id, dns_thread_id, this);
				msg.m_buf = nullptr;
				msg.m_buf_type = MsgBufferType::STATIC;
				///TODO: set DNS timeout timer
			}
			break;
		case NET_LISTEN_ADDR_REQ:
		{
			const auto& r = create_listen_socket(msg.m_buf,
				static_cast<uint16_t>(msg.m_ctx.i64 >> 32),
				static_cast<thread_pool_id_t>(msg.m_ctx.i64 >> 16),
				static_cast<thread_pool_id_t>(msg.m_ctx.i64));

			get_asynframe()->send_resp_msg(NET_LISTEN_ADDR_RESP,
				nullptr, 0, MsgBufferType::STATIC,
				{ static_cast<uint64_t>(r.first) << 32 |
				static_cast<uint32_t>(r.second) },
				MsgContextType::STATIC, msg, this);
		}
			break;
		case NET_QUERY_DNS_RESP:
		{
			MsgCtx respctx;
			QueryDnsRespCtx* dnsctx = dynamic_cast<QueryDnsRespCtx*>(msg.m_ctx.obj);
			if (dnsctx->m_ret == 0)
			{
				const auto& r = create_connect_socket(dnsctx->m_ip, dnsctx->m_port);
				respctx.i64 = static_cast<uint64_t>(r.first) << 32
					| static_cast<uint32_t>(r.second);
			}
			else
			{
				respctx.i64 = static_cast<uint64_t>(dnsctx->m_ret) << 32
					| 0xffffffffllu;
			}
			get_asynframe()->send_thread_msg(NET_CONNECT_HOST_RESP,
				msg.m_buf, msg.m_buf_len, msg.m_buf_type,
				respctx, MsgContextType::STATIC, dnsctx->m_src_thread_pool_id,
				dnsctx->m_src_thread_id, this);
			msg.m_buf = nullptr;
			msg.m_buf_type = MsgBufferType::STATIC;
		}
			break;
		default:
			break;
		}
	}

	virtual int32_t poll() override
	{
		int32_t n = m_selector.poll(reinterpret_cast<void*>(this), 0);

		if (!m_removed_conns.empty())
		{ // close conn
			for (auto id : m_removed_conns)
			{
				m_conns.erase(id);
			}
			m_removed_conns.clear();
		}

		return n;
	}
public:
	virtual int32_t add_conn(NetConnect* conn) override
	{
		if (set_sock_nonblock(conn->m_fd) == 0)
		{
			_TRACELOG(logger, "socket_fd:%d", conn->m_fd);
			m_selector.add(conn->m_fd);
			m_conns.insert(std::make_pair(conn->id(), std::move(*conn)));
			return 0;
		}
		else return 1;
	}
	virtual int32_t remove_conn(NetConnect* conn) override
	{
		assert(conn->m_fd != INVALID_SOCKET);
		if (conn->m_state != NetConnectState::NET_CONN_CLOSED)
		{
			_TRACELOG(logger, "socket_fd:%d", conn->m_fd);
			conn->m_state = NetConnectState::NET_CONN_CLOSED;
			m_selector.del(conn->m_fd);
			m_removed_conns.push_back(conn->id());
			on_close(conn);
		}
		return 0;
	}
	virtual int32_t remove_conn(uint32_t conn_id)
	{
		return remove_conn(&m_conns[conn_id]);
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
