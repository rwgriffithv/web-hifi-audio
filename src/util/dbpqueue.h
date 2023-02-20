/*
threadsafe dual-blocking pointer queue of fixed capacity
pushing blocks when queue is full
popping blocks when queue is empty

@author Robert Griffith
*/
#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace util
{

    template <typename T>
    class DBPQueue
    {
    public:
        DBPQueue(size_t capacity);
        ~DBPQueue();

        /*
        clear queue
        notify all waiting threads
        waiting push and pop calls will return false
        */
        void flush();

        /*
        clear queue and handle invoked on each popped element
        notify all waiting threads
        waiting push and pop calls will return false
        */
        void flush(void (*handle)(T *ptr));

        /*
        @param[out] value moved from front of queue
        @return true if successful, false if flushing or flushed while waiting
        */
        bool pop(T *&ptr);

        /*
        @param[out] value moved from front of queue
        @param timeout max duration to wait for
        @return true if successful, false if flushed or timeout reached
        */
        template <typename Rep, typename Period>
        bool pop(T *&ptr, const std::chrono::duration<Rep, Period> &timeout);

        /*
        @param ptr pointer to copy to queue
        @return true if successful, false if flushing or flushed while waiting
        */
        bool push(const T *ptr);

        /*
        @param ptr pointer to copy to queue
        @param timeout max duration to wait for
        @return true if successful, false if flushed or timeout reached
        */
        template <typename Rep, typename Period>
        bool push(const T *ptr, const std::chrono::duration<Rep, Period> &timeout);

        /*
        @return maximum size of queue (backed by two arrays of this size, worst case hold 2 * capacity)
        */
        const size_t get_capacity();

        /*
        @return number of items in queue
        */
        const size_t get_size();

    protected:
        struct Buffer
        {
            T **buf;
            size_t pos;
            size_t sz;
        };

        struct State
        {
            bool flush;
            size_t num_wait;
        };

        /*
        not thread safe
        if push buffer has data, swaps array pointers and sizes, sets push buffer size to zero
        @return true if buffers swapped, false if push buffer was empty
        */
        bool fill_pop_buffer();

        size_t _capacity;

        Buffer _pop_buf;
        State _pop_st;
        std::mutex _pop_mtx;
        std::condition_variable _pop_cond;

        Buffer _push_buf;
        State _push_st;
        std::mutex _push_mtx;
        std::condition_variable _push_cond;
    };

}