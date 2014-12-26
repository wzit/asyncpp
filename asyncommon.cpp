#include "asyncommon.hpp"
#include <cassert>
#include <ctime>

namespace asyncpp
{

volatile time_t g_unix_timestamp = time(nullptr);

thread_pool_id_t dns_thread_pool_id;
thread_id_t dns_thread_id;

} //end of namespace asyncpp

