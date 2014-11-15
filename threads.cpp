#include "threads.hpp"
#include <cassert>

namespace asyncpp
{

void BaseThread::run()
{
	for (;;)
	{
		uint32_t self_msg_cnt = 0;
		uint32_t pool_msg_cnt = 0;
		self_msg_cnt = m_msg_queue.pop(m_msg_cache, MSG_CACHE_SIZE);
		for (uint32_t i = 0; i < self_msg_cnt; ++i)
		{
			process_msg(m_msg_cache[i]);
		}
		if (m_master != nullptr)
		{
			pool_msg_cnt = m_master->pop(m_msg_cache, MSG_CACHE_SIZE);
			for (uint32_t i = 0; i < pool_msg_cnt; ++i)
			{
				process_msg(m_msg_cache[i]);
			}
		}

		uint32_t timer_cnt = timer_check();

		if (self_msg_cnt == 0 && pool_msg_cnt == 0 && timer_cnt == 0)
		{
			usleep(1 * 1000);
		}
	}
}

void MultiWaitNetThread::on_read_event(NetConnect* conn)
{
	switch (conn->m_state)
	{
	case NetConnectState::NET_CONN_CONNECTED:
		on_read(conn);
		break;
	case NetConnectState::NET_CONN_LISTENING:
		on
		break;
	case NetConnectState::NET_CONN_CONNECTING:
		break;
	case NetConnectState::NET_CONN_CLOSING:
		break;
	case NetConnectState::NET_CONN_ERROR:
		break;
	default:
		assert(0);
		break;
	}
}

void MultiWaitNetThread::on_write_event(NetConnect* conn)
{
	switch (conn->m_state)
	{
	case NetConnectState::NET_CONN_CONNECTED:
		break;
	case NetConnectState::NET_CONN_LISTENING:
		break;
	case NetConnectState::NET_CONN_CONNECTING:
		break;
	case NetConnectState::NET_CONN_CLOSING:
		break;
	case NetConnectState::NET_CONN_ERROR:
		break;
	default:
		assert(0);
		break;
	}
}

void MultiWaitNetThread::on_write(NetConnect* conn)
{
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

int32_t MultiWaitNetThread::frame()
{
	if (m_header_len == 0)
	{
		if (m_recv_len < MIN_PACKAGE_SIZE) return MIN_PACKAGE_SIZE;
		if (memcmp(m_recv_buf, "GET", 3) == 0)
		{
			m_net_msg_type = NetMsgType::HTTP_GET;
		}
		else if (memcmp(m_recv_buf, "POST", 4) == 0)
		{
			m_net_msg_type = NetMsgType::HTTP_POST;
		}
		else if (memcmp(m_recv_buf, "HTTP", 4) == 0)
		{
			m_net_msg_type = NetMsgType::HTTP_RESP;
		}
		else return m_recv_len; //unsupported custom binary m_recv_buf

		m_header_len = http_get_header_len(m_recv_buf, m_recv_len);
		if (m_header_len == 0) return m_recv_len * 2;
	}

	if (m_body_len == 0)
	{
		if (m_net_msg_type == NetMsgType::HTTP_GET) return m_header_len;
		char* p;
		uint32_t len;
		if (http_get_request_header(m_recv_buf, m_header_len,
			"Content-Length", strlen("Content-Length"),
			&p, &len) == 0)
		{
			m_body_len = atoi(p);
			return m_header_len + m_body_len;
		}
		else if(http_get_request_header(m_recv_buf, m_header_len,
			"Transfer-Encoding", strlen("Transfer-Encoding"),
			&p, &len) == 0)
		{
			if (memcmp(p, "chunked", len) == 0)
			{
				if (m_net_msg_type == NetMsgType::HTTP_POST)
					m_net_msg_type = NetMsgType::HTTP_POST_CHUNKED;
				else if (m_net_msg_type == NetMsgType::HTTP_RESP)
					m_net_msg_type = NetMsgType::HTTP_RESP_CHUNKED;

				if (m_recv_len > m_header_len + 4) //4 == strlen("\r\n""\r\n")
				{
					char* end;
					p = m_recv_buf + m_header_len;
					m_body_len = strtol(p, &end, 16);
					m_body_len += end - p + 4;
					return m_header_len + m_body_len;
				}
				else return m_recv_len * 2;
			}
			else return m_recv_len; //unsupported
		}
		else return m_recv_len; ///TODO:recv until conn close
	}
	else
	{
		return m_header_len + m_body_len;
	}
}

void MultiWaitNetThread::on_read(NetConnect* conn)
{}

void MultiWaitNetThread::on_accept(NetConnect* conn)
{}

void MultiWaitNetThread::on_close(NetConnect* conn)
{}

void MultiWaitNetThread::on_error(NetConnect* conn, int32_t errcode)
{}

void MultiWaitNetThread::process_msg(NetConnect* conn)
{}

void MultiWaitNetThread::force_close(NetConnect* conn)
{}

void MultiWaitNetThread::reset(NetConnect* conn)
{}

} //end of namespace asyncpp
