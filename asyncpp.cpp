#include "asyncpp.hpp"
#include <cassert>

namespace asyncpp
{

#ifdef _WIN32
HANDLE AsyncFrame::m_iocp = INVALID_HANDLE_VALUE;
#endif

AsyncFrame::AsyncFrame()
	: m_thread_pools()
	, m_ctxs()
	, m_ctxs_mtx()
	, m_end(false)
{
	g_unix_timestamp = time(nullptr);
#ifdef _WIN32
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	g_us_tick = ((uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) / 10;
#ifdef _ASYNCPP_DEBUG
	assert(g_us_tick / 1000000 - 11644473600 == g_unix_timestamp);
#endif
#else
	g_us_tick = g_unix_timestamp * 1000 * 1000;
#ifdef _ASYNCPP_DEBUG
	assert(g_us_tick / 1000000 == g_unix_timestamp);
#endif
#endif

	/* global thread pool */
	m_thread_pools.push_back(new ThreadPool(this, 0));

	/* add dns thread to global thread poll*/
	dns_thread_pool_id = static_cast<thread_pool_id_t>(0);
	dns_thread_id = add_thread<DnsThread>(dns_thread_pool_id);
#if 0
	if (m_iocp == INVALID_HANDLE_VALUE)
	{
		m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		assert(m_iocp != INVALID_HANDLE_VALUE);
		if (m_iocp == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "CreateIoCompletionPort fail:%d, errno:%d[%s]\n", GetLastError(), errno, strerror(errno));
		}
	}
#endif
}

AsyncFrame::~AsyncFrame()
{
	for (auto t : m_thread_pools)delete t;
	for (auto& it : m_ctxs)delete it.second;
}

static void thread_main(BaseThread* t)
{
	t->set_state(ThreadState::WORKING);
	t->run();
}

void AsyncFrame::start_thread(BaseThread* t)
{
	if(t->get_state() == ThreadState::INIT)
	{
		t->attach_thread(new std::thread(thread_main, t));
	}
}

void AsyncFrame::start_thread(thread_pool_id_t t_pool_id, thread_id_t t_id)
{
	start_thread(get_thread(t_pool_id, t_id));
}

void AsyncFrame::start()
{
#ifdef _WIN32
	int ret;
	WSADATA wsaData;
	if ((ret = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup failed\n");
		return;
	}
#endif

	//start all thread
	for(auto t_pool : m_thread_pools)
	{
		for (auto t : t_pool->get_threads())
		{
			start_thread(t);
		}
	}

	while (!end())
	{
#ifdef _WIN32
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		g_us_tick = ((uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) / 10;
		g_unix_timestamp = time(NULL);
#else
		struct timeval tv;
		gettimeofday(&tv, NULL);
		g_us_tick = (uint64_t)tv.tv_sec*1000*1000 + tv.tv_usec;
		g_unix_timestamp = tv.tv_sec;
#endif
		///TODO:: server manager
		usleep(10 * 1000);
	}

	usleep(100 * 1000);

	//stop all thread - block
	for (auto t_pool : m_thread_pools)
	{
		for (auto t : t_pool->get_threads())
		{
			t->waitstop();
		}
	}

#ifdef _WIN32
	WSACleanup();
#endif

	exit(0);
}

void AsyncFrame::stop()
{
	m_end = true;
}

} //end of namespace asyncpp

