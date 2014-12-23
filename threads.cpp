#include "threads.hpp"
#include "asyncpp.hpp"
#include <cassert>
#include <errno.h>

namespace asyncpp
{

uint32_t BaseThread::check_timer_and_thread_msg()
{
	uint32_t self_msg_cnt = 0;
	uint32_t pool_msg_cnt = 0;
	self_msg_cnt = m_msg_queue.pop(m_msg_cache, MSG_CACHE_SIZE);
	for (uint32_t i = 0; i < self_msg_cnt; ++i)
	{
		process_msg(m_msg_cache[i]);
	}
	if (get_thread_pool_id() != 0)
	{
		pool_msg_cnt = m_master->pop(m_msg_cache, MSG_CACHE_SIZE);
		for (uint32_t i = 0; i < pool_msg_cnt; ++i)
		{
			process_msg(m_msg_cache[i]);
		}
	}

	uint32_t timer_cnt = timer_check();

	return self_msg_cnt + pool_msg_cnt + timer_cnt;
}

void BaseThread::run()
{
	for (;;)
	{
		uint32_t msg_cnt = check_timer_and_thread_msg();
		if (msg_cnt == 0)
		{
			usleep(1 * 1000);
		}
	}
}

thread_id_t BaseThread::get_thread_pool_id() const
{
	return m_master->get_id();
}

AsyncFrame* BaseThread::get_asynframe() const
{
	return m_master->get_asynframe();
}

int32_t dns_query(const char* host, char* ip)
{
	struct addrinfo hints;
	struct addrinfo* result = nullptr;
	struct sockaddr_in* psin = nullptr;
	int32_t ret = 0;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; //IPv4
	hints.ai_socktype = 0; //any address type
	//hints.ai_flags = AI_CANONNAME;
	hints.ai_protocol = 0; //any protocol;
	hints.ai_addrlen = 0;
	hints.ai_addr = nullptr;
	hints.ai_canonname = nullptr;
	hints.ai_next = nullptr;

	ret = getaddrinfo(host, nullptr, &hints, &result);
	if (0 != ret)
	{
		logger_error(logger, "getaddrinfo of %s fail:%d[%s], "
			"errno:%d[%s], result: %p", host, ret,
			gai_strerror(ret), errno, strerror(errno), result);
	}
	else
	{
		psin = reinterpret_cast<struct sockaddr_in *>(result->ai_addr);
		const char* p = inet_ntop(AF_INET,
			&psin->sin_addr, ip, MAX_IP);
		if (NULL == p)
		{
			ret = errno;
			logger_debug(logger, "inet_ntop fail:%d[%s]", ret, strerror(ret));
		}
	}
	freeaddrinfo(result);

	return ret;
}

void DnsThread::process_msg(ThreadMsg& msg)
{
	switch (msg.m_type)
	{
	case NET_QUERY_DNS_REQ:
	{
		QueryDnsRespCtx* dnsctx = dynamic_cast<QueryDnsRespCtx*>(msg.m_ctx.obj);
		dnsctx->m_ret = dns_query(msg.m_buf, dnsctx->m_ip);
		get_asynframe()->send_resp_msg(NET_QUERY_DNS_RESP,
			msg.m_buf, msg.m_buf_len, msg.m_buf_type,
			msg.m_ctx, msg.m_ctx_type, msg, this);
		msg.m_buf = nullptr;
		msg.m_buf_type = MsgBufferType::STATIC;
		msg.m_ctx.obj = nullptr;
		msg.m_ctx_type = MsgContextType::STATIC;
	}
		break;
	default:
		logger_warn(logger, "dns thread recv error mag type:%u", msg.m_type);
		break;
	}
}

void NetBaseThread::do_accept(NetConnect* conn)
{
	for (;;)
	{
		SOCKET_HANDLE fd = accept(conn->m_fd, nullptr, nullptr);
		if (fd != INVALID_SOCKET)
		{
			int32_t ret = on_accept(fd);
			if (ret == 0)
			{
				get_asynframe()->send_thread_msg(NET_ACCEPT_CLIENT_REQ,
					nullptr, 0, MsgBufferType::STATIC,
					{ static_cast<uint64_t>(fd) }, MsgContextType::STATIC,
					conn->m_client_thread_pool, conn->m_client_thread,
					this, false);
			}
			else
			{
				if (ret == 1)
				{ //reject and close conn
					::closesocket(fd);
				}
			}
		}
		else
		{
			int32_t errcode = GET_SOCK_ERR();
			if (errcode != EWOULDBLOCK && errcode != EAGAIN && errcode != EINTR)
			{
				on_error(conn, errcode);
			}
			break;
		}
	}
}

void NetBaseThread::do_connect(NetConnect* conn)
{
	int32_t sockerr = -1;
	socklen_t len = sizeof(sockerr);
	getsockopt(conn->m_fd, SOL_SOCKET, SO_ERROR,
		reinterpret_cast<char*>(&sockerr), &len);
	if (sockerr == 0)
	{ //connect success
		conn->m_state = NetConnectState::NET_CONN_CONNECTED;
	}
}

uint32_t NetBaseThread::do_send(NetConnect* conn)
{
	uint32_t bytes_sent = 0;
	while (!conn->m_send_list.empty())
	{
		SendMsgType& msg = conn->m_send_list.front();
		int32_t n = ::send(conn->m_fd, msg.data + msg.bytes_sent,
			msg.data_len - msg.bytes_sent, MSG_NOSIGNAL);
		if (n >= 0)
		{
			bytes_sent += n;
			msg.bytes_sent += n;
			if (msg.bytes_sent == msg.data_len)
			{
				free_buffer(msg.data, msg.buf_type);
				conn->m_send_list.pop();
			}
			///TODO: return if n==0
		}
		else
		{
			int32_t errcode = GET_SOCK_ERR();
			if (errcode != EWOULDBLOCK && errcode != EAGAIN && errcode != EINTR)
			{
				///TODO: drop msg if fail several times
				on_error(conn, errcode);
			}
			return bytes_sent;
		}
	}
	set_read_event(conn);
	return bytes_sent;
}

uint32_t NetBaseThread::do_recv(NetConnect* conn)
{
	uint32_t bytes_recv = 0;
L_READ:
	int32_t len = conn->m_recv_buf_len - conn->m_recv_len;
	if (len == 0)
	{
		conn->enlarge_recv_buffer();
		len = conn->m_recv_buf_len - conn->m_recv_len;
	}

	int32_t recv_len = recv(conn->m_fd,
		conn->m_recv_buf + conn->m_recv_len,
		len, 0);

	if (recv_len > 0)
	{ //recv data
		bytes_recv += recv_len;
		conn->m_recv_len += recv_len;
		int32_t package_len = frame(conn);
		if (package_len == conn->m_recv_len)
		{ //recv one package
			process_net_msg(conn);
			conn->m_recv_len = 0;
		}
		else if (package_len < conn->m_recv_len)
		{ //recv more than one package
			int32_t remain_len = conn->m_recv_len - package_len;
			process_net_msg(conn);
			memmove(conn->m_recv_buf,
				conn->m_recv_buf + package_len, remain_len);
			conn->m_recv_len = remain_len;
		}
		else
		{ // recv partial package
			conn->enlarge_recv_buffer(package_len);
		}

		if (recv_len == len)
		{
			goto L_READ;
		}
	}
	else if (recv_len == 0)
	{ //peer close conn ///TODO: 半关闭
		if (conn->m_recv_len > 0)
		{
			process_net_msg(conn);
		}
		remove_conn(conn);
	}
	else
	{ //error
		int32_t errcode = GET_SOCK_ERR();
		if (errcode != EWOULDBLOCK && errcode != EAGAIN && errcode != EINTR)
		{
			on_error(conn, errcode);
			remove_conn(conn);
		}
	}

	return bytes_recv;
}

int32_t NetBaseThread::set_sock_nonblock(SOCKET_HANDLE fd)
{
#ifdef _WIN32
	u_long non_block = 1;
	int ret = ioctlsocket(fd, FIONBIO, &non_block);
#else
	//long flags = fcntl(fd, F_GETFL);
	int ret = fcntl(fd, F_SETFL, /*flags |*/ O_NONBLOCK);
#endif
	assert(ret == 0);
	return  ret == 0 ? 0 : GET_SOCK_ERR();
}

std::pair<int32_t, SOCKET_HANDLE>
NetBaseThread::create_listen_socket(const char* ip, uint16_t port,
	thread_pool_id_t client_thread_pool, thread_id_t client_thread)
{
	int ret = 0;
	struct sockaddr_in addr = {};
	SOCKET_HANDLE fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd != INVALID_SOCKET);
	if (fd == INVALID_SOCKET)
		return std::make_pair(GET_SOCK_ERR(), fd);

