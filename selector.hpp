#ifndef _SELECTOR_HPP_
#define _SELECTOR_HPP_

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <cassert>

#ifdef __GNUC__

#include <sys/types.h>
#include <sys/socket.h>

/*FOR SELECT*/
#ifndef _DISABLE_SELECT
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/*FOR EPOLL*/
#ifndef _DISABLE_EPOLL
#include <sys/epoll.h>
#endif

#define SOCKET_HANDLE int
#define INVALID_SOCKET  -1
#define INVALID_HANDLE_VALUE -1

namespace asyncpp
{

#ifndef _DISABLE_EPOLL
class EpollSelector
{
private:
	HANDLE m_fd;
public:
	EpollSelector()
	{
		m_fd = epoll_create(102400);
		assert(m_fd != INVALID_HANDLE_VALUE);
		if(m_fd == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "epoll_create fail:%d[%s]\n", errno, strerror(errno));
		}
	}
	~EpollSelector()
	{
		close(m_fd);
	}
	EpollSelector(const EpollSelector&) = delete;
	EpollSelector& operator=(const EpollSelector&) = delete;

	int32_t add(SOCKET_HANDLE fd, void* ctx)
	{
		struct epoll_event ev = {};
		assert(m_fd != INVALID_HANDLE_VALUE);
		return epoll_ctl(m_fd, EPOLL_CTL_ADD, fd, &ev);
	}
	/*暂时不支持*/
	int32_t del(SOCKET_HANDLE fd)
	{
		struct epoll_event ev = {};
		assert(m_fd != INVALID_HANDLE_VALUE);
		return epoll_ctl(m_fd, EPOLL_CTL_DEL, fd, &ev);
	}
	int32_t wait_read(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_write(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_read_write(SOCKET_HANDLE fd, void* ctx){}

	void poll(void* pthread, uint32_t ms);
};
#endif

} //end of namespace asyncpp

#elif defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <WinBase.h>

#pragma comment(lib, "ws2_32.lib")

#define SOCKET_HANDLE SOCKET
#define	socklen_t int
#define usleep(n) Sleep((n)/1000)

namespace asyncpp
{

class IOCPSelector
{
private:
	static HANDLE m_fd;
public:
	IOCPSelector()
	{
		m_fd = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		assert(m_fd != INVALID_HANDLE_VALUE);
		if (m_fd == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "CreateIoCompletionPort fail:%d, errno:%d[%s]\n", GetLastError(), errno, strerror(errno));
		}
	}
	~IOCPSelector()
	{
		///TODO: close all socket
		CloseHandle(m_fd);
	}
	IOCPSelector(const IOCPSelector&) = delete;
	IOCPSelector& operator=(const IOCPSelector&) = delete;

	int32_t add(SOCKET_HANDLE fd, void* ctx)
	{
		assert(m_fd != INVALID_HANDLE_VALUE);
		CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	}
	/*暂时不支持*/
	int32_t del(SOCKET_HANDLE fd)
	{
		return closesocket(fd);
	}
	int32_t wait_read(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_write(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_read_write(SOCKET_HANDLE fd, void* ctx){}

	void poll(void* pthread, uint32_t ms);
};

#ifndef _DISABLE_SELECT	
class SelSelector
{
private:
	//HANDLE m_fd; //not need
	std::vector<SOCKET_HANDLE> fd_read;
	std::vector<SOCKET_HANDLE> fd_write;
public:
	SelSelector() = default;
	~SelSelector() = default;
	SelSelector(const SelSelector&) = delete;
	SelSelector& operator=(const SelSelector&) = delete;

	int32_t add(SOCKET_HANDLE fd, void* ctx)
	{
	}
	int32_t del(SOCKET_HANDLE fd)
	{
	}
	int32_t wait_read(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_write(SOCKET_HANDLE fd, void* ctx){}
	int32_t wait_read_write(SOCKET_HANDLE fd, void* ctx){}

	void poll(void* pthread, uint32_t ms);
};
#endif

} //end of namespace asyncpp

#else
#error os not support
#endif

#endif
