#include "asyncommon.hpp"
#include <cassert>
#include <ctime>

namespace asyncpp
{

volatile time_t g_unix_timestamp = time(nullptr);
#ifdef _WIN32
//convert to the windows FILETIME (as us)
volatile uint64_t g_us_tick = g_unix_timestamp * 1000 * 1000 + 11644473600000000;
#else
volatile uint64_t g_us_tick = g_unix_timestamp * 1000 * 1000;
#endif

thread_pool_id_t dns_thread_pool_id;
thread_id_t dns_thread_id;

} //end of namespace asyncpp

