#include "asyncommon.hpp"
#include <cassert>
#include <ctime>

namespace asyncpp
{

volatile time_t g_unix_timestamp = time(nullptr);

} //end of namespace asyncpp