	ret = set_sock_nonblock(fd);
	if (ret != 0) goto L_ERR;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_pton(AF_INET, ip, reinterpret_cast<void*>(&addr.sin_addr));
	if (ret != 1) goto L_ERR; //@return 0 if ip invalid, -1 if error occur

	ret = bind(fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof addr);
	if (ret != 0) goto L_ERR;

	ret = listen(fd, 100);
	if (ret == 0)
	{
		NetConnect conn(fd, client_thread_pool, client_thread);
		add_conn(&conn);
		return std::make_pair(0, fd);
	}
	else goto L_ERR;

L_ERR:
	ret = GET_SOCK_ERR();
	::closesocket(fd);
	return std::make_pair(ret, fd);
}

std::pair<int32_t, SOCKET_HANDLE>
NetBaseThread::create_connect_socket(const char* ip, uint16_t port)
{
	int ret = 0;
	struct sockaddr_in addr = {};
	SOCKET_HANDLE fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd != INVALID_SOCKET);
	if (fd == INVALID_SOCKET)
		return std::make_pair(GET_SOCK_ERR(), fd);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_pton(AF_INET, ip, reinterpret_cast<void*>(&addr.sin_addr));
	if (ret != 1) goto L_ERR; //@return 0 if ip invalid, -1 if error occur
	for (;;)
	{
		int ret = connect(fd,
			reinterpret_cast<const struct sockaddr*>(&addr), sizeof addr);
		if (ret == 0)
		{
			NetConnect conn(fd);
			add_conn(&conn);
			return std::make_pair(0, fd);
		}
		else
		{
			ret = GET_SOCK_ERR();
			if (ret == EWOULDBLOCK/*win*/ || ret == EINPROGRESS/*linux*/)
			{
				NetConnect conn(fd);
				conn.m_state = NetConnectState::NET_CONN_CONNECTING;
				add_conn(&conn);
				return std::make_pair(0, fd);
			}
			else if (ret == EINTR/*linux*/)
			{
				continue;
			}
			else goto L_ERR;
		}
	}
