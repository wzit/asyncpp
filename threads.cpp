﻿#include "threads.hpp"
#include "asyncpp.hpp"
#include "http_utility.h"
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
		_TRACELOG(logger, "msg_type:%d, from %hu:%hu, to %hu:%hu",
			m_msg_cache[i].m_type, m_msg_cache[i].m_src_thread_pool_id,
			m_msg_cache[i].m_src_thread_id, m_msg_cache[i].m_dst_thread_pool_id,
			m_msg_cache[i].m_dst_thread_id);
		process_msg(m_msg_cache[i]);
	}
	if (get_thread_pool_id() != 0)
	{
		pool_msg_cnt = m_master->pop(m_msg_cache, MSG_CACHE_SIZE);
		for (uint32_t i = 0; i < pool_msg_cnt; ++i)
		{
			_TRACELOG(logger, "msg_type:%d, from %hu:%hu, to %hu:%hu",
				m_msg_cache[i].m_type, m_msg_cache[i].m_src_thread_pool_id,
				m_msg_cache[i].m_src_thread_id, m_msg_cache[i].m_dst_thread_pool_id,
				m_msg_cache[i].m_dst_thread_id);
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
			usleep(10 * 1000);
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
		_ERRORLOG(logger, "getaddrinfo of %s fail:%d[%s], "
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
			_DEBUGLOG(logger, "inet_ntop fail:%d[%s]", ret, strerror(ret));
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
		msg.detach();
	}
		break;
	default:
		_WARNLOG(logger, "dns thread recv error msg type:%u,"
			" from %hu:%hu, to %hu:%hu", msg.m_type,
			msg.m_src_thread_pool_id, msg.m_src_thread_id,
			msg.m_dst_thread_pool_id, msg.m_dst_thread_id);
		break;
	}
}

uint32_t NetBaseThread::do_accept(NetConnect* conn)
{
	uint32_t accept_cnt = 0;
	for (;;)
	{
		SOCKET_HANDLE fd = accept(conn->m_fd, nullptr, nullptr);
		if (fd != INVALID_SOCKET)
		{
			_TRACELOG(logger, "socket_fd:%d accept %d", conn->m_fd, fd);
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
			_TRACELOG(logger, "socket_fd:%d", fd);
			++accept_cnt;
		}
		else
		{
			int32_t errcode = GET_SOCK_ERR();
			if (errcode != WSAEWOULDBLOCK && errcode != EAGAIN && errcode != WSAEINTR)
			{
				on_error_event(conn);
				_INFOLOG(logger, "socket_fd:%d error:%d", errcode);
			}
			break;
		}
	}
	return accept_cnt;
}

