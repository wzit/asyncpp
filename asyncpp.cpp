#include "asyncpp.hpp"
#include <cassert>

namespace asyncpp
{

HANDLE AsyncFrame::m_iocp = INVALID_HANDLE_VALUE;

#ifdef _WIN32
AsyncFrame::AsyncFrame()
{
	if (m_iocp == INVALID_HANDLE_VALUE)
	{
		m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		assert(m_iocp != INVALID_HANDLE_VALUE);
		if (m_iocp == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "CreateIoCompletionPort fail:%d, errno:%d[%s]\n", GetLastError(), errno, strerror(errno));
		}
	}
}

AsyncFrame::~AsyncFrame()
{}

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
	//start all thread
	for(auto t_pool : m_thread_pools)
	{
		for (auto t : t_pool->get_threads())
		{
			start_thread(t);
		}
	}
	for(auto t : m_global_threads)
	{
		start_thread(t);
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

#endif

} //end of namespace asyncpp

