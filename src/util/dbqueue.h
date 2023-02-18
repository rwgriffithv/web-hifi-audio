/*
threadsafe queue of fixed capacity
pushing blocks when queue is full
popping blocks when queue is empty

@author Robert Griffith
*/

#include <condition_variable>
#include <mutex>
#include <queue>

namespace util
{

    template <typename T>
    class DualBlockingQueue
    {
    public:
        DualBlockingQueue(size_t capacity);
        ~DualBlockingQueue();

        /*
        clear queue
        interrupt all blocking threads
        interrupted push and pop calls will return false
        */
        void flush();

        /*
        clear queue while performing op on each popped element
        interrupt all blocking threads
        interrupted push and pop calls will return false
        */
        void flush(void (*op)(T &&value));

        /*
        @param value lvalue to copy into back of queue
        @return true if successful, false if flushing or flushed while blocking
        */
        bool push(const T &value);

        /*
        @param value rvalue to move into back of queue
        @return true if successful, false if flushing or flushed while blocking
        */
        bool push(T &&value);

        /*
        @param[out] value moved from front of queue
        @return true if successful, false if flushing or flushed while blocking
        */
        bool pop(T &value);

        /*
        @return maximum size of queue
        */
        const size_t capacity();

        /*
        @return number of items in queue
        */
        const size_t size();

    private:
        size_t m_capacity;
        bool m_flush;
        size_t m_num_wait;
        std::mutex m_mtx;
        std::condition_variable m_cond;
        std::queue<T> m_queue;
    };

}