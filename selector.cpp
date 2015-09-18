#include "selector.hpp"
#include "asyncpp.hpp"
#include "string_utility.h"
#include <stdio.h>

#ifdef __GNUC__

#ifndef _DISABLE_EPOLL

namespace asyncpp
{

//template class MultiplexNetThread<EpollSelector>;

int32_t EpollSelector::poll(void* p_thread, uint32_t mode, uint32_t ms)
{
	int32_t ret = 0;
	uint32_t bytes_sent_total = 0;
	uint32_t bytes_recv_total = 0;
	auto t = reinterpret_cast<MultiplexNetThread<EpollSelector>*>(p_thread);
	struct epoll_event evs[16];
	ret = epoll_wait(m_fd, evs, 16, ms);
	if(ret > 0)
	{
		uint32_t bytes_sent;
		uint32_t bytes_recv;
		int32_t rp = rand() % ret;

		for(int32_t i = rp; i<ret; ++i)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(evs[i].data.fd));

			if(mode&SELIN && evs[i].events&EPOLLIN)
			{
				bytes_recv = t->on_read_event(conn);
				///TODO: 出错中断，防止重复on_error_event
			}
			else bytes_recv = 0;

			if(mode&SELOUT && evs[i].events&EPOLLOUT)
			{
				bytes_sent = t->on_write_event(conn);
			}
			else butes_sent = 0;

			if(evs[i].events & EPOLLERR)
			{
				++bytes_recv;
				t->on_error_event(conn);
			}

			t->m_ss.sample(bytes_recv, bytes_sent);
			bytes_sent_total += bytes_sent;
			bytes_recv_total += bytes_recv;
		}

		for(int32_t i = 0; i<rp; ++i)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(evs[i].data.fd));

			if(mode&SELIN && evs[i].events&EPOLLIN)
			{
				bytes_recv = t->on_read_event(conn);
				///TODO: 出错中断，防止重复on_error_event
			}
			else bytes_recv = 0;

			if(mode&SELOUT && evs[i].events&EPOLLOUT)
			{
				bytes_sent = t->on_write_event(conn);
			}
			else butes_sent = 0;

			if(evs[i].events & EPOLLERR)
			{
				++bytes_recv;
				t->on_error_event(conn);
			}

			t->m_ss.sample(bytes_recv, bytes_sent);
			bytes_sent_total += bytes_sent;
			bytes_recv_total += bytes_recv;
		}
	}
	else if(ret < 0)
	{
		_WARNLOG(logger, "epoll_wait fail:%d[%s]", errno, strerror(errno));
	}
		
	//t->m_ss.sample(bytes_recv_total, bytes_sent_total);
	return bytes_sent_total + bytes_recv_total; //ret;
}

} //end of namespace asyncpp

#endif

#elif defined(_WIN32)

char* asyncpp_inet_ntop(int af, const void* paddr, char* dst, size_t size)
{
	if (af == AF_INET)
	{
		auto p = (const struct in_addr*)paddr;
		auto pc = (const uint8_t*)&(p->s_addr);
		if (size < 16)
		{
			errno = E2BIG;
			return nullptr;
		}
		sprintf(dst, "%hhu.%hhu.%hhu.%hhu", pc[0], pc[1], pc[2], pc[3]);
		return dst;
	}
	else if (af == AF_INET6)
	{
		//auto p = (const struct in_addr6*)paddr;
		///TODO:
		return dst;
	}
	else
	{
		errno = EAFNOSUPPORT;
		return nullptr;
	}
}

int asyncpp_inet_pton(int af, const char* src, void* dst)
{
	if (af == AF_INET)
	{
		auto p = (struct in_addr*)dst;
		auto pc = (uint8_t*)&(p->s_addr);
		if (!isdigit((uint8_t)*src)) return 0;
		pc[0] = (uint8_t)strtou32(src, &src, 10);
		if (*src++ != '.' && !isdigit((uint8_t)*src)) return 0;
		pc[1] = (uint8_t)strtou32(src, &src, 10);
		if (*src++ != '.' && !isdigit((uint8_t)*src)) return 0;
		pc[2] = (uint8_t)strtou32(src, &src, 10);
		if (*src++ != '.' && !isdigit((uint8_t)*src)) return 0;
		pc[3] = (uint8_t)strtou32(src, &src, 10);

		return 1;
	}
	else if (af == AF_INET6)
	{
		//auto p = (struct in_addr6*)dst;
		///TODO:
		return 0;
	}
	else
	{
		errno = EAFNOSUPPORT;
		return -1;
	}
}

