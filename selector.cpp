#include "selector.hpp"
#include "asyncpp.hpp"

#ifdef __GNUC__

#ifndef _DISABLE_EPOLL

namespace asyncpp
{

//template class MultiplexNetThread<EpollSelector>;

int32_t EpollSelector::poll(void* p_thread, uint32_t mode, uint32_t ms)
{
	int32_t ret = 0;
	uint32_t bytes_sent = 0;
	uint32_t bytes_recv = 0;
	auto t = reinterpret_cast<MultiplexNetThread<EpollSelector>*>(p_thread);
	struct epoll_event evs[16];
	ret = epoll_wait(m_fd, evs, 16, ms);
	if(ret > 0)
	{
		for(int32_t i = 0; i<ret; ++i)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(evs[i].data.fd));
			if(mode&SELIN && evs[i].events&EPOLLIN)
			{
				bytes_recv += t->on_read_event(conn);
			}
			if(mode&SELOUT && evs[i].events&EPOLLOUT)
			{
				bytes_sent += t->on_write_event(conn);
			}
			if(evs[i].events & EPOLLERR)
			{
				++bytes_recv;
				t->on_error_event(conn);
			}
		}
	}
	else if(ret < 0)
	{
		fprintf(stderr, "epoll_wait fail:%d[%s]\n", errno, strerror(errno));
	}
		
	t->m_ss.sample(bytes_recv, bytes_sent);
	return bytes_sent + bytes_recv; //ret;
}

} //end of namespace asyncpp

#endif

#elif defined(_WIN32)

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
	uint32_t bytes_sent = 0;
	uint32_t bytes_recv = 0;
	struct timeval tv = { static_cast<decltype(tv.tv_sec)>(ms / 1000),
		static_cast<decltype(tv.tv_usec)>((ms % 1000) * 1000) };
	auto t = reinterpret_cast<MultiplexNetThread<SelSelector>*>(p_thread);
	SOCKET_HANDLE max_fd = 0;
	fd_set read_fds;
	fd_set write_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	if (!m_removed_fds.empty())
	{ //清除已经关闭了的连接
		for (auto fd : m_removed_fds) m_fds.erase(fd);
		m_removed_fds.clear();
	}

	for (auto it = m_fds.begin(); it != m_fds.end(); ++it)
	{
		if (mode&SELIN && it->second&SELIN)
		{
			++n;
			if (it->first > max_fd) max_fd = it->first;
			FD_SET(it->first, &read_fds);
		}
		if (mode&SELOUT && it->second&SELOUT)
		{
			++n;
			if (it->first > max_fd) max_fd = it->first;
			FD_SET(it->first, &write_fds);
		}
	}
	if (n == 0) goto L_RET;

	n = select(max_fd + 1, &read_fds, &write_fds, nullptr, &tv);
	if (n > 0)
	{
		for (auto it = m_fds.begin(); it != m_fds.end(); ++it)
		{
			NetConnect* conn = t->get_conn(static_cast<uint32_t>(it->first));
			if (FD_ISSET(it->first, &read_fds))
			{
				bytes_recv += t->on_read_event(conn);
			}
			if (FD_ISSET(it->first, &write_fds))
			{
				bytes_sent += t->on_write_event(conn);
			}
		}
	}
	else if (n < 0)
	{
		fprintf(stderr, "select fail:%d[%s]\n", GET_SOCK_ERR(), strerror(errno));
		n = 0;
	}

L_RET:
	t->m_ss.sample(bytes_recv, bytes_sent);
	return bytes_sent + bytes_recv;
}

} //end of namespace asyncpp

#endif