L_ERR:
	ret = GET_SOCK_ERR();
	::closesocket(fd);
	return std::make_pair(ret, fd);
}

uint32_t NetBaseThread::on_read_event(NetConnect* conn)
{
	switch (conn->m_state)
	{
	case NetConnectState::NET_CONN_CONNECTED:
		return do_recv(conn);
		break;
	case NetConnectState::NET_CONN_LISTENING:
		do_accept(conn);
		break;
	case NetConnectState::NET_CONN_CONNECTING:
		//non-blocking connect check write event
		break;
	case NetConnectState::NET_CONN_CLOSING:
		//ignore
		break;
	case NetConnectState::NET_CONN_CLOSED:
		//ignore
		break;
	case NetConnectState::NET_CONN_ERROR:
		//ignore
		break;
	default:
		assert(0);
		break;
	}
	return 0;
}

uint32_t NetBaseThread::on_write_event(NetConnect* conn)
{
	switch (conn->m_state)
	{
	case NetConnectState::NET_CONN_CONNECTED:
		return do_send(conn);
		break;
	case NetConnectState::NET_CONN_LISTENING:
		//ignore
		break;
	case NetConnectState::NET_CONN_CONNECTING:
		do_connect(conn);
		break;
	case NetConnectState::NET_CONN_CLOSING:
	{
		uint32_t bytes_sent = do_send(conn);
		if (conn->m_send_list.empty())
		{
			remove_conn(conn);
		}
		return bytes_sent;
	}
		break;
	case NetConnectState::NET_CONN_CLOSED:
		//ignore
		break;
	case NetConnectState::NET_CONN_ERROR:
		//ignore
		break;
	default:
		assert(0);
		break;
	}
	return 0;
}

static int32_t http_get_header_len(char* package, uint32_t package_len)
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
	for (; p<end; ++p)
	{
		if (/*p[0]=='\r' && p[1]=='\n'*/ *p == '\n')
		{
			++p; //p += 2;
			return p < end ? p : NULL;
		}
	}
	return NULL;
}

static int32_t http_get_request_header(char* header, uint32_t header_len,
									const char* name, uint32_t name_len,
									char** p_value, uint32_t* p_value_len)
{
	char* line = header;
	char* end = header + header_len;
	do
	{
		if (memcmp(name, line, name_len) == 0)
		{
			char* p = line + 1;
			while (*p != ':')
			{
				if (*p == '\r' || p >= end)
					return ERR_PROTOCOL;
				++p;
			}
			while(isspace((int)(uint8_t)*++p));
			*p_value = p;
			while (*p != '\r' && p<end) ++p;
			*p_value_len = p - *p_value;
			return 0;
		}
		line = http_move_to_next_line(line, header, header_len);
	} while (line);

	return ERR_NOT_FOUND;
}

