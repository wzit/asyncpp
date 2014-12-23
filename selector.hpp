#ifndef _SELECTOR_HPP_
#define _SELECTOR_HPP_

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cassert>

#ifdef _WIN32
#pragma warning(disable:4819) //for visual studio
#endif

#ifdef __GNUC__

#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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
#define closesocket(fd) close(fd)
#define GET_SOCK_ERR() errno

#ifndef _DISABLE_EPOLL

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace asyncpp
{

class EpollSelector
{
private:
	SOCKET_HANDLE m_fd;
public:
	EpollSelector()
	{
		m_fd = epoll_create(102400);
		assert(m_fd != INVALID_SOCKET);
		if(m_fd == INVALID_SOCKET)
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

	int32_t add(SOCKET_HANDLE fd)
	{
		struct epoll_event ev;
		assert(fd != INVALID_SOCKET);
		ev.events = EPOLLIN|EPOLLOUT;
		ev.data.fd = fd;
		return epoll_ctl(m_fd, EPOLL_CTL_ADD, fd, &ev);
	}
	int32_t del(SOCKET_HANDLE fd)
	{
		struct epoll_event ev;
		assert(fd != INVALID_SOCKET);
		return epoll_ctl(m_fd, EPOLL_CTL_DEL, fd, &ev);
	}
	int32_t set_read_event(SOCKET_HANDLE fd)
	{
		struct epoll_event ev;
		assert(fd != INVALID_SOCKET);
		ev.events = EPOLLIN;
		ev.data.fd = fd;
		return epoll_ctl(m_fd, EPOLL_CTL_MOD, fd, &ev);
	}
	int32_t set_write_event(SOCKET_HANDLE fd)
	{
		struct epoll_event ev;
		assert(fd != INVALID_SOCKET);
		ev.events = EPOLLOUT;
		ev.data.fd = fd;
		return epoll_ctl(m_fd, EPOLL_CTL_MOD, fd, &ev);
	}
	int32_t set_read_write_event(SOCKET_HANDLE fd)
	{
		struct epoll_event ev;
		assert(fd != INVALID_SOCKET);
		ev.events = EPOLLIN|EPOLLOUT;
		ev.data.fd = fd;
		return epoll_ctl(m_fd, EPOLL_CTL_MOD, fd, &ev);
	}

	int32_t poll(void* p_thread, uint32_t ms);
};

} //end of namespace asyncpp

#endif

#elif defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <WinBase.h>

#pragma comment(lib, "ws2_32.lib")

#define SOCKET_HANDLE SOCKET
#define	socklen_t int
#define usleep(n) Sleep((n)/1000)
#define MSG_NOSIGNAL 0

#define GET_SOCK_ERR() WSAGetLastError()

#ifndef _DISABLE_IOCP

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
	int32_t set_read_event(SOCKET_HANDLE fd){ return 0; }
	int32_t set_write_event(SOCKET_HANDLE fd){ return 0; }
	int32_t set_read_write_event(SOCKET_HANDLE fd){ return 0; }

	void poll(void* p_thread, uint32_t ms);
};

} //end of namespace asyncpp

#endif

#else
#error os not support
#endif

#ifndef _DISABLE_SELECT

namespace asyncpp
{

class SelSelector
{
private:
	//HANDLE m_fd; //not need
	enum SELEVENTS{SELIN=1,SELOUT=2};
	std::unordered_map<SOCKET_HANDLE, uint32_t> m_fds;
	std::vector<SOCKET_HANDLE> m_removed_fds;
public:
	SelSelector() = default;
	~SelSelector() = default;
	SelSelector(const SelSelector&) = delete;
	SelSelector& operator=(const SelSelector&) = delete;

	int32_t add(SOCKET_HANDLE fd)
	{
		assert(fd != INVALID_SOCKET);
		m_fds.insert(std::make_pair(fd, SELIN | SELOUT));
		return 0;
	}
	int32_t del(SOCKET_HANDLE fd)
	{
		assert(fd != INVALID_SOCKET);
		m_removed_fds.push_back(fd);
		return 0;
	}
	int32_t set_read_event(SOCKET_HANDLE fd)
	{
		assert(fd != INVALID_SOCKET);
		m_fds[fd] = SELIN;
		return 0;
	}
	int32_t set_write_event(SOCKET_HANDLE fd)
	{
		assert(fd != INVALID_SOCKET);
		m_fds[fd] = SELOUT;
		return 0;
	}
	int32_t set_read_write_event(SOCKET_HANDLE fd)
	{
		assert(fd != INVALID_SOCKET);
		m_fds[fd] = SELIN | SELOUT;
		return 0;
	}

	int32_t poll(void* p_thread, uint32_t ms);
};

}

#endif

#endif
