#include "asyncommon.hpp"
#include <cassert>
#include <ctime>

namespace asyncpp
{

volatile time_t g_unix_timestamp = time(nullptr);
volatile uint64_t g_us_tick = g_unix_timestamp * 1000 * 1000;

thread_pool_id_t dns_thread_pool_id;
thread_id_t dns_thread_id;

} //end of namespace asyncpp

