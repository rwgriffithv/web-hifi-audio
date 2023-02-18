#include <mutex>
#include <condition_variable>

namespace util
{
    struct condlock
    {
        std::mutex mx;
        std::condition_variable cv;
    };
}