int32_t NetBaseThread::frame(NetConnect* conn)
{
	if (conn->m_header_len == 0)
	{
		if (conn->m_recv_len < MIN_PACKAGE_SIZE) return MIN_PACKAGE_SIZE;
		if (memcmp(conn->m_recv_buf, "GET", 3) == 0)
		{
			conn->m_net_msg_type = NetMsgType::HTTP_GET;
		}
		else if (memcmp(conn->m_recv_buf, "POST", 4) == 0)
		{
			conn->m_net_msg_type = NetMsgType::HTTP_POST;
		}
		else if (memcmp(conn->m_recv_buf, "HTTP", 4) == 0)
		{
			conn->m_net_msg_type = NetMsgType::HTTP_RESP;
		}
		else return conn->m_recv_len; //unsupported custom binary conn->m_recv_buf

		conn->m_header_len = http_get_header_len(conn->m_recv_buf, conn->m_recv_len);
		if (conn->m_header_len == 0) return conn->m_recv_len * 2;
	}

	if (conn->m_body_len == 0)
	{
		if (conn->m_net_msg_type == NetMsgType::HTTP_GET) return conn->m_header_len;
		char* p;
		uint32_t len;
		if (http_get_request_header(conn->m_recv_buf, conn->m_header_len,
			"Content-Length", strlen("Content-Length"),
			&p, &len) == 0)
		{
			conn->m_body_len = atoi(p);
			return conn->m_header_len + conn->m_body_len;
		}
		else if(http_get_request_header(conn->m_recv_buf, conn->m_header_len,
			"Transfer-Encoding", strlen("Transfer-Encoding"),
			&p, &len) == 0)
		{
			if (memcmp(p, "chunked", len) == 0)
			{
				if (conn->m_net_msg_type == NetMsgType::HTTP_POST)
					conn->m_net_msg_type = NetMsgType::HTTP_POST_CHUNKED;
				else if (conn->m_net_msg_type == NetMsgType::HTTP_RESP)
					conn->m_net_msg_type = NetMsgType::HTTP_RESP_CHUNKED;

				if (conn->m_recv_len > conn->m_header_len + 4) //4 == strlen("\r\n""\r\n")
				{
					char* end;
					p = conn->m_recv_buf + conn->m_header_len;
					conn->m_body_len = strtol(p, &end, 16);
					conn->m_body_len += end - p + 4;
					return conn->m_header_len + conn->m_body_len;
				}
				else return conn->m_recv_len * 2;
			}
			else return conn->m_recv_len; //unsupported
		}
		else return conn->m_recv_len; ///TODO:recv until conn close
	}
	else
	{
		return conn->m_header_len + conn->m_body_len;
	}
}

void NetBaseThread::run()
{
	for (;;)
	{
		uint32_t net_msg_cnt = poll();
		uint32_t thread_msg_cnt = check_timer_and_thread_msg();
		if (thread_msg_cnt == 0 && net_msg_cnt == 0)
		{
			usleep(1 * 1000);
		}
	}
}

int32_t is_str_ipv4(const char* ipv4str)
{
	if (!isdigit(*ipv4str)) return 0;
	do{ ++ipv4str; } while (isdigit(*ipv4str));
	if (*ipv4str++ != '.') return 0;

	if (!isdigit(*ipv4str)) return 0;
	do{ ++ipv4str; } while (isdigit(*ipv4str));
	if (*ipv4str++ != '.') return 0;

	if (!isdigit(*ipv4str)) return 0;
	do{ ++ipv4str; } while (isdigit(*ipv4str));
	if (*ipv4str++ != '.') return 0;

	if (!isdigit(*ipv4str)) return 0;
	do{++ipv4str;}while (isdigit(*ipv4str));
	if (*ipv4str != 0) return 0;

	return 1;
}

void NonblockNetThread::process_msg(ThreadMsg& msg)
{
	switch (msg.m_type)
	{
	case NET_ACCEPT_CLIENT_REQ:
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			logger_debug(logger, "busy, reject client:%d", m_conn.m_fd);
			get_asynframe()->send_resp_msg(NET_ACCEPT_CLIENT_RESP,
				nullptr, 0, MsgBufferType::STATIC,
				{static_cast<uint64_t>(EINPROGRESS) << 32| m_conn.m_fd},
				MsgContextType::STATIC, msg, this);
		}
		else
		{
			auto fd = static_cast<SOCKET_HANDLE>(msg.m_ctx.i64);
			if (set_sock_nonblock(fd) == 0)
			{ //do not send resp
				m_conn = NetConnect(fd);
			}
			else
			{ //resp
			}
		}
		break;
	default:
		logger_warn(logger, "recv error msg:%u", msg.m_type);
		break;
	}
}

int32_t NonblockNetThread::poll()
{
	if (m_conn.m_fd != INVALID_SOCKET)
	{
		uint32_t bytes_sent = 0;
		uint32_t bytes_recv = 0;
		bytes_recv = do_recv(&m_conn);
		if (!m_conn.m_send_list.empty())
		{
			bytes_recv = do_send(&m_conn);
		}
		m_ss.sample(bytes_recv, bytes_recv);
		return bytes_sent + bytes_recv != 0;
	}
	else return 0;
}

} //end of namespace asyncpp
