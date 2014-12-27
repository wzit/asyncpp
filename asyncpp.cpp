﻿#include "asyncpp.hpp"
#include <cassert>

namespace asyncpp
{

#ifdef _WIN32
HANDLE AsyncFrame::m_iocp = INVALID_HANDLE_VALUE;
#endif

AsyncFrame::AsyncFrame()
{
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
		std::thread thr(thread_main, t);
		thr.detach();
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

	for (;;)
	{
		time_t cur_time = time(NULL);
		if (cur_time != g_unix_timestamp)
		{
			g_unix_timestamp = cur_time;
		}
		///TODO:: server manager
		usleep(10 * 1000);
	}
}

} //end of namespace asyncpp