uint32_t NetBaseThread::do_connect(NetConnect* conn)
{
	int32_t sockerr = -1;
	socklen_t len = sizeof(sockerr);
	_TRACELOG(logger, "socket_fd:%d", conn->m_fd);
	getsockopt(conn->m_fd, SOL_SOCKET, SO_ERROR,
		reinterpret_cast<char*>(&sockerr), &len);
	if (sockerr == 0)
	{ //connect success
		conn->m_state = NetConnectState::NET_CONN_CONNECTED;
		on_connect(conn);
		return 1;
	}
	return 0;
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
			_TRACELOG(logger, "socket_fd:%d send %uB", conn->m_fd, n);
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
			if (errcode != WSAEWOULDBLOCK && errcode != EAGAIN && errcode != WSAEINTR)
			{
				///TODO: drop msg if fail several times
				on_error_event(conn);
				_INFOLOG(logger, "socket_fd:%d error:%d", errcode);
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
		_TRACELOG(logger, "socket_fd:%d recv %uB, total:%uB",
			conn->m_fd, recv_len, conn->m_recv_len);
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
		_TRACELOG(logger, "socket_fd:%d close");
		if (conn->m_recv_len > 0)
		{
			process_net_msg(conn);
		}
		remove_conn(conn);
	}
	else
	{ //error
		int32_t errcode = GET_SOCK_ERR();
		if (errcode != WSAEWOULDBLOCK && errcode != EAGAIN && errcode != WSAEINTR)
		{
			on_error_event(conn);
			_INFOLOG(logger, "socket_fd:%d error:%d", errcode);
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

SOCKET_HANDLE NetBaseThread::create_tcp_socket(bool nonblock)
{
#ifdef SOCK_NONBLOCK
	return socket(AF_INET, 
		nonblock ? SOCK_STREAM|SOCK_NONBLOCK : SOCK_STREAM, 0);
#else
	SOCKET_HANDLE fd = socket(AF_INET, SOCK_STREAM, 0);
	if (nonblock)
	{
		int ret = set_sock_nonblock(fd);
		assert(ret == 0);
	}
	return fd;
#endif
}

std::pair<int32_t, SOCKET_HANDLE>
NetBaseThread::create_listen_socket(const char* ip, uint16_t port,
	thread_pool_id_t client_thread_pool, thread_id_t client_thread,
	bool nonblock)
{
	int ret = 0;
	struct sockaddr_in addr = {};
	SOCKET_HANDLE fd = create_tcp_socket(nonblock);
	assert(fd != INVALID_SOCKET);
	if (fd == INVALID_SOCKET)
		return std::make_pair(GET_SOCK_ERR(), fd);

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
NetBaseThread::create_connect_socket(const char* ip,
	uint16_t port, bool nonblock)
{
	int ret = 0;
	struct sockaddr_in addr = {};
	SOCKET_HANDLE fd = create_tcp_socket(nonblock);
	assert(fd != INVALID_SOCKET);
	if (fd == INVALID_SOCKET)
		return std::make_pair(GET_SOCK_ERR(), fd);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	ret = inet_pton(AF_INET, ip, reinterpret_cast<void*>(&addr.sin_addr));
	if (ret != 1)
	{ //@return 0 if ip invalid, -1 if error occur
		ret = GET_SOCK_ERR();
		goto L_ERR;
	}
	for (;;)
	{
		ret = connect(fd,
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
			if (ret == WSAEWOULDBLOCK/*win*/ || ret == WSAEINPROGRESS/*linux*/)
			{
				NetConnect conn(fd, NetConnectState::NET_CONN_CONNECTING);
				add_conn(&conn);
				return std::make_pair(0, fd);
			}
			else if (ret == WSAEINTR/*linux*/)
			{
				continue;
			}
			else goto L_ERR;
		}
	}
L_ERR:
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
		return do_accept(conn);
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
		if (!conn->m_send_list.empty())
		{
			return do_send(conn);
		}
		break;
	case NetConnectState::NET_CONN_LISTENING:
		//ignore
		break;
	case NetConnectState::NET_CONN_CONNECTING:
		return do_connect(conn);
		break;
	case NetConnectState::NET_CONN_CLOSING:
	{
		uint32_t bytes_sent = 0;
		if (!conn->m_send_list.empty())
		{
			bytes_sent = do_send(conn);
		}
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

int32_t calc_http_chunked(NetConnect* conn)
{
	int32_t chunklen = -1;

	if (conn->m_recv_len < conn->m_header_len + conn->m_body_len)
		return conn->m_header_len + conn->m_body_len;

	char* end = conn->m_recv_buf + conn->m_recv_len;
	do
	{
		/** pchunk ->
		1, 1*HEX [chunk-extension] CRLF chunk-body CRLF ....
		2, CRLF 1*HEX [chunk-extension] CRLF chunk-body CRLF ....
		3, CRLF 0 CRLF [trailer] CRLF
		*/
		char* pchunk = conn->m_recv_buf +
			conn->m_header_len + conn->m_body_len;
		char* pchunkbody = pchunk;
		for (; pchunkbody < end - 1; ++pchunkbody)
		{ //ignore chunk-extension
			if (pchunkbody[0] == '\r' && pchunkbody[1] == '\n')
				break;
		}
		if (pchunkbody == end - 1)
		{
			return conn->m_recv_len * 2;
		}
		else
		{
			// pchunkbody -> CRLF
			// CRLF 1*HEX CRLF
			// ^          ^
			chunklen = strtol(pchunk, &pchunkbody, 16);
			// CRLF 1*HEX CRLF
			//          ^

			for (; pchunkbody < end - 1; ++pchunkbody)
			{ //ignore chunk-extension
				if (pchunkbody[0] == '\r' && pchunkbody[1] == '\n')
					break;
			}
			if (pchunkbody == end - 1)
			{
				return conn->m_recv_len * 2;
			}
			// CRLF 1*HEX CRLF
			//            ^

			if (chunklen != 0)
			{
				conn->m_body_len += chunklen;
				pchunkbody += strlen("\r\n");
				conn->m_recv_len -= pchunkbody - pchunk;
				memmove(pchunk, pchunkbody, end - pchunkbody);
			}
			else
			{
				// do NOT support chunk trailer
				conn->m_recv_len -= pchunkbody - pchunk + strlen("\r\n\r\n");
			}
		}
	} while (conn->m_header_len + conn->m_body_len < conn->m_recv_len);

	return conn->m_header_len + conn->m_body_len;
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
			{ //暂不支持 chunk-extension , trailer
				if (conn->m_net_msg_type == NetMsgType::HTTP_POST)
					conn->m_net_msg_type = NetMsgType::HTTP_POST_CHUNKED;
				else if (conn->m_net_msg_type == NetMsgType::HTTP_RESP)
					conn->m_net_msg_type = NetMsgType::HTTP_RESP_CHUNKED;

				return calc_http_chunked(conn);

			}
			else return conn->m_recv_len; //unsupported
		}
		else return conn->m_recv_len; ///TODO:recv until conn close
	}
	else
	{
		if (conn->m_net_msg_type == NetMsgType::HTTP_POST_CHUNKED
			|| conn->m_net_msg_type == NetMsgType::HTTP_RESP_CHUNKED)
			return calc_http_chunked(conn);
		else return conn->m_header_len + conn->m_body_len;
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
			usleep(10 * 1000);
		}
	}
}

void NonblockNetThread::process_msg(ThreadMsg& msg)
{
	int ret = 0;
	switch (msg.m_type)
	{
	case NET_ACCEPT_CLIENT_REQ:
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			ret = EINPROGRESS;
		}
		else
		{
			auto fd = static_cast<SOCKET_HANDLE>(msg.m_ctx.i64);
			ret = set_sock_nonblock(fd);
			if ( ret == 0)
			{ //do not send resp
				m_conn = NetConnect(fd);
				return; //NO resp on success
			}
		}
		break;
	default:
		ret = EINVAL;
		_WARNLOG(logger, "recv error msg:%u,"
			" from %hu:%hu, to %hu:%hu", msg.m_type,
			msg.m_src_thread_pool_id, msg.m_src_thread_id,
			msg.m_dst_thread_pool_id, msg.m_dst_thread_id);
		return;
		break;
	}
	_DEBUGLOG(logger, "reject client:%llu, result:%d", msg.m_ctx.i64, ret);
	get_asynframe()->send_resp_msg(NET_ACCEPT_CLIENT_RESP,
		nullptr, 0, MsgBufferType::STATIC,
		{ static_cast<uint64_t>(ret) << 32 | m_conn.m_fd },
		MsgContextType::STATIC, msg, this);
}

void NonblockConnectThread::process_msg(ThreadMsg& msg)
{
	int ret = 0;
	switch (msg.m_type)
	{
	case NET_CONNECT_HOST_REQ:
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			ret = EINPROGRESS;
		}
		else
		{
			char ip[MAX_IP];
			ret = dns_query(msg.m_buf, ip);
			if (ret == 0)
			{
				const auto& r = create_connect_socket(ip,
					static_cast<uint16_t>(msg.m_ctx.i64));
				assert(ret == 0);
				ret = r.first;
			}
		}
		break;
	default:
		ret = EINVAL;
		_WARNLOG(logger, "recv error msg:%u,"
			" from %hu:%hu, to %hu:%hu", msg.m_type,
			msg.m_src_thread_pool_id, msg.m_src_thread_id,
			msg.m_dst_thread_pool_id, msg.m_dst_thread_id);
		return;
		break;
	}
	_DEBUGLOG(logger, "%s, result:%d", msg.m_buf, ret);
	get_asynframe()->send_resp_msg(NET_CONNECT_HOST_RESP,
		msg.m_buf, msg.m_buf_len, msg.m_buf_type,
		{ static_cast<uint64_t>(ret) << 32 | m_conn.m_fd },
		MsgContextType::STATIC, msg, this);
	msg.m_buf = nullptr;
	msg.m_buf_type = MsgBufferType::STATIC;
}

void NonblockListenThread::process_msg(ThreadMsg& msg)
{
	int ret = 0;
	switch (msg.m_type)
	{
	case NET_LISTEN_ADDR_REQ:
		if (m_conn.m_fd != INVALID_SOCKET)
		{
			ret = EINPROGRESS;
		}
		else
		{
			const auto& r = create_listen_socket(msg.m_buf,
				static_cast<uint16_t>(msg.m_ctx.i64 >> 32),
				static_cast<thread_pool_id_t>(msg.m_ctx.i64 >> 16),
				static_cast<thread_id_t>(msg.m_ctx.i64));
			ret = r.first;
		}
		break;
	case NET_ACCEPT_CLIENT_RESP:
		_WARNLOG(logger, "recv accept client resp:%#llx,"
			" from %hu:%hu, to %hu:%hu", msg.m_ctx.i64,
			msg.m_src_thread_pool_id, msg.m_src_thread_id,
			msg.m_dst_thread_pool_id, msg.m_dst_thread_id);
		return;
		break;
	default:
		ret = EINVAL;
		_WARNLOG(logger, "recv error msg:%u,"
			" from %hu:%hu, to %hu:%hu", msg.m_type,
			msg.m_src_thread_pool_id, msg.m_src_thread_id,
			msg.m_dst_thread_pool_id, msg.m_dst_thread_id);
		return;
		break;
	}
	_DEBUGLOG(logger, "%s, result:%d", msg.m_buf, ret);
	get_asynframe()->send_resp_msg(NET_LISTEN_ADDR_RESP,
		msg.m_buf, msg.m_buf_len, msg.m_buf_type,
		{ static_cast<uint64_t>(ret) << 32 | m_conn.m_fd },
		MsgContextType::STATIC, msg, this);
	msg.m_buf = nullptr;
	msg.m_buf_type = MsgBufferType::STATIC;
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

} //end of namespace asyncpp
