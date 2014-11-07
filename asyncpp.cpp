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
	t->run();
}

void AsyncFrame::start_thread(BaseThread* t)
{
	std::thread thr(thread_main, t);
	thr.detach();
}

void AsyncFrame::start()
{
	///TODO: start all thread

	for (;;)
	{
		time_t cur_time = time(NULL);
		if (cur_time != g_unix_timestamp)
		{
			g_unix_timestamp = cur_time;
		}
		///TODO:: server manager
	}
}

#endif

} //end of namespace asyncpp

