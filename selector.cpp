#include "selector.hpp"
#include "asyncpp.hpp"

#ifdef __GNUC__

#ifndef _DISABLE_SELECT

namespace asyncpp
{

void SelSelector::poll(void* pthread, uint32_t ms)
{
}

} //end of namespace asyncpp

#endif

#ifndef _DISABLE_EPOLL

namespace asyncpp
{

void EpollSelector::poll(void* pthread, uint32_t ms)
{
	int32_t ret = 0;
	struct epoll_event evs[8];
	ret = epoll_wait(m_fd, &evs, 8, ms);
	if(ret > 0)
	{
		for(int32_t i = 0; i<ret; ++i)
		{
			NetConnect* conn = reinterpret_cast<NetConnect*>(evs[i].data.ptr);
			if(evs[i].events & EPOLLIN)
			{
				conn->on_read_event();
			}
			if(evs[i].events & EPOLLOUT)
			{
				conn->on_write_event();
			}
			if(evs[i].events & EPOLLERR)
			{
				conn->on_error_event();
			}
		}
	}
	else if(ret < 0)
	{
		fprintf(stderr, "epoll_wait fail:%d[%s]\n", errno, strerror(errno));
	}
}

} //end of namespace asyncpp

#endif

#elif defined(_WIN32)

namespace asyncpp
{

HANDLE IOCPSelector::m_fd = INVALID_HANDLE_VALUE;

void IOCPSelector::poll(void* pthread, uint32_t ms)
{
}

} //end of namespace asyncpp

#else
#error os not support
#endif