#ifndef _DISABLE_IOCP

namespace asyncpp
{

HANDLE IOCPSelector::m_fd = INVALID_HANDLE_VALUE;

void IOCPSelector::poll(void* p_thread, uint32_t mode, uint32_t ms)
{
}

} //end of namespace asyncpp

#endif

#else
#error os not support
#endif

#ifndef _DISABLE_SELECT

namespace asyncpp
{

//template class MultiplexNetThread<SelSelector>;

int32_t SelSelector::poll(void* p_thread, uint32_t mode, uint32_t ms)
{
	int n = 0;
	uint32_t bytes_sent_total = 0;
	uint32_t bytes_recv_total = 0;
	struct timeval tv = { static_cast<decltype(tv.tv_sec)>(ms / 1000),
		static_cast<decltype(tv.tv_usec)>((ms % 1000) * 1000) };
	auto t = reinterpret_cast<MultiplexNetThread<SelSelector>*>(p_thread);
	SOCKET_HANDLE max_fd = 0;
	std::unordered_map<SOCKET_HANDLE, uint32_t>::const_iterator record_point;
	int32_t rp;
	fd_set read_fds;
	fd_set write_fds;
	fd_set except_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);

	if (!m_removed_fds.empty())
	{ //清除已经关闭了的连接
		for (auto fd : m_removed_fds) m_fds.erase(fd);
		m_removed_fds.clear();
	}
	if (m_fds.empty()) return 0;

	rp = rand() % (int32_t)m_fds.size();
	record_point = m_fds.begin();
	for (auto it = record_point; it != m_fds.end(); ++it)
	{
		if (mode&SELIN && it->second&SELIN)
		{
			//++n;
			if (it->first > max_fd) max_fd = it->first;
			FD_SET(it->first, &read_fds);
		}
		if (mode&SELOUT && it->second&SELOUT)
		{
			//++n;
			if (it->first > max_fd) max_fd = it->first;
			FD_SET(it->first, &write_fds);
		}

		if (n == rp) record_point = it;
		++n;
		FD_SET(it->first, &except_fds);
	}
	if (n == 0)
	{
		t->m_ss.sample(0, 0);
		goto L_RET;
	}

	n = select(static_cast<int>(max_fd + 1), &read_fds, &write_fds, &except_fds, &tv);
	if (n > 0)
	{
		uint32_t bytes_sent;
		uint32_t bytes_recv;
		for (auto it = record_point; it != m_fds.end(); ++it)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(it->first));

			if (FD_ISSET(it->first, &read_fds))
			{
				bytes_recv = t->on_read_event(conn);
				///TODO: 出错中断，防止重复on_error_event
			}
			else bytes_recv = 0;

			if (FD_ISSET(it->first, &write_fds))
			{
				bytes_sent = t->on_write_event(conn);
			}
			else bytes_sent = 0;

			if (FD_ISSET(it->first, &except_fds))
			{
				++bytes_recv;
				t->on_error_event(conn);
			}

			t->m_ss.sample(bytes_recv, bytes_sent);
			bytes_sent_total += bytes_sent;
			bytes_recv_total += bytes_recv;
		}

		for (auto it = m_fds.begin(); it != record_point; ++it)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(it->first));

			if (FD_ISSET(it->first, &read_fds))
			{
				bytes_recv = t->on_read_event(conn);
				///TODO: 出错中断，防止重复on_error_event
			}
			else bytes_recv = 0;

			if (FD_ISSET(it->first, &write_fds))
			{
				bytes_sent = t->on_write_event(conn);
			}
			else bytes_sent = 0;

			if (FD_ISSET(it->first, &except_fds))
			{
				++bytes_recv;
				t->on_error_event(conn);
			}

			t->m_ss.sample(bytes_recv, bytes_sent);
			bytes_sent_total += bytes_sent;
			bytes_recv_total += bytes_recv;
		}
	}
	else if (n < 0)
	{
		_WARNLOG(logger, "select fail:%d[%s], maxfd:%u", GET_SOCK_ERR(), strerror(errno), max_fd);
		n = 0;
	}
	else
	{
		t->m_ss.sample(0, 0);
	}

L_RET:
	//t->m_ss.sample(bytes_recv_total, bytes_sent_total);
	return bytes_sent_total + bytes_recv_total;
}

} //end of namespace asyncpp

#endif
